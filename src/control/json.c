/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "json.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Sizes of the writer buffers. */
#define JSON_START_SIZE  1024 /* the first buffer of a JSON text */
#define JSON_NUMBER_SIZE 24   /* an int64_t in decimal digits with its sign */
#define JSON_ESCAPE_SIZE 8    /* one escaped character, at most six bytes, and the end */

void json_free(Json *j)
{
    free(j->s);
    *j = (Json){0};
}

/* Appends n bytes, growing the buffer. */
static void put(Json *j, const char *text, size_t n)
{
    if (j->len + n + 1 > j->cap) {
        size_t cap = j->cap ? j->cap : JSON_START_SIZE;
        while (cap < j->len + n + 1)
            cap *= 2;
        char *s = realloc(j->s, cap);
        if (!s)
            abort();
        j->s = s;
        j->cap = cap;
    }
    memcpy(j->s + j->len, text, n);
    j->len += n;
    j->s[j->len] = 0;
}

/* Puts a comma before the next value unless it opens an object, a list, or follows a key. */
static void separate(Json *j)
{
    if (j->len && !strchr("{[:", j->s[j->len - 1]))
        put(j, ",", 1);
}

void json_raw(Json *j, const char *text)
{
    put(j, text, strlen(text));
}

void json_key(Json *j, const char *key)
{
    json_str(j, key);
    put(j, ":", 1);
}

void json_int(Json *j, int64_t value)
{
    char buf[JSON_NUMBER_SIZE];
    separate(j);
    snprintf(buf, sizeof buf, "%" PRId64, value);
    json_raw(j, buf);
}

void json_bool(Json *j, bool flag)
{
    separate(j);
    json_raw(j, flag ? "true" : "false");
}

void json_null(Json *j)
{
    separate(j);
    json_raw(j, "null");
}

void json_str(Json *j, const char *text)
{
    char buf[JSON_ESCAPE_SIZE];
    separate(j);
    put(j, "\"", 1);
    for (const unsigned char *character = (const unsigned char *)text; *character; character++) {
        if (*character == '"' || *character == '\\') {
            buf[0] = '\\';
            buf[1] = (char)*character;
            put(j, buf, 2);
        } else if (*character == '\n') {
            put(j, "\\n", 2);
        } else if (*character < ' ' || *character > '~') {
            snprintf(buf, sizeof buf, "\\u%04x", *character);
            put(j, buf, 6);
        } else {
            put(j, (const char *)character, 1);
        }
    }
    put(j, "\"", 1);
}

void json_open(Json *j, char bracket)
{
    separate(j);
    put(j, &bracket, 1);
}

void json_close(Json *j, char bracket)
{
    put(j, &bracket, 1);
}

void json_key_int(Json *j, const char *key, int64_t value)
{
    json_key(j, key);
    json_int(j, value);
}

void json_key_str(Json *j, const char *key, const char *text)
{
    json_key(j, key);
    json_str(j, text);
}

void json_key_bool(Json *j, const char *key, bool flag)
{
    json_key(j, key);
    json_bool(j, flag);
}
