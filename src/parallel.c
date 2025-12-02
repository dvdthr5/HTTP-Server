#define _GNU_SOURCE
#include "parallel.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "connection.h"
#include "server.h"

typedef struct job {
    int client_fd;
    struct job *next;
} job_t;

typedef struct {
    pthread_t *threads;
    size_t num_threads;

    job_t *head;
    job_t *tail;

    pthread_mutex_t mutex;
    pthread_cond_t cond;

    int shutting_down;
    int initialized;
} thread_pool_t;

static thread_pool_t pool = {
    .threads = NULL,
    .num_threads = 0,
    .head = NULL,
    .tail = NULL,
    .shutting_down = 0,
    .initialized = 0
};

static void enqueue_job(int client_fd) {
    job_t *job = malloc(sizeof(job_t));
    if (!job) {
        close(client_fd);
        return;
    }

    job->client_fd = client_fd;
    job->next = NULL;

    pthread_mutex_lock(&pool.mutex);

    if (pool.tail) {
        pool.tail->next = job;
        pool.tail = job;
    } else {
        pool.head = pool.tail = job;
    }

    pthread_cond_signal(&pool.cond);
    pthread_mutex_unlock(&pool.mutex);
}

static job_t *dequeue_job(void) {
    job_t *job = pool.head;
    if (!job) {
        return NULL;
    }

    pool.head = job->next;
    if (!pool.head) {
        pool.tail = NULL;
    }

    return job;
}

static void *worker_main(void *arg) {
    (void)arg;

    for (;;) {
        pthread_mutex_lock(&pool.mutex);

        while (!pool.head && !pool.shutting_down) {
            pthread_cond_wait(&pool.cond, &pool.mutex);
        }

        if (pool.shutting_down && !pool.head) {
            pthread_mutex_unlock(&pool.mutex);
            break;
        }

        job_t *job = dequeue_job();
        pthread_mutex_unlock(&pool.mutex);

        if (!job) {
            continue;
        }

        int client_fd = job->client_fd;
        free(job);

        handle_connection(client_fd);
    }

    return NULL;
}

int parallel_init(size_t num_threads) {
    if (pool.initialized) {
        return 0;
    }

    if (num_threads == 0) {
        errno = EINVAL;
        return -1;
    }

    pool.threads = calloc(num_threads, sizeof(pthread_t));
    if (!pool.threads) {
        return -1;
    }

    pool.num_threads = num_threads;
    pool.head = NULL;
    pool.tail = NULL;
    pool.shutting_down = 0;

    if (pthread_mutex_init(&pool.mutex, NULL) != 0) {
        free(pool.threads);
        pool.threads = NULL;
        return -1;
    }

    if (pthread_cond_init(&pool.cond, NULL) != 0) {
        pthread_mutex_destroy(&pool.mutex);
        free(pool.threads);
        pool.threads = NULL;
        return -1;
    }

    for (size_t i = 0; i < num_threads; i++) {
        int rc = pthread_create(&pool.threads[i], NULL, worker_main, NULL);
        if (rc != 0) {
            pthread_mutex_lock(&pool.mutex);
            pool.shutting_down = 1;
            pthread_cond_broadcast(&pool.cond);
            pthread_mutex_unlock(&pool.mutex);

            for (size_t j = 0; j < i; j++) {
                pthread_join(pool.threads[j], NULL);
            }

            pthread_cond_destroy(&pool.cond);
            pthread_mutex_destroy(&pool.mutex);
            free(pool.threads);
            pool.threads = NULL;
            errno = rc;
            return -1;
        }
    }

    pool.initialized = 1;
    return 0;
}

void parallel_shutdown(void) {
    if (!pool.initialized) {
        return;
    }

    pthread_mutex_lock(&pool.mutex);
    pool.shutting_down = 1;
    pthread_cond_broadcast(&pool.cond);
    pthread_mutex_unlock(&pool.mutex);

    for (size_t i = 0; i < pool.num_threads; i++) {
        pthread_join(pool.threads[i], NULL);
    }

    job_t *cur = pool.head;
    while (cur) {
        job_t *next = cur->next;
        close(cur->client_fd);
        free(cur);
        cur = next;
    }

    pthread_cond_destroy(&pool.cond);
    pthread_mutex_destroy(&pool.mutex);
    free(pool.threads);

    pool.threads = NULL;
    pool.num_threads = 0;
    pool.head = pool.tail = NULL;
    pool.shutting_down = 0;
    pool.initialized = 0;
}

int parallel_connection_handler(int client_fd, void *context) {
    (void)context;

    if (!pool.initialized) {
        handle_connection(client_fd);
        return SERVER_CONNECTION_CONTINUE;
    }

    enqueue_job(client_fd);
    return SERVER_CONNECTION_CONTINUE;
}
