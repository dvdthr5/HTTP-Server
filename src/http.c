#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <limits.h>
#include <errno.h>
#include <stdint.h>

struct http_request{
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



int parse_request(int fd, struct http_request *req){
    memset(req, 0, sizeof(*req));
    req->request_id = 0;
    req->content_length = -1;
    req->valid = 0;

    char line[2048];
    int len = safe_readline(fd, line, sizeof(line));
    if (len <= 0) return 400;
    if (len < 2 || line[len-1] != '\n' || line[len-2] != '\r') return 400;
    line[len-2] = '\0';
    


    int status = parse_request_line(line, req);
    if (status != 0) return status;

    status = parse_headers(fd, req);
    if (status != 0) return status;

    // Validate correct Content-Length rules
    if (strcmp(req->method, "GET") == 0 && req->content_length != -1) {
        return 400;
    }

    if (strcmp(req->method, "PUT") == 0 && req->content_length < 0) {
        return 400;
    }

    if (strcmp(req->method, "PUT") == 0) {
        if (req->content_length > 0) {
            req->body = malloc(req->content_length);
            if (!req->body) return 500;
        }
        status = read_request_body(fd, req);
        if (status != 0) return status;
    }

    req->valid = 1;
    
    return 0;
}

int parse_request_line(char *line, struct http_request *req){
    char *method = strtok(line, " ");
    char *uri = strtok(NULL, " ");
    char *version = strtok(NULL, " ");

    if (!method || !uri || !version || strtok(NULL, " ") != NULL) return 400;

    if (strcmp(method, "GET") != 0 && strcmp(method, "PUT") != 0) return 501;
    strncpy(req->method, method, sizeof(req->method) - 1);
    req->method[sizeof(req->method) - 1] = '\0';

    if (!is_valid_uri(uri)) return 400;
    strncpy(req->uri, uri, sizeof(req->uri) - 1);
    req->uri[sizeof(req->uri) - 1] = '\0';

    if (strcmp(version, "HTTP/1.1") != 0) return 505;
    strncpy(req->version, version, sizeof(req->version) - 1);
    req->version[sizeof(req->version) - 1] = '\0';
    return 0;
}

int parse_headers(int fd, struct http_request *req){
    char line[2048];

    int content_length_seen = 0;

    while(1){
        int len = safe_readline(fd, line, sizeof(line));
        if (len < 0) return 400;
        if (len == 2 && line[0] == '\r' && line[1] == '\n') break;
        line[len-2] = '\0';

        char *colon = strchr(line, ':');
        if (!colon) return 400;

        *colon = '\0';
        char *header_name = line;
        char *header_value = colon +1;

        while (*header_value == ' '){
            header_value++;
        }

        if (strcmp(header_name, "Content-Length") == 0) {
            if (content_length_seen) return 400;
            content_length_seen = 1;

            int parsed_len = parse_content_length(header_value);
            if (parsed_len < 0) return 400;
            req->content_length = parsed_len;
            continue;
        }

        if (strcmp(header_name, "Request-ID") == 0) {
            // Must be strictly numeric
            for (const char *p = header_value; *p; p++) {
                if (*p < '0' || *p > '9') {
                    return 400;
                }
            }

            errno = 0;
            long id = strtol(header_value, NULL, 10);
            if (errno == ERANGE || id < 0 || id > INT_MAX) {
                return 400;
            }

            req->request_id = (int) id;
            continue;
        }

        // Any other header is invalid
        return 400;
    }

    return 0;
}

int read_request_body(int fd, struct http_request *req){
    int remaining = req->content_length;
    int total_read = 0;

    if (remaining == 0){
        req->body_length = 0;
        return 0;
    }

    while (remaining > 0){
        ssize_t n = read(fd, req->body + total_read, remaining);
        if (n < 0) return 500;
        if (n == 0) return 400;
        total_read += n;
        remaining -= n;
    }
    req->body_length = total_read;

    return 0;
}

int is_valid_uri(const char *uri){

    int len = strlen(uri);
    if (len < 1 || len > 27) return 0;

    for (int i = 0; i < len; i++){
        char c = uri[i];
        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.') {
            continue;
        }
        return 0;
    }

    return 1;
}

int safe_readline(int fd, char *buffer, size_t max){
    
    size_t idx = 0;
    while (idx < max -1){
        char c;
        ssize_t n = read(fd, &c, 1);
        if (n < 0 || n == 0) return -1;
        buffer[idx++] = c;

        if (c == '\n'){
            buffer[idx] = '\0';
            return idx;
        }
    }
    
    return -1;
}

int parse_content_length(const char *header_value){
    if (!header_value || *header_value == '\0') return -1;
    errno = 0;

    for (const char *p = header_value; *p; p++){
        if (*p < '0' || *p > '9') return -1;
    }
    long val = strtol(header_value, NULL, 10);
    if (errno == ERANGE) return -1;
    if (val < 0 || val > INT_MAX) return -1;

    return (int)val;
}
