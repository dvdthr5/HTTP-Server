#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "parallel.h"
#include "server.h"

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s [-p threads] <port> <data-dir>\n", prog);
}

static int parse_port(const char *value) {
    char *end = NULL;
    long port = strtol(value, &end, 10);
    if (end == value || *end != '\0' || port <= 0 || port > 65535) {
        return -1;
    }
    return (int)port;
}

int http_main(int argc, char **argv) {
    int opt;
    int parallel_workers = 0;
    int parallel_active = 0;

    while ((opt = getopt(argc, argv, "p:")) != -1) {
        switch (opt) {
            case 'p':
                parallel_workers = atoi(optarg);
                break;
            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    if (argc - optind != 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    int port = parse_port(argv[optind]);
    if (port < 0) {
        fprintf(stderr, "Invalid port: %s\n", argv[optind]);
        return EXIT_FAILURE;
    }

    const char *data_dir = argv[optind + 1];

    if (log_init() != 0) {
        perror("log_init");
        return EXIT_FAILURE;
    }

    if (parallel_workers > 0) {
        if (parallel_init((size_t)parallel_workers) != 0) {
            perror("parallel_init");
            log_close();
            return EXIT_FAILURE;
        }
        server_set_connection_handler(parallel_connection_handler, NULL);
        parallel_active = 1;
    }

    int rc = run_server(port, data_dir);
    if (parallel_active) {
        parallel_shutdown();
        server_set_connection_handler(NULL, NULL);
    }
    log_close();

    if (rc != 0) {
        fprintf(stderr, "Server exited with code %d\n", rc);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

#ifndef UNIT_TEST
int main(int argc, char **argv) {
    return http_main(argc, argv);
}
#endif
