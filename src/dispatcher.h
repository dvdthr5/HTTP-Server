#ifndef DISPATCHER_H
#define DISPATCHER_H

#include "http.h"

int dispatch_request(struct http_request *req, int fd);

int handle_get(struct http_request *req, int fd);

int handle_put(struct http_request *req, int fd);

#endif
