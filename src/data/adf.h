/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_DATA_ADF_H
#define DM_DATA_ADF_H
/* Prefixes: adf_ */
#include <stdbool.h>

/* Reads an Amiga floppy disk image (.adf, 880 KB, OFS file system), the form of the original game disks. */

/* true when the root folder of the disk image has a file of this name (the case does not matter). */
bool adf_has_file(const char *adf_path, const char *name);
/* Writes every file of the disk image below out_dir and makes the folders. False when the
   image is not an OFS floppy, a file name is not safe (adf_is_safe_name), or a file cannot be written. */
bool adf_extract(const char *adf_path, const char *out_dir);
/* true when an Amiga file name is a safe file name on macOS and Windows: not empty, not "." or "..", no
   control character and none of / \ : * ? " < > |; exported for the tests. */
bool adf_is_safe_name(const char *name);

#endif
