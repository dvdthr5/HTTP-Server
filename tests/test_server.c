#define _GNU_SOURCE
#include <arpa/inet.h>
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

#include "../src/server.h"

#define TMP_DIR "tests/tmp/server"

static volatile int handler_calls = 0;

int handle_connection(int client_fd) {
    (void)client_fd;
    return SERVER_CONNECTION_CONTINUE;
}

static int test_handler(int client_fd, void *ctx) {
    (void)ctx;
    handler_calls++;

    char buf[16] = {0};
    read(client_fd, buf, sizeof(buf));
    return SERVER_CONNECTION_SHUTDOWN;
}

struct server_args {
    int port;
    const char *dir;
};

static void *server_thread(void *arg) {
    struct server_args *cfg = arg;
    int rc = run_server(cfg->port, cfg->dir);
    return (void *)(intptr_t)rc;
}

static int find_available_port(void) {
    int last_err = 0;
    for (int port = 20000; port < 21000; port++) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            continue;
        }

        int opt = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
#ifdef __APPLE__
        addr.sin_len = sizeof(addr);
#endif
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
            close(fd);
            return port;
        }
        last_err = errno;
        close(fd);

        if (last_err == EPERM || last_err == EACCES) {
            return -1;
        }
    }

    if (last_err == EPERM || last_err == EACCES) {
        return -1;
    }

    return -2;
}

static int connect_with_retry(int port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
#ifdef __APPLE__
    addr.sin_len = sizeof(addr);
#endif
    addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    for (int attempt = 0; attempt < 50; attempt++) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        assert(fd >= 0);
        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
            return fd;
        }
        close(fd);
        usleep(20000);
    }

    fprintf(stderr, "Failed to connect to server on port %d\n", port);
    exit(1);
}

int main(void) {
    mkdir("tests/tmp", 0777);
    mkdir(TMP_DIR, 0777);

    server_set_connection_handler(test_handler, NULL);
    int port = find_available_port();
    if (port < 0) {
        if (port == -1) {
            printf("test_server_basic: SKIP (no permission to bind sockets)\n");
            return 0;
        }
        fprintf(stderr, "Unable to locate an open port\n");
        return 1;
    }

    struct server_args args = {.port = port, .dir = TMP_DIR};
    pthread_t tid;
    assert(pthread_create(&tid, NULL, server_thread, &args) == 0);

    int client_fd = connect_with_retry(port);
    write(client_fd, "ping", 4);
    close(client_fd);

    void *thread_result = NULL;
    assert(pthread_join(tid, &thread_result) == 0);
    int server_rc = (int)(intptr_t)thread_result;
    assert(server_rc == 0);
    assert(handler_calls == 1);

    printf("test_server_basic: OK\n");
    return 0;
}
