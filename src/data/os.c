/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "data/os.h"

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
