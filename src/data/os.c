/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "data/os.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

/* Message buffer. */
#define OS_MESSAGE_SIZE 512 /* longest log message */

OsMakeFolder g_os_make_folder_hook;
OsListFolder g_os_list_folder_hook;
OsLogHook g_os_log_hook;
OsFatalHook g_os_fatal_hook;

/* Sends a formatted message to the hook, or to stderr without a hook. */
static void report(OsLogHook hook, const char *format, va_list args)
{
    char message[OS_MESSAGE_SIZE];
    vsnprintf(message, sizeof message, format, args);
    if (hook)
        hook(message);
    else
        fprintf(stderr, "%s\n", message);
}

void os_log(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    report(g_os_log_hook, format, args);
    va_end(args);
}

_Noreturn void os_fatal(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    report(g_os_fatal_hook, format, args);
    va_end(args);
    exit(EXIT_FAILURE);
}

bool os_make_folder(const char *path)
{
    return g_os_make_folder_hook && g_os_make_folder_hook(path);
}

void os_list_folder(const char *path, void (*found)(const char *name, void *data), void *data)
{
    if (g_os_list_folder_hook)
        g_os_list_folder_hook(path, found, data);
}
