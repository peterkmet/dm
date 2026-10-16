#include <stdio.h>
#include <stdlib.h>
#include "file.h"

/* Reads all of an open file; NULL when a seek, the allocation or the read fails. */
static uint8_t *read_open_file(FILE *f, size_t *len)
{
    if (fseek(f, 0, SEEK_END))
        return NULL;
    int64_t n = ftell(f);
    if (n < 0 || fseek(f, 0, SEEK_SET))
        return NULL;
    uint8_t *buf = malloc(n ? (size_t)n : 1);
    if (!buf)
        return NULL;
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf);
        return NULL;
    }
    *len = (size_t)n;
    return buf;
}

uint8_t *file_read(const char *path, size_t *len)
{
    *len = 0;
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    uint8_t *buf = read_open_file(f, len);
    fclose(f);
    return buf;
}

bool file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return false;
    fclose(f);
    return true;
}
