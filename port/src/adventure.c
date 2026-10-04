#include <stdio.h>
#include <string.h>
#include "adventure.h"

#define PATH_SIZE 1024

int16_t g_adventure = ADVENTURE_DM;

static const char *const names[] = { "dm", "csb" };
static const char *const dirs[] = { "game", "game-csb" };
static const char *const save_names[] = { "DMGAME.DAT", "CSBGAME.DAT" };
static const char *const backup_names[] = { "DMGAME.BAK", "CSBGAME.BAK" };

int adventure_of(const char *name)
{
    for (int i = 0; i < (int)(sizeof names / sizeof names[0]); i++)
        if (!strcmp(name, names[i])) return i;
    return -1;
}

const char *adventure_default_dir(int16_t adventure) { return dirs[adventure]; }
const char *adventure_save_name(void) { return save_names[g_adventure]; }
const char *adventure_backup_name(void) { return backup_names[g_adventure]; }

int adventure_csb_present(const char *dir)
{
    static const char *const files[] = { "APPB.FTL", "Graphics.DAT", "Dungeon.DAT", "disk-b/MINI.DAT" };
    char path[PATH_SIZE];
    for (int i = 0; i < (int)(sizeof files / sizeof files[0]); i++) {
        snprintf(path, sizeof path, "%s/%s", dir, files[i]);
        FILE *f = fopen(path, "rb");
        if (!f) return 0;
        fclose(f);
    }
    return 1;
}
