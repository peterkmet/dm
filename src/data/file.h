/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_DATA_FILE_H
#define DM_DATA_FILE_H
/* Prefixes: file_ */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The malloc'd contents of the file at path and its length in *len; NULL when it is missing or unreadable. */
uint8_t *file_read(const char *path, size_t *len);
/* True when the file at path can be opened for reading. */
bool file_exists(const char *path);

#endif
