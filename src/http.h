#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>
#include <stdint.h>

struct http_request {
    char method[8];
    char uri[2048];
    char version[16];

    int content_length;
    uint8_t *body;
    size_t body_length;

    int request_id;
    int valid;
};
int parse_request(int fd, struct http_request *req);
int parse_request_line(char *line, struct http_request *req);
int parse_headers(int fd, struct http_request *req);
int read_request_body(int fd, struct http_request *req);
int is_valid_uri(const char *uri);
int safe_readline(int fd, char *buffer, size_t max);
int parse_content_length(const char *header_value);

#endif
