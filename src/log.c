#include "log.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static int log_ready = 0;

int log_init(void) {
    if (log_ready) {
        return 0;
    }

    int rc = pthread_mutex_init(&log_mutex, NULL);
    if (rc != 0) {
        errno = rc;
        return -1;
    }

    log_ready = 1;
    return 0;
}

void log_close(void) {
    if (!log_ready) {
        return;
    }

    pthread_mutex_destroy(&log_mutex);
    log_ready = 0;
}

void log_request(const struct http_request *req, int status_code) {
    if (!log_ready || req == NULL) {
        return;
    }

    pthread_mutex_lock(&log_mutex);
    fprintf(stderr,
            "%s,%s,%d,%d\n",
            req->method[0] ? req->method : "-",
            req->uri[0] ? req->uri : "-",
            status_code,
            req->request_id);
    fflush(stderr);
    pthread_mutex_unlock(&log_mutex);
}
