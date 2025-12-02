#ifndef SERVER_H
#define SERVER_H

#include <stddef.h>

#define SERVER_CONNECTION_CONTINUE 0
#define SERVER_CONNECTION_SHUTDOWN 1

typedef int (*server_connection_handler)(int client_fd, void *context);

// Starts the server listening on the given port and serving files from data_dir.
// Returns 0 on clean shutdown, nonzero on fatal error.
int run_server(int port, const char *data_dir);

// Override the connection handler (primarily for tests). Passing NULL resets to
// the default handle_connection provided by connection.c.
void server_set_connection_handler(server_connection_handler handler, void *context);

#endif
