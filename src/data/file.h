/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_DATA_FILE_H
#define DM_DATA_FILE_H
/* Prefixes: file_ */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Whole-file reads and writes and file names: the C standard library only, no folder services. */
#define PATH_SIZE 1024 /* longest file path */

/* The malloc'd contents of the file at path and its length in *len; NULL when it is missing or unreadable. */
uint8_t *file_read(const char *path, size_t *len);
/* True when the file at path can be opened for reading. */
bool file_exists(const char *path);
/* Compares two file names as ASCII without case, as strcasecmp. */
int file_compare_names(const char *a, const char *b);
/* Writes length bytes to path, replacing the file. True when every byte is written and the file is closed. */
bool file_write(const char *path, const uint8_t *contents, size_t length);
/* Renames from to to, replacing to. True on success. */
bool file_replace(const char *from, const char *to);

#endif
