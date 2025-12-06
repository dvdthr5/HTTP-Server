#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "server.h"

int handle_connection(int client_fd);

static server_connection_handler override_handler = NULL;
static void *override_ctx = NULL;

void server_set_connection_handler(server_connection_handler handler, void *context) {
    override_handler = handler;
    override_ctx = context;
}

static int dispatch_connection(int client_fd) {
    if (override_handler) {
        return override_handler(client_fd, override_ctx);
    }
    return handle_connection(client_fd);
}

int run_server(int port, const char *data_dir) {
    if (port <= 0 || port > 65535) {
        errno = EINVAL;
        return -1;
    }

    if (data_dir && *data_dir) {
        if (chdir(data_dir) != 0) {
            return -1;
        }
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        return -1;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(server_fd);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t) port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, 16) < 0) {
        close(server_fd);
        return -1;
    }

    int rc = 0;
    for (;;) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            rc = -1;
            break;
        }

        int handler_rc = dispatch_connection(client_fd);

        if (handler_rc == SERVER_CONNECTION_SHUTDOWN) {
            break;
        }
    }

    close(server_fd);
    return rc;
}
