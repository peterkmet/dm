/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "data/adventure.h"
#include "data/adf.h"
#include "data/bytes.h"
#include "data/crc32.h"
#include "data/file.h"
#include "data/graphics.h"
#include "data/tables.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int16_t g_adventure = ADVENTURE_DM;

/* CRC32 of the game files of Dungeon Master v3.60 and Chaos Strikes Back for the Amiga. */
#define CRC_DM_KAOS      0x63c20344u
#define CRC_DM_GRAPHICS  0x04598281u
#define CRC_DM_DUNGEON   0xab3f5e58u
#define CRC_CSB_APPB     0xb11dca9du
#define CRC_CSB_GRAPHICS 0x3bc83984u
#define CRC_CSB_DUNGEON  0x4f765636u
#define CRC_CSB_MINI     0x7055872du

static const Adventure adventures[] = {
    {"dm",
     "game",     "Dungeon Master v3.60",
     "KAOS.FTL", "DMGAME.DAT",
     "DMGAME.BAK",  {{"KAOS.FTL", CRC_DM_KAOS}, {"Graphics.DAT", CRC_DM_GRAPHICS}, {"Dungeon.DAT", CRC_DM_DUNGEON}}},
    {"csb",
     "game-csb", "Chaos Strikes Back",
     "APPB.FTL", "CSBGAME.DAT",
     "CSBGAME.BAK", {{"APPB.FTL", CRC_CSB_APPB},
      {"Graphics.DAT", CRC_CSB_GRAPHICS},
      {"Dungeon.DAT", CRC_CSB_DUNGEON},
      {CSB_MINI_FILE, CRC_CSB_MINI}}                                                            },
};

/* A disk is known by a file in its root folder, tested in this order: disk B and the DM disk also have APPB.FTL. */
static const struct {
    const char *file, *dir;
} game_disks[] = {
    {"MINI.DAT", "game-csb/disk-b"},
    {"KAOS.FTL", "game"           },
    {"APPB.FTL", "game-csb"       }
};

const Adventure *adventure_get(int16_t adventure)
{
    return &adventures[adventure];
}

int16_t adventure_find(const char *name)
{
    for (int16_t i = 0; i < ARRAY_COUNT(adventures); i++)
        if (!strcmp(name, adventures[i].name))
            return i;
    return -1;
}

bool adventure_is_csb_present(const char *dir)
{
    char path[PATH_SIZE];
    const GameFile *files = adventures[ADVENTURE_CSB].files;
    for (int i = 0; i < ADVENTURE_GAME_FILES && files[i].name; i++) {
        snprintf(path, sizeof path, "%s/%s", dir, files[i].name);
        if (!file_exists(path))
            return false;
    }
    return true;
}

const char *adventure_find_bad_file(int16_t adventure, const char *dir)
{
    char path[PATH_SIZE];
    const GameFile *files = adventures[adventure].files;
    for (int i = 0; i < ADVENTURE_GAME_FILES && files[i].name; i++) {
        size_t len;
        snprintf(path, sizeof path, "%s/%s", dir, files[i].name);
        uint8_t *data = file_read(path, &len);
        uint32_t crc = data ? crc32_update(0, data, len) : 0;
        free(data);
        if (!data || crc != files[i].crc)
            return files[i].name;
    }
    return NULL;
}

bool adventure_load_files(const char *dir, const char **failed)
{
    char path[PATH_SIZE];
    const Adventure *a = adventure_get(g_adventure);
    snprintf(path, sizeof path, "%s/%s", dir, a->engine);
    if (!tables_load(path)) {
        *failed = a->engine;
        return false;
    }
    snprintf(path, sizeof path, "%s/%s", dir, ADVENTURE_GRAPHICS_FILE);
    if (!gfx_open(path)) {
        *failed = ADVENTURE_GRAPHICS_FILE;
        return false;
    }
    return true;
}

AdventureDisk adventure_install_disk(const char *adf_path, const char **disk_dir)
{
    char path[PATH_SIZE];
    for (int i = 0; i < ARRAY_COUNT(game_disks); i++) {
        if (!adf_has_file(adf_path, game_disks[i].file))
            continue;
        *disk_dir = game_disks[i].dir;
        snprintf(path, sizeof path, "%s/%s", *disk_dir, game_disks[i].file);
        if (file_exists(path))
            return ADVENTURE_DISK_SKIPPED;
        return adf_extract(adf_path, *disk_dir) ? ADVENTURE_DISK_INSTALLED : ADVENTURE_DISK_FAILED;
    }
    return ADVENTURE_DISK_SKIPPED;
}
