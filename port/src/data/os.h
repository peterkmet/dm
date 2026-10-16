#ifndef DM_OS_H
#define DM_OS_H
/* Prefixes: os_ */
#include <stdbool.h>

/* Folder services of the operating system. platform/ sets the hooks; tests set their own. */
typedef bool (*OsMakeFolder)(const char *path);
typedef void (*OsListFolder)(const char *path, void (*found)(const char *name, void *data), void *data);
extern OsMakeFolder g_os_make_folder_hook; /* platform/ sets it: SDL_CreateDirectory */
extern OsListFolder g_os_list_folder_hook; /* platform/ sets it: SDL_EnumerateDirectory */

/* Makes the folder. True when the folder exists afterwards (also when it existed before); false when no hook is set. */
bool os_make_folder(const char *path);
/* Calls found for each name in the folder. Calls nothing when no hook is set. */
void os_list_folder(const char *path, void (*found)(const char *name, void *data), void *data);
/* Compares two names as ASCII without case, as strcasecmp. */
int os_compare_names(const char *a, const char *b);

#endif
