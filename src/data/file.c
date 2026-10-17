/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "file.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

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

uint8_t *file_read(const char *path, size_t *read_length)
{
    *read_length = 0;
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    uint8_t *buf = read_open_file(f, read_length);
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

int file_compare_names(const char *a, const char *b)
{
    const unsigned char *p = (const unsigned char *)a;
    const unsigned char *q = (const unsigned char *)b;
    while (*p && tolower(*p) == tolower(*q)) {
        p++;
        q++;
    }
    return tolower(*p) - tolower(*q);
}

bool file_write(const char *path, const uint8_t *contents, size_t length)
{
    FILE *f = fopen(path, "wb");
    bool ok = f && fwrite(contents, 1, length, f) == length;
    if (f && fclose(f))
        ok = false;
    return ok;
}

bool file_replace(const char *from, const char *to)
{
    remove(to);
    return rename(from, to) == 0;
}
