#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "adventure.h"
#include "adf.h"
#include "crc32.h"
#include "ftl.h"

int16_t g_adventure = ADVENTURE_DM;

static const char *const names[] = {"dm", "csb"};
static const char *const dirs[] = {"game", "game-csb"};
static const char *const engine_names[] = {"KAOS.FTL", "APPB.FTL"};
static const char *const save_names[] = {"DMGAME.DAT", "CSBGAME.DAT"};
static const char *const backup_names[] = {"DMGAME.BAK", "CSBGAME.BAK"};

/* CRC32 of the game files of Dungeon Master v3.60 and Chaos Strikes Back for the Amiga. */
#define CRC_DM_KAOS      0x63c20344u
#define CRC_DM_GRAPHICS  0x04598281u
#define CRC_DM_DUNGEON   0xab3f5e58u
#define CRC_CSB_APPB     0xb11dca9du
#define CRC_CSB_GRAPHICS 0x3bc83984u
#define CRC_CSB_DUNGEON  0x4f765636u
#define CRC_CSB_MINI     0x7055872du
#define GAME_FILES       4 /* the most files of one adventure */

typedef struct {
    const char *name;
    uint32_t crc;
} GameFile;

static const GameFile game_files[][GAME_FILES] = {
    {{"KAOS.FTL", CRC_DM_KAOS}, {"Graphics.DAT", CRC_DM_GRAPHICS}, {"Dungeon.DAT", CRC_DM_DUNGEON}},
    {{"APPB.FTL", CRC_CSB_APPB},
     {"Graphics.DAT", CRC_CSB_GRAPHICS},
     {"Dungeon.DAT", CRC_CSB_DUNGEON},
     {CSB_MINI_FILE, CRC_CSB_MINI}},
};

/* A disk is known by a file in its root folder, tested in this order: disk B and the DM disk also have APPB.FTL. */
static const struct {
    const char *file, *dir;
} game_disks[] = {
    {"MINI.DAT", "game-csb/disk-b"},
    {"KAOS.FTL", "game"           },
    {"APPB.FTL", "game-csb"       }
};

int adventure_of(const char *name)
{
    for (int i = 0; i < (int)(sizeof names / sizeof names[0]); i++)
        if (!strcmp(name, names[i]))
            return i;
    return -1;
}

const char *adventure_default_dir(int16_t adventure)
{
    return dirs[adventure];
}
const char *adventure_engine_name(void)
{
    return engine_names[g_adventure];
}
const char *adventure_save_name(void)
{
    return save_names[g_adventure];
}
const char *adventure_backup_name(void)
{
    return backup_names[g_adventure];
}

bool adventure_csb_present(const char *dir)
{
    char path[PATH_SIZE];
    for (int i = 0; i < GAME_FILES; i++) {
        snprintf(path, sizeof path, "%s/%s", dir, game_files[ADVENTURE_CSB][i].name);
        FILE *f = fopen(path, "rb");
        if (!f)
            return false;
        fclose(f);
    }
    return true;
}

const char *adventure_bad_file(int16_t adventure, const char *dir)
{
    char path[PATH_SIZE];
    for (int i = 0; i < GAME_FILES && game_files[adventure][i].name; i++) {
        size_t len;
        snprintf(path, sizeof path, "%s/%s", dir, game_files[adventure][i].name);
        uint8_t *data = read_file(path, &len);
        uint32_t crc = data ? crc32_update(0, data, len) : 0;
        free(data);
        if (!data || crc != game_files[adventure][i].crc)
            return game_files[adventure][i].name;
    }
    return NULL;
}

int adventure_install_disk(const char *adf_path, const char **dir)
{
    char path[PATH_SIZE];
    for (int i = 0; i < (int)(sizeof game_disks / sizeof game_disks[0]); i++) {
        if (!adf_has_file(adf_path, game_disks[i].file))
            continue;
        *dir = game_disks[i].dir;
        snprintf(path, sizeof path, "%s/%s", *dir, game_disks[i].file);
        FILE *f = fopen(path, "rb");
        if (f)
            return fclose(f), 0;
        return adf_extract(adf_path, *dir) ? -1 : 1;
    }
    return 0;
}
