#define _GNU_SOURCE

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "dispatcher.h"
#include "http.h"
#include "file.h"
#include <pthread.h>

// Simple per-URI mutex map to serialize conflicting file accesses.
typedef struct file_lock {
    char *uri;
    pthread_mutex_t mutex;
    struct file_lock *next;
} file_lock_t;

static pthread_mutex_t lock_map_mutex = PTHREAD_MUTEX_INITIALIZER;
static file_lock_t *lock_map_head = NULL;

static pthread_mutex_t *get_lock_for_uri(const char *uri) {
    pthread_mutex_lock(&lock_map_mutex);
    file_lock_t *cur = lock_map_head;
    while (cur) {
        if (strcmp(cur->uri, uri) == 0) {
            pthread_mutex_unlock(&lock_map_mutex);
            return &cur->mutex;
        }
        cur = cur->next;
    }

    file_lock_t *node = calloc(1, sizeof(*node));
    if (!node) {
        pthread_mutex_unlock(&lock_map_mutex);
        return NULL;
    }
    node->uri = strdup(uri);
    if (!node->uri) {
        free(node);
        pthread_mutex_unlock(&lock_map_mutex);
        return NULL;
    }
    pthread_mutex_init(&node->mutex, NULL);
    node->next = lock_map_head;
    lock_map_head = node;
    pthread_mutex_unlock(&lock_map_mutex);
    return &node->mutex;
}

static int send_error(int fd, int status) {
    const char *phrase = NULL;
    const char *msg = NULL;
    switch (status) {
    case 400:
        phrase = "Bad Request";
        msg = "Bad Request\n";
        break;
    case 403:
        phrase = "Forbidden";
        msg = "Forbidden\n";
        break;
    case 404:
        phrase = "Not Found";
        msg = "Not Found\n";
        break;
    case 500:
        phrase = "Internal Server Error";
        msg = "Internal Server Error\n";
        break;
    case 501:
        phrase = "Not Implemented";
        msg = "Not Implemented\n";
        break;
    case 505:
        phrase = "Version Not Supported";
        msg = "Version Not Supported\n";
        break;
    default:
        phrase = "Internal Server Error";
        msg = "Internal Server Error\n";
        break;
    }

    size_t len = strlen(msg);
    dprintf(fd,
        "HTTP/1.1 %d %s\r\n"
        "Content-Length: %zu\r\n"
        "\r\n",
        status, phrase, len);
    write(fd, msg, len);

    return status;
}

int handle_get(struct http_request *req, int fd) {
    uint8_t *file_data = NULL;
    size_t file_size = 0;

    pthread_mutex_t *mtx = get_lock_for_uri(req->uri);
    if (!mtx) {
        return send_error(fd, 500);
    }
    pthread_mutex_lock(mtx);
    int status = file_get(req->uri, &file_data, &file_size);
    pthread_mutex_unlock(mtx);

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
    pthread_mutex_t *mtx = get_lock_for_uri(req->uri);
    if (!mtx) {
        return send_error(fd, 500);
    }
    pthread_mutex_lock(mtx);
    int status = file_put(req->uri, req->body, req->body_length);
    pthread_mutex_unlock(mtx);

    if (status == 200) {
        const char *body = "OK\n";
        size_t len = strlen(body);
        dprintf(fd,
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: %zu\r\n"
            "\r\n",
            len);
        write(fd, body, len);
        return 200;
    }

    if (status == 201) {
        const char *body = "Created\n";
        size_t len = strlen(body);
        dprintf(fd,
            "HTTP/1.1 201 Created\r\n"
            "Content-Length: %zu\r\n"
            "\r\n",
            len);
        write(fd, body, len);
        return 201;
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
