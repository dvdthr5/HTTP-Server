#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int exists;
    int readable;
    int writable;
} file_status_t;

int file_stat_status(const char *path, file_status_t *out_status);
int file_read_all(const char *path, char **out_buf, size_t *out_len);
int file_write_all(const char *path, const void *data, size_t len, int *created);
int file_get(const char *uri, uint8_t **data_out, size_t *size_out);
int file_put(const char *uri, const uint8_t *data, size_t size);
