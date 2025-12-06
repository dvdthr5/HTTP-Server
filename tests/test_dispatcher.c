#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

#include "../src/dispatcher.h"
#include "../src/http.h"
#include "../src/file.h"

#define TMPDIR "tests/tmp/"

//
// Helpers
//

static void clean_tmpdir(void) {
    // Delete all files inside tests/tmp/
    system("rm -rf tests/tmp/*");
}

static void ensure_tmpdir(void) {
    mkdir(TMPDIR, 0777);
}

static int make_pipe(int fds[2]) {
    if (pipe(fds) < 0) {
        perror("pipe");
        exit(1);
    }
    return 0;
}

static char *read_fd(int fd) {
    static char buf[4096];
    memset(buf, 0, sizeof(buf));
    read(fd, buf, sizeof(buf));
    return buf;
}

//
// Test cases
//

void test_get_ok(void) {
    ensure_tmpdir();
    clean_tmpdir();

    // Create real file: tests/tmp/exists
    FILE *f = fopen("tests/tmp/exists", "wb");
    assert(f);
    fwrite("HELLO", 1, 5, f);
    fclose(f);

    int fds[2];
    make_pipe(fds);

    struct http_request req = {0};
    strcpy(req.method, "GET");
    strcpy(req.uri, "/exists");

    dispatch_request(&req, fds[1]);

    char *resp = read_fd(fds[0]);
    assert(strstr(resp, "200"));
    assert(strstr(resp, "HELLO"));

    close(fds[0]);
    close(fds[1]);
    printf("test_get_ok: OK\n");
}


void test_get_404(void) {
    ensure_tmpdir();
    clean_tmpdir();

    int fds[2];
    make_pipe(fds);

    struct http_request req = {0};
    strcpy(req.method, "GET");
    strcpy(req.uri, "/missing");

    dispatch_request(&req, fds[1]);

    char *resp = read_fd(fds[0]);
    assert(strstr(resp, "404"));

    close(fds[0]);
    close(fds[1]);
    printf("test_get_404: OK\n");
}


void test_put_ok(void) {
    ensure_tmpdir();
    clean_tmpdir();

    int fds[2];
    make_pipe(fds);

    struct http_request req = {0};
    strcpy(req.method, "PUT");
    strcpy(req.uri, "/file");
    req.body = (uint8_t *)"DATA";
    req.body_length = 4;

    dispatch_request(&req, fds[1]);

    char *resp = read_fd(fds[0]);
    assert(strstr(resp, "201"));

    // Verify file was written
    FILE *f = fopen("tests/tmp/file", "rb");
    assert(f);

    char buf[10] = {0};
    fread(buf, 1, 4, f);
    assert(memcmp(buf, "DATA", 4) == 0);

    fclose(f);
    close(fds[0]);
    close(fds[1]);
    printf("test_put_ok: OK\n");
}


void test_put_403(void) {
    ensure_tmpdir();
    clean_tmpdir();

    // Create a readonly file
    FILE *f = fopen("tests/tmp/readonly", "wb");
    assert(f);
    fwrite("XXX", 1, 3, f);
    fclose(f);

    chmod("tests/tmp/readonly", 0444); // readonly

    int fds[2];
    make_pipe(fds);

    struct http_request req = {0};
    strcpy(req.method, "PUT");
    strcpy(req.uri, "/readonly");
    req.body = (uint8_t *)"DATA";
    req.body_length = 4;

    dispatch_request(&req, fds[1]);

    char *resp = read_fd(fds[0]);
    assert(strstr(resp, "403"));

    close(fds[0]);
    close(fds[1]);
    printf("test_put_403: OK\n");
}


int main(void) {
    setenv("TEST_TMPDIR", TMPDIR, 1);
    test_get_ok();
    test_get_404();
    test_put_ok();
    test_put_403();
    return 0;
}
