/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_CONTROL_REQUEST_H
#define DM_CONTROL_REQUEST_H
/* Prefixes: request_ */

/* The reader of one request line. Contract: a key occurs once per request; a string holds no '"' and no
   escape; objects nest no deeper than the args of an action. A key is found by a flat search of "key". */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Result of request_next_item. */
typedef enum {
    REQUEST_ITEM_END,   /* the list ends */
    REQUEST_ITEM_FOUND, /* out holds the next item */
    REQUEST_ITEM_BAD    /* the next item is not a "name" or an {...} object, or is too long */
} RequestItem;

/* Queries */
/* The text after "key" and its colon in line, or NULL when the key is missing. */
const char *request_find_value(const char *line, const char *key);
/* Copies the string value of key into out, cut to size; false when the key is missing or not a string. */
bool request_get_string(const char *line, const char *key, char *out, size_t size);
/* The integer value of key, or missing when the key is missing or not a number. */
int64_t request_get_int(const char *line, const char *key, int64_t missing);
/* Copies the next item of a list ("name" or {...}) at *cursor into out and moves *cursor past it. */
RequestItem request_next_item(const char **cursor, char *out, size_t size);

#endif
