#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "../src/file.h"

// Create a temporary file path under /tmp
static char *tmp_path(const char *name) {
    static char buf[256];
    snprintf(buf, sizeof(buf), "/tmp/testfile_%s_%d", name, getpid());
    return buf;
}

void test_stat_nonexistent() {
    const char *path = tmp_path("nofile");
    file_status_t st;
    int rc = file_stat_status(path, &st);
    assert(rc == 0);
    assert(st.exists == 0);
    printf("test_stat_nonexistent: OK\n");
}

void test_write_new_file() {
    const char *path = tmp_path("new");

    uint8_t data[] = {1,2,3,4};
    int created = 0;
    int rc = file_write_all(path, data, sizeof(data), &created);

    assert(rc == 201);
    assert(created == 1);

    printf("test_write_new_file: OK\n");
}

void test_write_overwrite() {
    const char *path = tmp_path("overwrite");

    uint8_t first[] = {9,9};
    uint8_t second[] = {1,2,3};

    int created = 0;

    // First write (file does not exist)
    file_write_all(path, first, sizeof(first), &created);
    assert(created == 1);

    // Overwrite (existing file)
    created = 0;
    int rc = file_write_all(path, second, sizeof(second), &created);

    assert(rc == 200);
    assert(created == 0);

    printf("test_write_overwrite: OK\n");
}

void test_read_existing() {
    const char *path = tmp_path("read");
    uint8_t written[] = {'A','B','C','D'};

    int created = 0;
    file_write_all(path, written, sizeof(written), &created);

    char *buf = NULL;
    size_t size = 0;
    int rc = file_read_all(path, &buf, &size);

    assert(rc == 200);
    assert(size == sizeof(written));
    assert(memcmp(buf, written, size) == 0);

    free(buf);
    printf("test_read_existing: OK\n");
}

void test_read_nonexistent() {
    const char *path = tmp_path("doesnotexist");

    char *buf = NULL;
    size_t size = 0;
    int rc = file_read_all(path, &buf, &size);

    assert(rc == 404);
    assert(buf == NULL);
    assert(size == 0);

    printf("test_read_nonexistent: OK\n");
}

void test_read_empty_file() {
    const char *path = tmp_path("empty");

    // Create empty file
    FILE *f = fopen(path, "w");
    fclose(f);

    char *buf = NULL;
    size_t size = 0;
    int rc = file_read_all(path, &buf, &size);

    assert(rc == 200);
    assert(size == 0);
    assert(buf != NULL);  // Should allocate an empty buffer

    free(buf);
    printf("test_read_empty_file: OK\n");
}
int main() {
    test_stat_nonexistent();
    test_write_new_file();
    test_write_overwrite();
    test_read_existing();
    test_read_nonexistent();
    test_read_empty_file();
    return 0;
}