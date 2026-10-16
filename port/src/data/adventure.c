#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "adventure.h"
#include "adf.h"
#include "bytes.h"
#include "crc32.h"
#include "file.h"

int16_t g_adventure = ADVENTURE_DM;

static const Adventure adventures[] = {
    {"dm",  "game",     "Dungeon Master v3.60", "KAOS.FTL", "DMGAME.DAT",  "DMGAME.BAK" },
    {"csb", "game-csb", "Chaos Strikes Back",   "APPB.FTL", "CSBGAME.DAT", "CSBGAME.BAK"},
};

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
    for (int i = 0; i < ARRAY_COUNT(adventures); i++)
        if (!strcmp(name, adventures[i].name))
            return i;
    return -1;
}

const Adventure *adventure_get(int16_t adventure)
{
    return &adventures[adventure];
}
const char *adventure_default_dir(int16_t adventure)
{
    return adventures[adventure].dir;
}
const char *adventure_engine_name(void)
{
    return adventures[g_adventure].engine;
}
const char *adventure_save_name(void)
{
    return adventures[g_adventure].save;
}
const char *adventure_backup_name(void)
{
    return adventures[g_adventure].backup;
}

bool adventure_csb_present(const char *dir)
{
    char path[PATH_SIZE];
    for (int i = 0; i < GAME_FILES; i++) {
        snprintf(path, sizeof path, "%s/%s", dir, game_files[ADVENTURE_CSB][i].name);
        if (!file_exists(path))
            return false;
    }
    return true;
}

const char *adventure_bad_file(int16_t adventure, const char *dir)
{
    char path[PATH_SIZE];
    for (int i = 0; i < GAME_FILES && game_files[adventure][i].name; i++) {
        size_t len;
        snprintf(path, sizeof path, "%s/%s", dir, game_files[adventure][i].name);
        uint8_t *data = file_read(path, &len);
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
    for (int i = 0; i < ARRAY_COUNT(game_disks); i++) {
        if (!adf_has_file(adf_path, game_disks[i].file))
            continue;
        *dir = game_disks[i].dir;
        snprintf(path, sizeof path, "%s/%s", *dir, game_disks[i].file);
        if (file_exists(path))
            return 0;
        return adf_extract(adf_path, *dir) ? -1 : 1;
    }
    return 0;
}
