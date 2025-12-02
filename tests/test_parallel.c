#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "../src/parallel.h"
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
    system("rm -rf " TMPDIR "*");
}

static void make_socket_pair(int fds[2]) {
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0) {
        perror("socketpair");
        exit(1);
    }
}

static void write_all(int fd, const char *data) {
    size_t len = strlen(data);
    size_t off = 0;
    while (off < len) {
        ssize_t n = write(fd, data + off, len - off);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("write");
            exit(1);
        }
        off += (size_t)n;
    }
}

static char *read_all(int fd) {
    static char buf[8192];
    memset(buf, 0, sizeof(buf));
    size_t off = 0;
    for (;;) {
        if (off >= sizeof(buf) - 1) break;
        ssize_t n = read(fd, buf + off, sizeof(buf) - 1 - off);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("read");
            exit(1);
        }
        if (n == 0) break;
        off += (size_t)n;
    }
    buf[off] = '\0';
    return buf;
}

// Capture all stderr output produced while fn() runs.
// Returns malloc'd string with the captured data.
static char *capture_stderr(void (*fn)(void *), void *arg) {
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        perror("pipe");
        exit(1);
    }

    int saved_stderr = dup(STDERR_FILENO);
    if (saved_stderr < 0) {
        perror("dup");
        exit(1);
    }

    if (dup2(pipefd[1], STDERR_FILENO) < 0) {
        perror("dup2");
        exit(1);
    }
    close(pipefd[1]);

    fn(arg);

    fflush(stderr);
    if (dup2(saved_stderr, STDERR_FILENO) < 0) {
        perror("dup2 restore");
        exit(1);
    }
    close(saved_stderr);

    char tmp[8192];
    memset(tmp, 0, sizeof(tmp));
    ssize_t n = read(pipefd[0], tmp, sizeof(tmp) - 1);
    if (n < 0) {
        perror("read log pipe");
        exit(1);
    }
    close(pipefd[0]);

    char *out = malloc((size_t)n + 1);
    if (!out) {
        perror("malloc");
        exit(1);
    }
    memcpy(out, tmp, (size_t)n);
    out[n] = '\0';
    return out;
}

// -----------------------
// Test 1: Property 1
// Sequential R1 then R2
// If R2 starts after R1 finishes, it must appear
// after R1 in the audit log.
// -----------------------

static void run_test_property1_body(void *arg) {
    (void)arg;

    ensure_tmpdir();
    clean_tmpdir();

    // R1: GET foo, Request-ID 1
    int fds1[2];
    make_socket_pair(fds1);
    int server_fd1 = fds1[0];
    int client_fd1 = fds1[1];

    const char *req1 =
        "GET foo HTTP/1.1\r\n"
        "Request-ID: 1\r\n"
        "\r\n";

    parallel_connection_handler(server_fd1, NULL);
    write_all(client_fd1, req1);
    shutdown(client_fd1, SHUT_WR);
    (void)read_all(client_fd1); // drain response
    close(client_fd1);

    // Only now start R2.
    int fds2[2];
    make_socket_pair(fds2);
    int server_fd2 = fds2[0];
    int client_fd2 = fds2[1];

    const char *req2 =
        "GET foo HTTP/1.1\r\n"
        "Request-ID: 2\r\n"
        "\r\n";

    parallel_connection_handler(server_fd2, NULL);
    write_all(client_fd2, req2);
    shutdown(client_fd2, SHUT_WR);
    (void)read_all(client_fd2);
    close(client_fd2);
}

static void test_property1_sequential_log_order(void) {
    char *logs = capture_stderr(run_test_property1_body, NULL);

    // Expect two lines of the form: METHOD,URI,STATUS,REQUESTID\n
    // e.g., "GET,foo,404,1\nGET,foo,404,2\n"
    int seen1 = 0, seen2 = 0;
    int first_id = -1, second_id = -1;

    char *saveptr = NULL;
    char *line = strtok_r(logs, "\n", &saveptr);
    int idx = 0;
    while (line) {
        int rid = -1;
        // Parse last field as request_id
        char *last_comma = strrchr(line, ',');
        if (last_comma) {
            rid = atoi(last_comma + 1);
        }

        if (idx == 0) {
            first_id = rid;
        } else if (idx == 1) {
            second_id = rid;
        }

        if (rid == 1) seen1++;
        if (rid == 2) seen2++;

        idx++;
        line = strtok_r(NULL, "\n", &saveptr);
    }

    assert(seen1 == 1);
    assert(seen2 == 1);
    assert(first_id == 1);
    assert(second_id == 2);

    free(logs);
    printf("test_property1_sequential_log_order: OK\n");
}

// -----------------------
// Test 2: Property 2
// If R2 appears after R1 in the log,
// then R2 must observe effects of R1.
// We'll use PUT then GET on same URI.
// -----------------------

static void run_test_property2_body(void *arg) {
    (void)arg;

    ensure_tmpdir();
    clean_tmpdir();

    // R1: PUT file with body "DATA"
    int fds1[2];
    make_socket_pair(fds1);
    int server_fd1 = fds1[0];
    int client_fd1 = fds1[1];

    const char *req1 =
        "PUT file HTTP/1.1\r\n"
        "Content-Length: 4\r\n"
        "Request-ID: 10\r\n"
        "\r\n"
        "DATA";

    parallel_connection_handler(server_fd1, NULL);
    write_all(client_fd1, req1);
    shutdown(client_fd1, SHUT_WR);
    (void)read_all(client_fd1);
    close(client_fd1);

    // R2: GET file, should see "DATA"
    int fds2[2];
    make_socket_pair(fds2);
    int server_fd2 = fds2[0];
    int client_fd2 = fds2[1];

    const char *req2 =
        "GET file HTTP/1.1\r\n"
        "Request-ID: 11\r\n"
        "\r\n";

    parallel_connection_handler(server_fd2, NULL);
    write_all(client_fd2, req2);
    shutdown(client_fd2, SHUT_WR);
    char *resp2 = read_all(client_fd2);
    close(client_fd2);

    // Verify that GET saw DATA
    assert(strstr(resp2, "HTTP/1.1 200 OK"));
    assert(strstr(resp2, "DATA"));
}

static void test_property2_visibility(void) {
    char *logs = capture_stderr(run_test_property2_body, NULL);

    // We expect PUT (10) and GET (11), and GET must appear after PUT.
    int first_id = -1, second_id = -1;

    char *saveptr = NULL;
    char *line = strtok_r(logs, "\n", &saveptr);
    int idx = 0;
    while (line) {
        int rid = -1;
        char *last_comma = strrchr(line, ',');
        if (last_comma) {
            rid = atoi(last_comma + 1);
        }

        if (idx == 0) first_id = rid;
        else if (idx == 1) second_id = rid;

        idx++;
        line = strtok_r(NULL, "\n", &saveptr);
    }

    // PUT should be 10, GET 11, and GET must be logged after.
    assert(first_id == 10);
    assert(second_id == 11);

    free(logs);
    printf("test_property2_visibility: OK\n");
}

// -----------------------
// Test 3: Parallel stress
// Fire many concurrent requests and make sure
// logs aren't corrupted and all requests processed.
// -----------------------

typedef struct {
    int client_fd;
    int request_id;
} client_task_t;

static void *client_thread_main(void *arg) {
    client_task_t *task = (client_task_t *)arg;

    char req[256];
    snprintf(req, sizeof(req),
             "GET ping%d HTTP/1.1\r\n"
             "Request-ID: %d\r\n"
             "\r\n",
             task->request_id,
             task->request_id);

    write_all(task->client_fd, req);
    shutdown(task->client_fd, SHUT_WR);
    (void)read_all(task->client_fd);
    close(task->client_fd);

    return NULL;
}

static void run_test_parallel_stress_body(void *arg) {
    (void)arg;

    ensure_tmpdir();
    clean_tmpdir();

    const int N = 10;
    pthread_t threads[N];
    client_task_t tasks[N];

    for (int i = 0; i < N; i++) {
        int fds[2];
        make_socket_pair(fds);
        int server_fd = fds[0];
        int client_fd = fds[1];

        tasks[i].client_fd = client_fd;
        tasks[i].request_id = i + 100;

        parallel_connection_handler(server_fd, NULL);

        int rc = pthread_create(&threads[i], NULL, client_thread_main, &tasks[i]);
        assert(rc == 0);
    }

    for (int i = 0; i < N; i++) {
        pthread_join(threads[i], NULL);
    }
}

static void test_parallel_stress(void) {
    char *logs = capture_stderr(run_test_parallel_stress_body, NULL);

    // We expect 10 log lines with request_ids 100..109
    int seen[10] = {0};

    char *saveptr = NULL;
    char *line = strtok_r(logs, "\n", &saveptr);
    int count = 0;
    while (line) {
        int rid = -1;
        char *last_comma = strrchr(line, ',');
        if (last_comma) {
            rid = atoi(last_comma + 1);
        }

        if (rid >= 100 && rid < 110) {
            seen[rid - 100]++;
        }

        count++;
        line = strtok_r(NULL, "\n", &saveptr);
    }

    assert(count == 10);
    for (int i = 0; i < 10; i++) {
        assert(seen[i] == 1);
    }

    free(logs);
    printf("test_parallel_stress: OK\n");
}

// -----------------------
// main()
// -----------------------

int main(void) {
    ensure_tmpdir();
    setenv("TEST_TMPDIR", TMPDIR, 1);

    assert(log_init() == 0);
    assert(parallel_init(4) == 0);

    test_property1_sequential_log_order();
    test_property2_visibility();
    test_parallel_stress();

    parallel_shutdown();
    log_close();
    return 0;
}
