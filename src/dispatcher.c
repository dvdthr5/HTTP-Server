#define _GNU_SOURCE


#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "dispatcher.h"
#include "http.h"
#include "file.h"

static int send_error(int fd, int status) {
    const char *msg;

    switch (status) {
        case 400: msg = "Bad Request"; break;
        case 403: msg = "Forbidden"; break;
        case 404: msg = "Not Found"; break;
        case 500:
        default: msg = "Internal Server Error"; break;
    }

    dprintf(fd,
        "HTTP/1.1 %d %s\r\n"
        "Content-Length: 0\r\n"
        "\r\n",
        status, msg);

    return status;
}

int handle_get(struct http_request *req, int fd) {
    uint8_t *file_data = NULL;
    size_t file_size = 0;

    int status = file_get(req->uri, &file_data, &file_size);

    if (status != 200) {
        return send_error(fd, status);
    }

    dprintf(fd,
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: %zu\r\n"
            "\r\n",
            file_size);

    if (file_size > 0 && file_data) {
        write(fd, file_data, file_size);
    }

    free(file_data);
    return 200;
}

int handle_put(struct http_request *req, int fd) {
    int status = file_put(req->uri, req->body, req->body_length);

    if (status == 200) {
        dprintf(fd,
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: 0\r\n"
                "\r\n");
        return 200;
    }

    if (status == 201) {
        dprintf(fd,
                "HTTP/1.1 201 Created\r\n"
                "Content-Length: 0\r\n"
                "\r\n");
        return 201;
    }

    if (status == 403) {
        return send_error(fd, 403);
    }

    if (status == 404) {
        return send_error(fd, 404);
    }

    if (status == 500) {
        return send_error(fd, 500);
    }

    return send_error(fd, status);
}

int dispatch_request(struct http_request *req, int fd) {
    if (strcmp(req->method, "GET") == 0) {
        return handle_get(req, fd);
    }

    if (strcmp(req->method, "PUT") == 0) {
        return handle_put(req, fd);
    }

    return send_error(fd, 501);
}
