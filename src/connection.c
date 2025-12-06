#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "connection.h"
#include "dispatcher.h"
#include "http.h"
#include "log.h"
#include "server.h"

static const char *status_phrase(int status) {
    switch (status) {
    case 200: return "OK";
    case 201: return "Created";
    case 400: return "Bad Request";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    case 505: return "Version Not Supported";
    default: return "Internal Server Error";
    }
}

static void send_simple_error(int fd, int status) {
    const char *phrase = status_phrase(status);
    char body[64];
    int n = snprintf(body, sizeof(body), "%s\n", phrase);
    if (n < 0) {
        return;
    }
    dprintf(fd,
        "HTTP/1.1 %d %s\r\n"
        "Content-Length: %d\r\n"
        "\r\n",
        status, phrase, n);
    write(fd, body, (size_t) n);
}

int handle_connection(int client_fd) {
    struct http_request req;
    memset(&req, 0, sizeof(req));

    int status = parse_request(client_fd, &req);
    if (status != 0) {
        send_simple_error(client_fd, status);
    } else {
        status = dispatch_request(&req, client_fd);
    }

    log_request(&req, status);

    free(req.body);
    close(client_fd);
    return SERVER_CONNECTION_CONTINUE;
}
