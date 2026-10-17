/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_CONTROL_JSON_H
#define DM_CONTROL_JSON_H
/* Prefixes: json_ */

/* A JSON text writer for the protocol replies. It only writes; an allocation failure aborts. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* A growing JSON text. Start with {0}; s points to owned memory, release it with json_free. */
typedef struct {
    char *s;
    size_t len;
    size_t cap;
} Json;

/* Setup */
/* Releases the text and leaves an empty writer. */
void json_free(Json *j);

/* Values: a value or a key gets its comma before it when it is not the first in its object or array. */
/* Appends text as it is. */
void json_raw(Json *j, const char *text);
/* Appends a key and its colon. */
void json_key(Json *j, const char *key);
/* Appends a number. */
void json_int(Json *j, int64_t value);
/* Appends true or false. */
void json_bool(Json *j, bool flag);
/* Appends null. */
void json_null(Json *j);
/* Appends a quoted string with the JSON escapes. */
void json_str(Json *j, const char *text);
/* Opens an object '{' or an array '['. */
void json_open(Json *j, char bracket);
/* Closes an object '}' or an array ']'. */
void json_close(Json *j, char bracket);

/* Members */
/* Appends a key with a number. */
void json_key_int(Json *j, const char *key, int64_t value);
/* Appends a key with a string. */
void json_key_str(Json *j, const char *key, const char *text);
/* Appends a key with true or false. */
void json_key_bool(Json *j, const char *key, bool flag);

#endif
