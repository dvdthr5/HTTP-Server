#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <stdint.h>
#include "../src/http.h"

static int make_pipe_with_input(const char *input) {
    int fds[2];
    if (pipe(fds) < 0) {
        perror("pipe");
        return -1;
    }
    write(fds[1], input, strlen(input));
    close(fds[1]);
    return fds[0];
}

void test_valid_get(void) {
    const char *req =
        "GET /foo.txt HTTP/1.1\r\n"
        "\r\n";

    int fd = make_pipe_with_input(req);
    struct http_request r;

    int st = parse_request(fd, &r);
    assert(st == 0);
    assert(strcmp(r.method, "GET") == 0);
    assert(strcmp(r.uri, "/foo.txt") == 0);
    assert(strcmp(r.version, "HTTP/1.1") == 0);
    assert(r.content_length == -1);

    close(fd);
    printf("test_valid_get: OK\n");
}

void test_invalid_method(void) {
    const char *req =
        "POST /foo HTTP/1.1\r\n"
        "\r\n";

    int fd = make_pipe_with_input(req);
    struct http_request r;

    int st = parse_request(fd, &r);
    assert(st == 501);

    close(fd);
    printf("test_invalid_method: OK\n");
}

void test_invalid_version(void) {
    const char *req =
        "GET /foo HTTP/2.0\r\n"
        "\r\n";

    int fd = make_pipe_with_input(req);
    struct http_request r;

    int st = parse_request(fd, &r);
    assert(st == 505);

    close(fd);
    printf("test_invalid_version: OK\n");
}

void test_invalid_uri(void) {
    const char *req =
        "GET /../secret HTTP/1.1\r\n"
        "\r\n";

    int fd = make_pipe_with_input(req);
    struct http_request r;

    int st = parse_request(fd, &r);
    assert(st == 400);

    close(fd);
    printf("test_invalid_uri: OK\n");
}

void test_put_missing_content_length(void) {
    const char *req =
        "PUT /foo HTTP/1.1\r\n"
        "\r\n";

    int fd = make_pipe_with_input(req);
    struct http_request r;

    int st = parse_request(fd, &r);
    assert(st == 400);

    close(fd);
    printf("test_put_missing_content_length: OK\n");
}

void test_put_with_body(void) {
    const char *req =
        "PUT /file HTTP/1.1\r\n"
        "Content-Length: 4\r\n"
        "\r\n"
        "DATA";

    int fd = make_pipe_with_input(req);
    struct http_request r;

    int st = parse_request(fd, &r);
    assert(st == 0);
    assert(r.content_length == 4);

    assert(r.body_length == 4);
    assert(memcmp(r.body, "DATA", 4) == 0);

    close(fd);
    printf("test_put_with_body: OK\n");
}

void test_put_short_body(void) {
    const char *req =
        "PUT /file HTTP/1.1\r\n"
        "Content-Length: 10\r\n"
        "\r\n"
        "12345";

    int fd = make_pipe_with_input(req);
    struct http_request r;

    int st = parse_request(fd, &r);
    assert(st == 400);
    close(fd);
    printf("test_put_short_body: OK\n");
}

void test_request_id_default(void) {
    const char *req =
        "GET /foo.txt HTTP/1.1\r\n"
        "\r\n";

    int fd = make_pipe_with_input(req);
    struct http_request r;

    int st = parse_request(fd, &r);
    assert(st == 0);
    assert(r.request_id == 0);

    close(fd);
    printf("test_request_id_default: OK\n");
}

void test_request_id_valid(void) {
    const char *req =
        "GET /foo.txt HTTP/1.1\r\n"
        "Request-ID: 42\r\n"
        "\r\n";

    int fd = make_pipe_with_input(req);
    struct http_request r;

    int st = parse_request(fd, &r);
    assert(st == 0);
    assert(r.request_id == 42);

    close(fd);
    printf("test_request_id_valid: OK\n");
}

void test_request_id_invalid(void) {
    const char *req =
        "GET /foo.txt HTTP/1.1\r\n"
        "Request-ID: abc\r\n"
        "\r\n";

    int fd = make_pipe_with_input(req);
    struct http_request r;

    int st = parse_request(fd, &r);
    assert(st == 400);

    close(fd);
    printf("test_request_id_invalid: OK\n");
}

int main(void) {
    test_valid_get();
    test_invalid_method();
    test_invalid_version();
    test_invalid_uri();
    test_put_missing_content_length();
    test_put_with_body();
    test_put_short_body();
    test_request_id_default();
    test_request_id_valid();
    test_request_id_invalid();
    return 0;
}
