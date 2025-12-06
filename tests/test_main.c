#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/server.h"
#include "../src/parallel.h"

extern int http_main(int argc, char **argv);

static int log_init_calls = 0;
static int log_close_calls = 0;
static int run_server_calls = 0;
static int parallel_init_calls = 0;
static int parallel_shutdown_calls = 0;
static size_t last_parallel_threads = 0;
static int handler_set_count = 0;

static int last_port = -1;
static char last_dir[256];
static int mock_log_init_rc = 0;
static int mock_run_server_rc = 0;
static int mock_parallel_init_rc = 0;

int log_init(void) {
    log_init_calls++;
    if (mock_log_init_rc != 0) {
        return -1;
    }
    return 0;
}

void log_close(void) {
    log_close_calls++;
}

int run_server(int port, const char *data_dir) {
    run_server_calls++;
    last_port = port;
    snprintf(last_dir, sizeof(last_dir), "%s", data_dir ? data_dir : "");
    return mock_run_server_rc;
}

int parallel_init(size_t num_threads) {
    parallel_init_calls++;
    last_parallel_threads = num_threads;
    if (mock_parallel_init_rc != 0) {
        errno = mock_parallel_init_rc;
        return -1;
    }
    return 0;
}

void parallel_shutdown(void) {
    parallel_shutdown_calls++;
}

int parallel_connection_handler(int client_fd, void *context) {
    (void)client_fd;
    (void)context;
    return SERVER_CONNECTION_CONTINUE;
}

void server_set_connection_handler(server_connection_handler handler, void *context) {
    (void)context;
    if (handler) {
        handler_set_count++;
    } else {
        handler_set_count--;
    }
}

static int invoke_main(const char *cmdline) {
    char *copy = strdup(cmdline);
    assert(copy);

    char *argv[16];
    int argc = 0;

    char *token = strtok(copy, " ");
    while (token != NULL && argc < 16) {
        argv[argc++] = token;
        token = strtok(NULL, " ");
    }

    argv[argc] = NULL;
    int rc = http_main(argc, argv);
    free(copy);
    return rc;
}

static void reset_state(void) {
    log_init_calls = 0;
    log_close_calls = 0;
    run_server_calls = 0;
    parallel_init_calls = 0;
    parallel_shutdown_calls = 0;
    handler_set_count = 0;
    last_parallel_threads = 0;
    last_port = -1;
    last_dir[0] = '\0';
    mock_log_init_rc = 0;
    mock_run_server_rc = 0;
    mock_parallel_init_rc = 0;
}

static void test_main_valid(void) {
    reset_state();
    int rc = invoke_main("prog 8080 tests/tmp");
    assert(rc == EXIT_SUCCESS);
    assert(log_init_calls == 1);
    assert(log_close_calls == 1);
    assert(run_server_calls == 1);
    assert(last_port == 8080);
    assert(strcmp(last_dir, "tests/tmp") == 0);
    assert(parallel_init_calls == 0);
    assert(parallel_shutdown_calls == 0);
}

static void test_main_invalid_port(void) {
    reset_state();
    int rc = invoke_main("prog notaport tests/tmp");
    assert(rc == EXIT_FAILURE);
    assert(run_server_calls == 0);
}

static void test_main_missing_args(void) {
    reset_state();
    int rc = invoke_main("prog 8080");
    assert(rc == EXIT_SUCCESS);
    assert(run_server_calls == 1);
    assert(strcmp(last_dir, ".") == 0);
}

static void test_main_parallel_flag(void) {
    reset_state();
    int rc = invoke_main("prog -p 4 9090 data");
    assert(rc == EXIT_SUCCESS);
    assert(run_server_calls == 1);
    assert(parallel_init_calls == 1);
    assert(parallel_shutdown_calls == 1);
    assert(last_parallel_threads == 4);
    assert(handler_set_count == 0);
}

int main(void) {
    test_main_valid();
    test_main_invalid_port();
    test_main_missing_args();
    test_main_parallel_flag();
    printf("test_main: OK\n");
    return 0;
}
