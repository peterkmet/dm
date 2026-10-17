/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_DATA_OS_H
#define DM_DATA_OS_H
/* Prefixes: os_ */

/* The operating system services that data/ and engine/ need: folders, file times, the log, and fatal errors. platform/ sets the hooks; tests set their own. */

#include <stdbool.h>
#include <stdint.h>

typedef bool (*OsMakeFolder)(const char *path);
typedef void (*OsListFolder)(const char *path, void (*found)(const char *name, void *data), void *data);
extern OsMakeFolder g_os_make_folder_hook; /* platform/ sets it: SDL_CreateDirectory */
extern OsListFolder g_os_list_folder_hook; /* platform/ sets it: SDL_EnumerateDirectory */

/* A local date and time to the minute. */
typedef struct {
    int16_t year, month, day, hour, minute;
} OsDateTime;
typedef bool (*OsFileTime)(const char *path, OsDateTime *time);
extern OsFileTime g_os_file_time_hook; /* platform/ sets it: SDL_GetPathInfo, local time */

/* Messages and fatal errors of data/ and engine/. platform/ sets the hooks to its log; tests set their own. */
typedef void (*OsLogHook)(const char *message);
typedef void (*OsFatalHook)(const char *message);
extern OsLogHook g_os_log_hook;     /* NULL: the message goes to stderr */
extern OsFatalHook g_os_fatal_hook; /* NULL: the message goes to stderr */
/* Formats a message as printf and logs it. */
void os_log(const char *format, ...) __attribute__((format(printf, 1, 2)));
/* Formats a message as printf, reports it, and ends the program: a port bug, never bad game data. */
_Noreturn void os_fatal(const char *format, ...) __attribute__((format(printf, 1, 2)));

/* Makes the folder. True when the folder exists afterwards (also when it existed before); false when no hook is set. */
bool os_make_folder(const char *path);
/* Calls found for each name in the folder. Calls nothing when no hook is set. */
void os_list_folder(const char *path, void (*found)(const char *name, void *data), void *data);
/* The local modification time of the file. False when the file is missing or no hook is set. */
bool os_get_file_time(const char *path, OsDateTime *time);

#endif
