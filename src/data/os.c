/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "data/os.h"
#include <ctype.h>

OsMakeFolder g_os_make_folder_hook;
OsListFolder g_os_list_folder_hook;

bool os_make_folder(const char *path)
{
    return g_os_make_folder_hook && g_os_make_folder_hook(path);
}

void os_list_folder(const char *path, void (*found)(const char *name, void *data), void *data)
{
    if (g_os_list_folder_hook)
        g_os_list_folder_hook(path, found, data);
}

int os_compare_names(const char *a, const char *b)
{
    const unsigned char *p = (const unsigned char *)a;
    const unsigned char *q = (const unsigned char *)b;
    while (*p && tolower(*p) == tolower(*q)) {
        p++;
        q++;
    }
    return tolower(*p) - tolower(*q);
}
