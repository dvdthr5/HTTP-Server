#include "file.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int errno_to_status(int err) {
    if (err == ENOENT) {
        return 404;
    }
    if (err == EACCES || err == EPERM) {
        return 403;
    }
    if (err == EISDIR) {
        return 403;
    }
    return 500;
}

static char *build_path(const char *uri) {
    if (!uri) {
        return NULL;
    }

    if (uri[0] == '/') {
        uri += 1;
    }

    const char *tmp = getenv("TEST_TMPDIR");
    const char *base = (tmp && *tmp) ? tmp : ".";
    size_t len = strlen(base) + 1 + strlen(uri) + 1;

    char *res = malloc(len);
    if (!res) {
        return NULL;
    }

    snprintf(res, len, "%s/%s", base, uri);
    return res;
}

int file_stat_status(const char *path, file_status_t *out_status) {
    if (!path || !out_status) {
        errno = EINVAL;
        return -1;
    }

    char *full = build_path(path);
    if (!full) {
        errno = ENOMEM;
        return -1;
    }

    struct stat st;
    if (stat(full, &st) == -1) {
        if (errno == ENOENT) {
            out_status->exists = 0;
            out_status->readable = 0;
            out_status->writable = 0;
            free(full);
            return 0;
        }
        free(full);
        return -1;
    }

    out_status->exists = 1;
    mode_t mode = st.st_mode;
    out_status->readable = ((mode & (S_IRUSR | S_IRGRP | S_IROTH)) != 0);
    out_status->writable = ((mode & (S_IWUSR | S_IWGRP | S_IWOTH)) != 0);

    free(full);
    return 0;
}

int file_read_all(const char *path, char **out_buf, size_t *out_len) {
    if (!path || !out_buf || !out_len) {
        return 500;
    }

    *out_buf = NULL;
    *out_len = 0;

    char *full = build_path(path);
    if (!full) {
        return 500;
    }

    struct stat st;
    if (stat(full, &st) == -1) {
        int status = errno_to_status(errno);
        free(full);
        return status;
    }
    if (S_ISDIR(st.st_mode)) {
        free(full);
        return 403;
    }

    FILE *fp = fopen(full, "rb");
    if (!fp) {
        int status = errno_to_status(errno);
        free(full);
        return status;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        free(full);
        return 500;
    }

    long sz = ftell(fp);
    if (sz < 0) {
        fclose(fp);
        free(full);
        return 500;
    }

    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        free(full);
        return 500;
    }

    size_t size = (size_t) sz;
    size_t alloc_size = size == 0 ? 1 : size;
    char *buf = malloc(alloc_size);
    if (!buf) {
        fclose(fp);
        free(full);
        return 500;
    }

    if (size > 0) {
        size_t n = fread(buf, 1, size, fp);
        if (n != size) {
            free(buf);
            fclose(fp);
            free(full);
            return 500;
        }
    } else {
        buf[0] = '\0';
    }

    fclose(fp);
    free(full);

    *out_buf = buf;
    *out_len = size;
    return 200;
}

int file_write_all(const char *path, const void *data, size_t len, int *created) {
    if (!path || (!data && len > 0)) {
        return 500;
    }

    if (created) {
        *created = 0;
    }

    char *full = build_path(path);
    if (!full) {
        return 500;
    }

    struct stat st;
    int existed_before = (stat(full, &st) == 0);

    if (existed_before) {
        if (access(full, W_OK) != 0) {
            free(full);
            return 403;
        }
    }

    FILE *fp = fopen(full, "wb");
    if (!fp) {
        int status = errno_to_status(errno);
        free(full);
        return status;
    }

    if (len > 0) {
        size_t n = fwrite(data, 1, len, fp);
        if (n != len) {
            fclose(fp);
            free(full);
            return 500;
        }
    }

    if (fclose(fp) != 0) {
        free(full);
        return 500;
    }

    int status = existed_before ? 200 : 201;
    if (!existed_before && created) {
        *created = 1;
    }

    free(full);
    return status;
}

int file_get(const char *uri, uint8_t **data_out, size_t *size_out) {
    if (!uri || !data_out || !size_out) {
        return 500;
    }

    char *buf = NULL;
    size_t len = 0;
    int status = file_read_all(uri, &buf, &len);
    if (status != 200) {
        return status;
    }

    *data_out = (uint8_t *) buf;
    *size_out = len;
    return 200;
}

int file_put(const char *uri, const uint8_t *data, size_t size) {
    if (!uri) {
        return 500;
    }

    file_status_t st;
    if (file_stat_status(uri, &st) == -1) {
        return 500;
    }

    if (st.exists && !st.writable) {
        return 403;
    }

    return file_write_all(uri, data, size, NULL);
}
