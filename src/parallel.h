#ifndef PARALLEL_H
#define PARALLEL_H

#include <stddef.h>

int parallel_init(size_t num_threads);

void parallel_shutdown(void);

int parallel_connection_handler(int client_fd, void *context);

#endif
