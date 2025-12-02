#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "../src/connection.h"
#include "../src/http.h"
#include "../src/dispatcher.h"
#include "../src/file.h"
#include "../src/log.h"

#define TMPDIR "tests/tmp/"

static void ensure_tmpdir(void) {
    mkdir(TMPDIR, 0777);
}

static void clean_tmpdir(void) {
    system("rm -rf tests/tmp/*");
}

static void prepare_tmpdir(void) {
    ensure_tmpdir();
    clean_tmpdir();
    setenv("TEST_TMPDIR", TMPDIR, 1);
}

// Full-duplex pair for server<->client
static void make_socket_pair(int fds[2]) {
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0) {
        perror("socketpair");
        exit(1);
    }
}

// Read whole response (small) into static buffer
static char *read_all(int fd) {
    static char buf[4096];
    memset(buf, 0, sizeof(buf));
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n < 0) {
        perror("read");
        exit(1);
    }
    return buf;
}

// Capture a single log line written to stderr by handle_connection
static char *capture_log_line(void (*fn)(int), int server_fd) {
    int log_fds[2];
    if (pipe(log_fds) < 0) {
        perror("pipe");
        exit(1);
    }

    int saved_stderr = dup(STDERR_FILENO);
    if (saved_stderr < 0) {
        perror("dup");
        exit(1);
    }

    if (dup2(log_fds[1], STDERR_FILENO) < 0) {
        perror("dup2");
        exit(1);
    }
    close(log_fds[1]); // now stderr points to log_fds[1]

    fn(server_fd);     // run handle_connection

    fflush(stderr);
    if (dup2(saved_stderr, STDERR_FILENO) < 0) {
        perror("dup2 restore");
        exit(1);
    }
    close(saved_stderr);

    static char log_buf[4096];
    memset(log_buf, 0, sizeof(log_buf));
    ssize_t n = read(log_fds[0], log_buf, sizeof(log_buf) - 1);
    if (n < 0) {
        perror("read log pipe");
        exit(1);
    }
    close(log_fds[0]);

    return log_buf;
}

// -----------------------
// Tests
// -----------------------

void test_connection_get_ok() {
    prepare_tmpdir();
    log_init();

    // Create file tests/tmp/exists with content "HELLO"
    FILE *f = fopen("tests/tmp/exists", "wb");
    assert(f);
    fwrite("HELLO", 1, 5, f);
    fclose(f);

    int fds[2];
    make_socket_pair(fds);

    int server_fd = fds[0];
    int client_fd = fds[1];

    // Send full HTTP GET request with Request-ID
    const char *req =
        "GET exists HTTP/1.1\r\n"
        "Request-ID: 42\r\n"
        "\r\n";

    write(client_fd, req, strlen(req));
    shutdown(client_fd, SHUT_WR);  // done sending request

    // Capture log while running handle_connection
    char *log_line = capture_log_line(handle_connection, server_fd);

    // Read server's response from client side
    char *resp = read_all(client_fd);
    close(client_fd);

    // Response checks
    assert(strstr(resp, "HTTP/1.1 200 OK"));
    assert(strstr(resp, "Content-Length: 5"));
    assert(strstr(resp, "HELLO"));

    // Log checks: "GET,exists,200,42\n"
    assert(strstr(log_line, "GET,exists,200,42"));

    log_close();
    printf("test_connection_get_ok: OK\n");
}

void test_connection_invalid_method() {
    prepare_tmpdir();
    log_init();

    int fds[2];
    make_socket_pair(fds);

    int server_fd = fds[0];
    int client_fd = fds[1];

    const char *req =
        "POST foo HTTP/1.1\r\n"
        "\r\n";

    write(client_fd, req, strlen(req));
    shutdown(client_fd, SHUT_WR);

    char *log_line = capture_log_line(handle_connection, server_fd);
    char *resp = read_all(client_fd);
    close(client_fd);

    // Response should be 501 Not Implemented with zero-length body
    assert(strstr(resp, "HTTP/1.1 501 Not Implemented"));
    assert(strstr(resp, "Content-Length: 0"));

    // Log line should at least contain ",501,"
    assert(strstr(log_line, ",501,"));

    log_close();
    printf("test_connection_invalid_method: OK\n");
}

void test_connection_put_ok() {
    prepare_tmpdir();
    log_init();

    int fds[2];
    make_socket_pair(fds);

    int server_fd = fds[0];
    int client_fd = fds[1];

    const char *req =
        "PUT newfile HTTP/1.1\r\n"
        "Content-Length: 4\r\n"
        "Request-ID: 7\r\n"
        "\r\n"
        "DATA";

    write(client_fd, req, strlen(req));
    shutdown(client_fd, SHUT_WR);

    char *log_line = capture_log_line(handle_connection, server_fd);
    char *resp = read_all(client_fd);
    close(client_fd);

    // First PUT to a non-existent file → 201 Created
    assert(strstr(resp, "HTTP/1.1 201 Created"));
    assert(strstr(resp, "Content-Length: 0"));

    // File contents
    FILE *f = fopen("tests/tmp/newfile", "rb");
    assert(f);
    char buf[8] = {0};
    fread(buf, 1, 4, f);
    fclose(f);
    assert(memcmp(buf, "DATA", 4) == 0);

    // Log line: "...200 or 201..." but dispatcher returns 201, so:
    assert(strstr(log_line, "PUT,newfile,201,7"));

    log_close();
    printf("test_connection_put_ok: OK\n");
}

int main() {
    test_connection_get_ok();
    test_connection_invalid_method();
    test_connection_put_ok();
    return 0;
}
