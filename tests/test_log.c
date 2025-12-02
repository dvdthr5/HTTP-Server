#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../src/log.h"

static void capture_stderr(char *buffer, size_t buf_size, void (*fn)(void *), void *ctx) {
    int pipefd[2];
    assert(pipe(pipefd) == 0);

    int saved = dup(STDERR_FILENO);
    assert(saved >= 0);
    assert(dup2(pipefd[1], STDERR_FILENO) >= 0);
    close(pipefd[1]);

    fn(ctx);

    fflush(stderr);
    assert(dup2(saved, STDERR_FILENO) >= 0);
    close(saved);

    ssize_t n = read(pipefd[0], buffer, buf_size - 1);
    assert(n >= 0);
    buffer[n] = '\0';
    close(pipefd[0]);
}

static void log_once(void *ctx) {
    (void)ctx;
    struct http_request req = {0};
    strcpy(req.method, "GET");
    strcpy(req.uri, "/index.html");
    req.request_id = 42;

    assert(log_init() == 0);
    log_request(&req, 200);
    log_close();
}

int main(void) {
    char buf[256];
    capture_stderr(buf, sizeof(buf), log_once, NULL);
    assert(strstr(buf, "GET"));
    assert(strstr(buf, "/index.html"));
    assert(strstr(buf, "200"));
    assert(strstr(buf, "42"));

    // log_close should be idempotent
    log_close();
    printf("test_log_basic: OK\n");
    return 0;
}
