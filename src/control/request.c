/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "control/request.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Sizes. */
#define KEY_PATTERN_SIZE 64 /* a quoted request key */

/* The closing quote of the string that starts at the quote p, or NULL. */
static const char *skip_string(const char *p)
{
    return strchr(p + 1, '"');
}

const char *request_find_value(const char *line, const char *key)
{
    char pattern[KEY_PATTERN_SIZE];
    snprintf(pattern, sizeof pattern, "\"%s\"", key);
    const char *p = strstr(line, pattern);
    if (!p)
        return NULL;
    p += strlen(pattern);
    while (*p == ' ' || *p == ':')
        p++;
    return p;
}

bool request_get_string(const char *line, const char *key, char *out, size_t size)
{
    const char *p = request_find_value(line, key);
    if (!p || *p != '"')
        return false;
    const char *end = skip_string(p);
    size_t n = end ? (size_t)(end - p - 1) : strlen(p + 1);
    if (n + 1 > size)
        n = size - 1;
    memcpy(out, p + 1, n);
    out[n] = 0;
    return true;
}

int64_t request_get_int(const char *line, const char *key, int64_t missing)
{
    const char *p = request_find_value(line, key);
    char *end;
    if (!p)
        return missing;
    int64_t value = strtoll(p, &end, 10);
    return end == p ? missing : value;
}

RequestItem request_next_item(const char **cursor, char *out, size_t size)
{
    const char *p = *cursor;
    while (*p == ' ' || *p == ',')
        p++;
    const char *start = p;
    if (!*p || *p == ']')
        return REQUEST_ITEM_END;
    if (*p == '"') {
        p = skip_string(p);
        if (!p)
            return REQUEST_ITEM_BAD;
        p++;
    } else if (*p == '{') {
        for (int depth = 0; *p; p++) {
            if (*p == '"')
                p = skip_string(p);
            if (!p)
                return REQUEST_ITEM_BAD;
            depth += (*p == '{') - (*p == '}');
            if (!depth)
                break;
        }
        if (!*p++)
            return REQUEST_ITEM_BAD;
    } else {
        return REQUEST_ITEM_BAD;
    }
    if ((size_t)(p - start) >= size)
        return REQUEST_ITEM_BAD;
    memcpy(out, start, (size_t)(p - start));
    out[p - start] = 0;
    *cursor = p;
    return REQUEST_ITEM_FOUND;
}
