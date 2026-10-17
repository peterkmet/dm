/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "savegame.h"
#include "adventure.h"
#include "bytes.h"
#include "file.h"
#include "graphics.h"
#include "os.h"
#include "champion.h"
#include "discard.h"
#include "dungeon.h"
#include "explored.h"
#include "game.h"
#include "group.h"
#include "inventory.h"
#include "map.h"
#include "party.h"
#include "random.h"
#include "savefile.h"
#include "timeline.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Sizes, limits and names of the save game code. */
#define TIMELINE_ENTRY_BYTES       2 /* one event index per timeline entry */
#define EXTENSION_LENGTH_BYTES     4 /* the length of an extension block, after the block */
#define NEW_ADVENTURE_LUCK_MINIMUM 10
#define SAVED_FREE_INDEX_SHIFT     16 /* a free event slot of the file keeps the next free index in the high word */
#define QUICK_SAVE_FILE_NAME       "QUICK.DAT"

/* The cheat block of the save extension: the magic, the version, and one byte of cheat bits (CHEAT_*). */
#define CHEAT_SAVE_MAGIC   "CHTS"
#define CHEAT_SAVE_VERSION 1
#define CHEAT_MAGIC_BYTES  4
#define CHEAT_SAVE_BYTES   6

/* Party record (g_party) offsets. */
#define SAVE_PARTY_MAGICAL_LIGHT   0
#define SAVE_PARTY_THIEVES_EYE     2
#define SAVE_PARTY_FOOTPRINTS      3
#define SAVE_PARTY_SHIELD          4
#define SAVE_PARTY_FIRE_SHIELD     6
#define SAVE_PARTY_SPELL_SHIELD    8
#define SAVE_PARTY_SCENT_COUNT     10
#define SAVE_PARTY_FREEZE_LIFE     11
#define SAVE_PARTY_FIRST_SCENT     12
#define SAVE_PARTY_LAST_SCENT      13
#define SAVE_PARTY_SCENTS          14
#define SAVE_PARTY_SCENT_STRENGTHS 62
#define SAVE_PARTY_INVISIBILITY    86

/* Timeline event record offsets. */
#define SAVE_EVENT_MAP_TIME 0x00
#define SAVE_EVENT_TYPE     0x04
#define SAVE_EVENT_PRIORITY 0x05
#define SAVE_EVENT_DATA_B   0x06
#define SAVE_EVENT_DATA_C   0x08

/* Active group record offsets: group index, then 14 bytes in the order of ActiveGroup. */
#define SAVE_GROUP_INDEX 0x00
/* Size of the group index in the record. */
#define SAVE_GROUP_BYTES 2

static uint8_t header_extra
    [SAVE_HEADER_EXTRA_BYTES]; /* the extra save header bytes of the loaded game, copied unchanged into the next save */

/* Block 0: game time, random seed, party position, leader, and the timeline and group counts. */
static void pack_globals(uint8_t *g)
{
    memset(g, 0, SAVE_GLOBALS_BYTES);
    wr32(g + SAVE_GLOBAL_GAME_TIME, g_game_time);
    wr32(g + SAVE_GLOBAL_RANDOM_SEED, g_random_seed);
    wr16(g + SAVE_GLOBAL_LEADER_HAND_OBJECT, g_leader_hand_object);
    wr16(g + SAVE_GLOBAL_PARTY_CHAMPION_COUNT, g_party_champion_count);
    wr16(g + SAVE_GLOBAL_PARTY_MAP_X, g_party_map_x);
    wr16(g + SAVE_GLOBAL_PARTY_MAP_Y, g_party_map_y);
    wr16(g + SAVE_GLOBAL_PARTY_DIRECTION, g_party_direction);
    wr16(g + SAVE_GLOBAL_PARTY_MAP_INDEX, g_party_map_index);
    wr16(g + SAVE_GLOBAL_LEADER_INDEX, g_leader_index);
    wr16(g + SAVE_GLOBAL_MAGIC_CASTER, g_magic_caster_champion_index);
    wr16(g + SAVE_GLOBAL_TIMELINE_COUNT, g_timeline.count);
    wr16(g + SAVE_GLOBAL_FIRST_UNUSED_EVENT, g_timeline.first_unused);
    wr16(g + SAVE_GLOBAL_EVENT_MAX_COUNT, g_timeline.capacity);
    wr16(g + SAVE_GLOBAL_ACTIVE_GROUP_COUNT, g_active_group_count);
    wr32(g + SAVE_GLOBAL_LAST_CREATURE_ATTACK_TIME, (uint32_t)g_last_creature_attack_time);
    wr32(g + SAVE_GLOBAL_LAST_PARTY_MOVEMENT_TIME, (uint32_t)g_last_party_movement_time);
    wr16(g + SAVE_GLOBAL_DISABLED_MOVEMENT_TICKS, g_disabled_movement_ticks);
    wr16(g + SAVE_GLOBAL_PROJECTILE_DISABLED_MOVEMENT_TICKS, g_projectile_disabled_movement_ticks);
    wr16(g + SAVE_GLOBAL_LAST_PROJECTILE_DIRECTION, g_last_projectile_disabled_movement_direction);
    wr16(g + SAVE_GLOBAL_MAX_ACTIVE_GROUP_COUNT, g_max_active_group_count);
}

/* Block 0 back into the globals; the game time also becomes the time of the last load. */
static void unpack_globals(const uint8_t *g)
{
    g_session.last_load_time = be32(g + SAVE_GLOBAL_GAME_TIME);
    g_game_time = g_session.last_load_time;
    g_random_seed = be32(g + SAVE_GLOBAL_RANDOM_SEED);
    g_leader_hand_object = be16(g + SAVE_GLOBAL_LEADER_HAND_OBJECT);
    g_party_champion_count = be16(g + SAVE_GLOBAL_PARTY_CHAMPION_COUNT);
    g_party_map_x = be16(g + SAVE_GLOBAL_PARTY_MAP_X);
    g_party_map_y = be16(g + SAVE_GLOBAL_PARTY_MAP_Y);
    g_party_direction = be16(g + SAVE_GLOBAL_PARTY_DIRECTION);
    g_party_map_index = be16(g + SAVE_GLOBAL_PARTY_MAP_INDEX);
    g_leader_index = be16(g + SAVE_GLOBAL_LEADER_INDEX);
    g_magic_caster_champion_index = be16(g + SAVE_GLOBAL_MAGIC_CASTER);
    g_timeline.count = be16(g + SAVE_GLOBAL_TIMELINE_COUNT);
    g_timeline.first_unused = be16(g + SAVE_GLOBAL_FIRST_UNUSED_EVENT);
    g_timeline.capacity = be16(g + SAVE_GLOBAL_EVENT_MAX_COUNT);
    g_active_group_count = be16(g + SAVE_GLOBAL_ACTIVE_GROUP_COUNT);
    g_last_creature_attack_time = be32(g + SAVE_GLOBAL_LAST_CREATURE_ATTACK_TIME);
    g_last_party_movement_time = be32(g + SAVE_GLOBAL_LAST_PARTY_MOVEMENT_TIME);
    g_disabled_movement_ticks = be16(g + SAVE_GLOBAL_DISABLED_MOVEMENT_TICKS);
    g_projectile_disabled_movement_ticks = be16(g + SAVE_GLOBAL_PROJECTILE_DISABLED_MOVEMENT_TICKS);
    g_last_projectile_disabled_movement_direction = be16(g + SAVE_GLOBAL_LAST_PROJECTILE_DIRECTION);
    g_max_active_group_count = be16(g + SAVE_GLOBAL_MAX_ACTIVE_GROUP_COUNT);
}

/* The 128 bytes from the party record; the bytes after the record stay 0. */
static void pack_party(uint8_t *p)
{
    memset(p, 0, SAVE_PARTY_BYTES);
    wr16(p + SAVE_PARTY_MAGICAL_LIGHT, g_party.magical_light_amount);
    p[SAVE_PARTY_THIEVES_EYE] = g_party.thieves_eye_count;
    p[SAVE_PARTY_FOOTPRINTS] = g_party.footprints_count;
    wr16(p + SAVE_PARTY_SHIELD, g_party.shield_defense);
    wr16(p + SAVE_PARTY_FIRE_SHIELD, g_party.fire_shield_defense);
    wr16(p + SAVE_PARTY_SPELL_SHIELD, g_party.spell_shield_defense);
    p[SAVE_PARTY_SCENT_COUNT] = g_party.scent_count;
    p[SAVE_PARTY_FREEZE_LIFE] = g_party.freeze_life_ticks;
    p[SAVE_PARTY_FIRST_SCENT] = g_party.first_scent_index;
    p[SAVE_PARTY_LAST_SCENT] = g_party.last_scent_index;
    for (int i = 0; i < PARTY_SCENT_COUNT; i++)
        wr16(p + SAVE_PARTY_SCENTS + 2 * i, g_party.scents[i]);
    memcpy(p + SAVE_PARTY_SCENT_STRENGTHS, g_party.scent_strengths, sizeof g_party.scent_strengths);
    p[SAVE_PARTY_INVISIBILITY] = g_party.invisibility_count;
}

/* Reads the party from the 128 bytes of the party record. */
static void unpack_party(const uint8_t *p)
{
    memset(&g_party, 0, sizeof g_party);
    g_party.magical_light_amount = be16(p + SAVE_PARTY_MAGICAL_LIGHT);
    g_party.thieves_eye_count = p[SAVE_PARTY_THIEVES_EYE];
    g_party.footprints_count = p[SAVE_PARTY_FOOTPRINTS];
    g_party.shield_defense = be16(p + SAVE_PARTY_SHIELD);
    g_party.fire_shield_defense = be16(p + SAVE_PARTY_FIRE_SHIELD);
    g_party.spell_shield_defense = be16(p + SAVE_PARTY_SPELL_SHIELD);
    g_party.scent_count = p[SAVE_PARTY_SCENT_COUNT];
    g_party.freeze_life_ticks = p[SAVE_PARTY_FREEZE_LIFE];
    g_party.first_scent_index = p[SAVE_PARTY_FIRST_SCENT];
    g_party.last_scent_index = p[SAVE_PARTY_LAST_SCENT];
    for (int i = 0; i < PARTY_SCENT_COUNT; i++)
        g_party.scents[i] = be16(p + SAVE_PARTY_SCENTS + 2 * i);
    memcpy(g_party.scent_strengths, p + SAVE_PARTY_SCENT_STRENGTHS, sizeof g_party.scent_strengths);
    g_party.invisibility_count = p[SAVE_PARTY_INVISIBILITY];
}

/* A free slot keeps the next free index in the high word, as timeline_reset writes it. */
static void pack_event(const TimelineEvent *e, uint8_t *p)
{
    wr32(p + SAVE_EVENT_MAP_TIME, e->type ? e->map_time : e->map_time << SAVED_FREE_INDEX_SHIFT);
    p[SAVE_EVENT_TYPE] = e->type;
    p[SAVE_EVENT_PRIORITY] = e->priority;
    wr16(p + SAVE_EVENT_DATA_B, e->b);
    wr16(p + SAVE_EVENT_DATA_C, e->c);
}

/* Reads one timeline event of the timeline block. */
static void unpack_event(const uint8_t *p, TimelineEvent *e)
{
    e->map_time = be32(p + SAVE_EVENT_MAP_TIME);
    e->type = p[SAVE_EVENT_TYPE];
    e->priority = p[SAVE_EVENT_PRIORITY];
    e->b = be16(p + SAVE_EVENT_DATA_B);
    e->c = be16(p + SAVE_EVENT_DATA_C);
}

/* An active group: the group index, then the creature state bytes as they are. */
static void pack_active_group(const ActiveGroup *a, uint8_t *p)
{
    wr16(p + SAVE_GROUP_INDEX, a->group_index);
    memcpy(p + SAVE_GROUP_BYTES, &a->directions, SAVE_ACTIVE_GROUP_BYTES - SAVE_GROUP_BYTES);
}

static void unpack_active_group(const uint8_t *p, ActiveGroup *a)
{
    a->group_index = be16(p + SAVE_GROUP_INDEX);
    memcpy(&a->directions, p + SAVE_GROUP_BYTES, SAVE_ACTIVE_GROUP_BYTES - SAVE_GROUP_BYTES);
}

/* Words in one row of a champion portrait. */
#define PORTRAIT_COLUMNS ((CHAMPION_PORTRAIT_WIDTH + PIXELS_PER_WORD - 1) / PIXELS_PER_WORD)

/* A portrait in the save: per row and per 16-pixel column, one word for each bit plane. */
static void portrait_to_interleaved(const uint8_t *px, uint8_t *out)
{
    for (int y = 0; y < CHAMPION_PORTRAIT_HEIGHT; y++)
        for (int c = 0; c < PORTRAIT_COLUMNS; c++)
            for (int plane = 0; plane < BITPLANE_COUNT; plane++, out += 2) {
                uint16_t word = 0;
                for (int k = 0; k < PIXELS_PER_WORD; k++) {
                    int x = c * PIXELS_PER_WORD + k;
                    if (x < CHAMPION_PORTRAIT_WIDTH && px[y * CHAMPION_PORTRAIT_WIDTH + x] >> plane & 1)
                        word |= 1u << (PIXELS_PER_WORD - 1 - k);
                }
                wr16(out, word);
            }
}

/* A portrait from the save: the reverse of portrait_to_interleaved. */
static void portrait_from_interleaved(const uint8_t *in, uint8_t *px)
{
    memset(px, 0, CHAMPION_PORTRAIT_WIDTH * CHAMPION_PORTRAIT_HEIGHT);
    for (int y = 0; y < CHAMPION_PORTRAIT_HEIGHT; y++)
        for (int c = 0; c < PORTRAIT_COLUMNS; c++)
            for (int plane = 0; plane < BITPLANE_COUNT; plane++, in += 2) {
                uint16_t word = be16(in);
                for (int k = 0; k < PIXELS_PER_WORD; k++) {
                    int x = c * PIXELS_PER_WORD + k;
                    if (x < CHAMPION_PORTRAIT_WIDTH && word >> (PIXELS_PER_WORD - 1 - k) & 1)
                        px[y * CHAMPION_PORTRAIT_WIDTH + x] |= 1u << plane;
                }
            }
}

/* The champion block: the four champion records, then the party record. The leader load does
   not count the object in the leader hand, as in save_game. */
static void pack_champions(uint8_t *champions)
{
    for (int i = 0; i < CHAMPION_MAX_COUNT; i++)
        champion_pack(&g_champions[i], champions + i * CHAMPION_RECORD_SIZE);
    if (!g_leader_empty_handed && g_leader_index != -1) {
        Champion leader = g_champions[g_leader_index];
        leader.load -= inventory_get_object_weight(g_leader_hand_object);
        champion_pack(&leader, champions + g_leader_index * CHAMPION_RECORD_SIZE);
    }
    pack_party(champions + CHAMPION_MAX_COUNT * CHAMPION_RECORD_SIZE);
}

/* Builds the five blocks, the portraits, and the dungeon of the current game. */
static uint8_t *encode_game(uint32_t *size)
{
    uint8_t header[SAVE_HEADER_BYTES], globals[SAVE_GLOBALS_BYTES], champions[SAVE_CHAMPIONS_BYTES];
    uint8_t portraits[SAVE_PORTRAITS_BYTES];
    uint16_t group_bytes = g_max_active_group_count * SAVE_ACTIVE_GROUP_BYTES;
    uint16_t event_bytes = g_timeline.capacity * SAVE_EVENT_BYTES;
    uint16_t timeline_bytes = g_timeline.capacity * TIMELINE_ENTRY_BYTES;
    uint8_t *groups = malloc(group_bytes);
    uint8_t *events = malloc(event_bytes);
    uint8_t *timeline = malloc(timeline_bytes);
    uint32_t dungeon_size = 0;
    uint8_t *dungeon = dungeon_save(&dungeon_size);
    uint8_t *file = NULL;
    if (!groups || !events || !timeline || !dungeon)
        goto done;

    timeline_reset();
    pack_globals(globals);
    for (uint16_t i = 0; i < g_max_active_group_count; i++)
        pack_active_group(&g_active_groups[i], groups + i * SAVE_ACTIVE_GROUP_BYTES);
    pack_champions(champions);
    for (int16_t i = 0; i < g_timeline.capacity; i++) {
        pack_event(&g_timeline.events[i], events + i * SAVE_EVENT_BYTES);
        wr16(timeline + TIMELINE_ENTRY_BYTES * i, g_timeline.heap[i]);
    }
    memset(portraits, 0, sizeof portraits);
    for (uint16_t i = 0; i < g_party_champion_count; i++)
        portrait_to_interleaved(g_champions[i].runtime.portrait, portraits + i * SAVE_PORTRAIT_BYTES);

    savefile_init_header(header, g_game_id, g_dungeon_id, header_extra);
    SaveFile s = {
        .header = header,
        .block = {globals,            groups,      champions,            events,      timeline      },
        .block_size = {SAVE_GLOBALS_BYTES, group_bytes, SAVE_CHAMPIONS_BYTES, event_bytes, timeline_bytes},
        .portraits = portraits,
        .dungeon = dungeon,
        .dungeon_size = dungeon_size,
    };
    file = savefile_encode(&s, size);
done:
    free(groups);
    free(events);
    free(timeline);
    free(dungeon);
    return file;
}

/* dir/name in a buffer of PATH_SIZE bytes. */
static void make_file_path(char *path, const char *dir, const char *name)
{
    snprintf(path, PATH_SIZE, "%s/%s", dir, name);
}

/* Adds a block and its length after the data. Returns the new size, or 0 when there is no memory. */
static uint32_t append_block(uint8_t **data, uint32_t size, const uint8_t *block, size_t length)
{
    uint8_t *grown = realloc(*data, size + length + EXTENSION_LENGTH_BYTES);
    if (!grown)
        return 0;
    memcpy(grown + size, block, length);
    wr32(grown + size + length, (uint32_t)length);
    *data = grown;
    return size + (uint32_t)(length + EXTENSION_LENGTH_BYTES);
}

/* Outside vanilla, the extension blocks follow the original save data: the cheat block in the advanced mode, then
   the automap block, each with its length. The original part stays the same. Returns the new size, or 0 when there
   is no memory. */
static uint32_t append_extension(uint8_t **data, uint32_t size)
{
    uint8_t *block = NULL;
    size_t length = 0;
    if (g_session.mode == GAME_MODE_VANILLA)
        return size;
    if (g_session.mode == GAME_MODE_ADVANCED) {
        uint8_t cheats[CHEAT_SAVE_BYTES] = {0};
        memcpy(cheats, CHEAT_SAVE_MAGIC, CHEAT_MAGIC_BYTES);
        cheats[CHEAT_MAGIC_BYTES] = CHEAT_SAVE_VERSION;
        cheats[CHEAT_MAGIC_BYTES + 1] = game_get_cheats();
        size = append_block(data, size, cheats, sizeof cheats);
        if (!size)
            return 0;
    }
    if (!explored_save(&block, &length))
        return 0;
    size = append_block(data, size, block, length);
    free(block);
    return size;
}

/* The extension blocks found at the end of a save file; NULL for a missing block. */
typedef struct {
    const uint8_t *automap, *cheats;
    size_t automap_length, cheats_length;
} Extension;

/* True when the block of length bytes starts with the magic. */
static bool has_magic(const uint8_t *block, size_t length, const char *magic)
{
    return length >= strlen(magic) && !memcmp(block, magic, strlen(magic));
}

/* Strips the extension blocks from the end of a save file: a block whose length fits and whose magic is known; the
   last block of a magic counts. *size becomes the size of the original part. */
static Extension find_extension(const uint8_t *data, size_t *size)
{
    Extension found = {0};
    while (*size >= SAVE_HEADER_BYTES + EXTENSION_LENGTH_BYTES) {
        size_t length = be32(data + *size - EXTENSION_LENGTH_BYTES);
        if (length > *size - SAVE_HEADER_BYTES - EXTENSION_LENGTH_BYTES)
            break;
        const uint8_t *block = data + *size - EXTENSION_LENGTH_BYTES - length;
        if (has_magic(block, length, EXPLORED_SAVE_MAGIC)) {
            if (!found.automap) {
                found.automap = block;
                found.automap_length = length;
            }
        } else if (has_magic(block, length, CHEAT_SAVE_MAGIC)) {
            if (!found.cheats) {
                found.cheats = block;
                found.cheats_length = length;
            }
        } else {
            break;
        }
        *size -= length + EXTENSION_LENGTH_BYTES;
    }
    return found;
}

/* The cheats of a load: the bits of the cheat block in the advanced mode; none without the block, for a damaged
   block (size, version, unknown bit), and in the other modes. */
static uint8_t get_loaded_cheats(const Extension *found)
{
    if (g_session.mode != GAME_MODE_ADVANCED || !found->cheats || found->cheats_length != CHEAT_SAVE_BYTES ||
        found->cheats[CHEAT_MAGIC_BYTES] != CHEAT_SAVE_VERSION || found->cheats[CHEAT_MAGIC_BYTES + 1] & ~CHEAT_ALL)
        return 0;
    return found->cheats[CHEAT_MAGIC_BYTES + 1];
}

/* The file name of a slot: the name of slot 0 with the slot number before the extension. */
static void make_slot_name(char *out, const char *name, int16_t slot)
{
    const char *dot = strchr(name, '.');
    int stem = dot ? (int)(dot - name) : (int)strlen(name);
    if (slot)
        snprintf(out, SAVEGAME_NAME_SIZE, "%.*s%d%s", stem, name, slot + 1, name + stem);
    else
        snprintf(out, SAVEGAME_NAME_SIZE, "%s", name);
}

SavegameSlotFiles savegame_get_slot_files(int16_t adventure, int16_t slot)
{
    const Adventure *a = adventure_get(adventure);
    SavegameSlotFiles files;
    make_slot_name(files.save, a->save, slot);
    make_slot_name(files.backup, a->backup, slot);
    return files;
}

/* The level of the party map of a decoded save file; -1 when the dungeon part has no such map. */
static int16_t get_party_level(const SaveFile *s)
{
    int16_t map = be16(s->block[SAVE_BLOCK_GLOBALS] + SAVE_GLOBAL_PARTY_MAP_INDEX);
    return dungeon_get_saved_level(s->dungeon, s->dungeon_size, map);
}

/* Reads the slot slot_info of dir/name, else of dir/backup when backup is not NULL. It decodes its own copy of the file
   and loads nothing. */
static SavegameInfoResult read_info(const char *dir, const char *name, const char *backup_name,
                                    SavegameSlotInfo *slot_info)
{
    char path[PATH_SIZE];
    size_t size = 0;
    *slot_info = (SavegameSlotInfo){0};
    make_file_path(path, dir, name);
    uint8_t *data = file_read(path, &size);
    if (!data && backup_name) {
        make_file_path(path, dir, backup_name);
        data = file_read(path, &size);
    }
    if (!data)
        return SAVEGAME_INFO_EMPTY;
    find_extension(data, &size);
    SaveFile s;
    int16_t level = savefile_decode(data, (uint32_t)size, &s) ? get_party_level(&s) : -1;
    savefile_free(&s);
    free(data);
    if (level < 0)
        return SAVEGAME_INFO_DAMAGED;
    slot_info->level = level;
    slot_info->has_time = os_get_file_time(path, &slot_info->time);
    return SAVEGAME_INFO_READ;
}

SavegameInfoResult savegame_read_slot_info(const char *dir, int16_t slot, SavegameSlotInfo *slot_info)
{
    SavegameSlotFiles files = savegame_get_slot_files(g_adventure, slot);
    return read_info(dir, files.save, files.backup, slot_info);
}

SavegameInfoResult savegame_read_quick_info(const char *dir, SavegameSlotInfo *slot_info)
{
    return read_info(dir, QUICK_SAVE_FILE_NAME, NULL, slot_info);
}

bool savegame_save(const char *dir)
{
    return savegame_save_slot(dir, 0);
}

bool savegame_save_slot(const char *dir, int16_t slot)
{
    SavegameSlotFiles files = savegame_get_slot_files(g_adventure, slot);
    return savegame_save_as(dir, files.save, files.backup);
}

/* Writes size bytes to dir/name; with a backup name, the old file becomes dir/backup first. Returns true on success. */
static bool write_save_file(const char *dir, const char *name, const char *backup_name, const uint8_t *data,
                            uint32_t size)
{
    char path[PATH_SIZE], backup[PATH_SIZE];
    make_file_path(path, dir, name);
    if (backup_name) {
        make_file_path(backup, dir, backup_name);
        file_replace(path, backup);
    }
    return file_write(path, data, size);
}

bool savegame_save_as(const char *dir, const char *name, const char *backup_name)
{
    uint32_t size = 0;
    uint8_t *data = encode_game(&size);
    if (!data)
        return false;
    size = append_extension(&data, size);
    bool ok = size && write_save_file(dir, name, backup_name, data, size);
    free(data);
    if (!ok)
        return false;
    g_session.saved_game_exists = true;
    g_session.last_save_time = g_game_time;
    return true;
}

/* CEDT check before the merge: each first name is not empty and differs from the others. */
static bool are_first_names_unique(void)
{
    for (uint16_t i = 0; i < g_party_champion_count; i++) {
        if (!g_champions[i].name[0])
            return false;
        for (uint16_t k = i + 1; k < g_party_champion_count; k++)
            if (!strncmp(g_champions[i].name, g_champions[k].name, sizeof g_champions[i].name))
                return false;
    }
    return true;
}

/* The bytes of a text field after the first NUL become 0. */
static void clear_after_text(char *text, size_t size)
{
    char *end = memchr(text, 0, size);
    if (end)
        memset(end, 0, size - (size_t)(end - text));
}

/* CEDT merge: a champion of the saved game starts the new adventure rested, without objects,
   effects, or temporary experience. turn is the change of the party direction. */
static void reset_for_new_adventure(Champion *c, int16_t turn)
{
    clear_after_text(c->name, sizeof c->name);
    clear_after_text(c->title, sizeof c->title);
    if (turn) {
        c->cell = NORMALIZE(c->cell + turn);
        c->direction = NORMALIZE(c->direction + turn);
    }
    c->action_index = ACTION_NONE;
    c->symbol_step = 0;
    c->symbols[0] = 0;
    c->direction_maximum_damage_received = 0;
    c->maximum_damage_received = 0;
    c->poison_event_count = 0;
    c->enable_action_event_index = -1;
    c->hide_damage_received_event_index = -1;
    c->attributes &= CHAMPION_ATTR_MALE;
    c->wounds = 0;
    c->current_health = c->maximum_health;
    c->current_stamina = c->maximum_stamina;
    c->current_mana = c->maximum_mana;
    c->action_defense = 0;
    c->food = INITIAL_FOOD_WATER + (random16() & FOOD_WATER_RANDOM_MASK);
    c->water = INITIAL_FOOD_WATER + (random16() & FOOD_WATER_RANDOM_MASK);
    for (int s = 0; s < STAT_COUNT; s++) {
        c->statistics[s][STAT_MINIMUM] = STAT_MINIMUM_VALUE;
        c->statistics[s][STAT_CURRENT] = c->statistics[s][STAT_MAXIMUM];
    }
    c->statistics[STAT_LUCK][STAT_MINIMUM] = NEW_ADVENTURE_LUCK_MINIMUM;
    for (int k = 0; k < SKILL_COUNT; k++)
        c->skills[k].temporary_experience = 0;
    for (int s = 0; s < SLOT_COUNT; s++)
        c->slots[s] = THING_NONE;
    c->load = 0;
    c->shield_defense = 0;
}

/* The lowest cell that no champion of the party uses, else 0. */
static uint8_t free_cell(const Champion *party)
{
    for (uint8_t cell = 0; cell < CHAMPION_MAX_COUNT; cell++) {
        bool used = false;
        for (uint16_t i = 0; i < g_party_champion_count; i++)
            used |= party[i].cell == cell;
        if (!used)
            return cell;
    }
    return 0;
}

/* CEDT merge into the decoded MINI.DAT: the champions and portraits of the party, reset; the
   champion count; the header of the new adventure. All other state stays from MINI.DAT. */
static void merge_party(SaveFile *mini)
{
    Champion party[CHAMPION_MAX_COUNT];
    uint8_t *globals = mini->block[SAVE_BLOCK_GLOBALS];
    int16_t turn = be16(globals + SAVE_GLOBAL_PARTY_DIRECTION) - g_party_direction;
    memcpy(party, g_champions, sizeof party);
    for (uint16_t i = 0; i < g_party_champion_count; i++)
        reset_for_new_adventure(&party[i], turn);
    for (uint16_t i = 0; i < g_party_champion_count; i++) /* each champion gets a cell of its own */
        for (uint16_t k = 0; k < g_party_champion_count; k++)
            if (k != i && party[k].cell == party[i].cell)
                party[k].cell = free_cell(party);
    for (int i = 0; i < CHAMPION_MAX_COUNT; i++)
        champion_pack(&party[i], mini->block[SAVE_BLOCK_CHAMPIONS] + i * CHAMPION_RECORD_SIZE);
    memset(mini->portraits, 0, SAVE_PORTRAITS_BYTES);
    for (uint16_t i = 0; i < g_party_champion_count; i++)
        portrait_to_interleaved(g_champions[i].runtime.portrait, mini->portraits + i * SAVE_PORTRAIT_BYTES);
    wr16(globals + SAVE_GLOBAL_PARTY_CHAMPION_COUNT, g_party_champion_count);
    savefile_make_adventure_header(mini->header, g_game_id);
}

/* CEDT checks of the prison party: SAVEGAME_ADVENTURE_NO_CHAMPIONS, SAVEGAME_ADVENTURE_NAMES_NOT_UNIQUE, else
   SAVEGAME_ADVENTURE_MADE. */
static SavegameAdventureResult check_party(void)
{
    if (!g_party_champion_count)
        return SAVEGAME_ADVENTURE_NO_CHAMPIONS;
    if (!are_first_names_unique())
        return SAVEGAME_ADVENTURE_NAMES_NOT_UNIQUE;
    return SAVEGAME_ADVENTURE_MADE;
}

SavegameAdventureResult savegame_make_adventure(const char *dir, const char *mini_path, int16_t slot)
{
    SavegameAdventureResult refused = check_party();
    if (refused != SAVEGAME_ADVENTURE_MADE)
        return refused;
    size_t size = 0;
    uint32_t out_size = 0;
    uint8_t *data = file_read(mini_path, &size);
    uint8_t *out = NULL;
    SaveFile mini;
    if (data && savefile_decode(data, (uint32_t)size, &mini)) {
        merge_party(&mini);
        out = savefile_encode(&mini, &out_size);
        savefile_free(&mini);
    }
    SavegameSlotFiles files = savegame_get_slot_files(g_adventure, slot);
    bool ok = out && write_save_file(dir, files.save, files.backup, out, out_size);
    free(out);
    free(data);
    return ok && savegame_load_slot(dir, slot) == SAVEGAME_LOADED ? SAVEGAME_ADVENTURE_MADE : SAVEGAME_ADVENTURE_FAILED;
}

/* Allocates the timeline and the active groups for the saved counts, as timeline_init and
   group_init_active_groups do for a loaded game. CSB allocates exactly the saved count. */
static bool allocate_tables(void)
{
    uint16_t groups = g_max_active_group_count;
    if (g_adventure == ADVENTURE_DM && groups < ACTIVE_GROUP_MAX_COUNT)
        groups = ACTIVE_GROUP_MAX_COUNT;
    if (!timeline_allocate(g_timeline.capacity))
        return false;
    group_allocate_active(groups);
    return true;
}

/* The state of a decoded save file. Returns false when the dungeon part or the counts are bad. */
static bool restore_game(const SaveFile *s)
{
    if (!dungeon_load_saved(s->dungeon, s->dungeon_size))
        return false;
    unpack_globals(s->block[SAVE_BLOCK_GLOBALS]);
    discard_reset(); /* the original keeps the search maps of the game before the load */
    if (!allocate_tables())
        return false;
    for (uint16_t i = 0; i < g_max_active_group_count; i++)
        unpack_active_group(s->block[SAVE_BLOCK_GROUPS] + i * SAVE_ACTIVE_GROUP_BYTES, &g_active_groups[i]);
    /* DM load_game raises the saved maximum to at least the DM maximum; CSB keeps it */
    if (g_adventure == ADVENTURE_DM && g_max_active_group_count < ACTIVE_GROUP_MAX_COUNT)
        g_max_active_group_count = ACTIVE_GROUP_MAX_COUNT;
    for (int i = 0; i < CHAMPION_MAX_COUNT; i++)
        champion_unpack(s->block[SAVE_BLOCK_CHAMPIONS] + i * CHAMPION_RECORD_SIZE, &g_champions[i]);
    unpack_party(s->block[SAVE_BLOCK_CHAMPIONS] + CHAMPION_MAX_COUNT * CHAMPION_RECORD_SIZE);
    for (int16_t i = 0; i < g_timeline.capacity; i++) {
        unpack_event(s->block[SAVE_BLOCK_EVENTS] + i * SAVE_EVENT_BYTES, &g_timeline.events[i]);
        g_timeline.heap[i] = be16(s->block[SAVE_BLOCK_TIMELINE] + TIMELINE_ENTRY_BYTES * i);
    }
    timeline_reset();
    for (uint16_t i = 0; i < g_party_champion_count && i < CHAMPION_MAX_COUNT; i++)
        portrait_from_interleaved(s->portraits + i * SAVE_PORTRAIT_BYTES, g_champions[i].runtime.portrait);
    g_game_id = savefile_get_game_id(s->header);
    g_dungeon_id = savefile_get_dungeon_id(s->header);
    memcpy(header_extra, savefile_get_header_extra(s->header), savefile_get_extra_size());
    g_session.party_dead = false;
    g_session.party_sleeping = false;
    g_session.saved_game_exists = true;
    return true;
}

SavegameResult savegame_load(const char *dir)
{
    return savegame_load_slot(dir, 0);
}

SavegameResult savegame_load_slot(const char *dir, int16_t slot)
{
    SavegameSlotFiles files = savegame_get_slot_files(g_adventure, slot);
    return savegame_load_from(dir, files.save, files.backup);
}

/* Puts back the game of a snapshot after a failed load. The saved-game flags stay. The current
   map is selected again, because the dungeon has new buffers. */
static void restore_snapshot(const uint8_t *data, uint32_t size)
{
    bool exists = g_session.saved_game_exists;
    bool dead = g_session.party_dead;
    int16_t current = g_current_map_index;
    uint32_t loaded = g_session.last_load_time;
    SaveFile s;
    if (savefile_decode(data, size, &s))
        restore_game(&s);
    savefile_free(&s);
    map_reset();
    if (current >= 0)
        map_set_current_map(current);
    g_session.saved_game_exists = exists;
    g_session.party_dead = dead;
    g_session.last_load_time = loaded;
}

/* The game of a decoded save file with its extension blocks: the automap (else none) and the cheats. After a failed
   restore the game before comes back. Returns false when restore_game refuses the file. */
static bool load_decoded(const SaveFile *s, const Extension *found)
{
    uint32_t snapshot_size = 0;
    /* restore_game frees the running dungeon before it checks the dungeon part */
    uint8_t *snapshot = g_dungeon_header ? encode_game(&snapshot_size) : NULL;
    bool ok = restore_game(s);
    if (!ok && snapshot)
        restore_snapshot(snapshot, snapshot_size);
    free(snapshot);
    if (ok && (!found->automap || !explored_load(found->automap, found->automap_length)))
        explored_reset();
    if (ok)
        game_set_cheats(get_loaded_cheats(found));
    return ok;
}

SavegameResult savegame_load_from(const char *dir, const char *name, const char *backup_name)
{
    char path[PATH_SIZE], backup[PATH_SIZE];
    size_t size = 0;
    make_file_path(path, dir, name);
    if (backup_name)
        make_file_path(backup, dir, backup_name);
    uint8_t *data = file_read(path, &size);
    bool from_backup = !data && backup_name;
    if (from_backup)
        data = file_read(backup, &size);
    if (!data)
        return SAVEGAME_MISSING;
    Extension found = find_extension(data, &size);
    SaveFile s;
    bool ok = savefile_decode(data, (uint32_t)size, &s) && load_decoded(&s, &found);
    savefile_free(&s);
    free(data);
    if (!ok)
        return SAVEGAME_DAMAGED;
    if (from_backup)
        file_replace(backup, path);
    return SAVEGAME_LOADED;
}

void savegame_forget(void)
{
    g_session.saved_game_exists = false;
}

SavegameAdventureResult savegame_start_adventure(const char *mini_path)
{
    SavegameAdventureResult refused = check_party();
    if (refused != SAVEGAME_ADVENTURE_MADE)
        return refused;
    size_t size = 0;
    uint8_t *data = file_read(mini_path, &size);
    uint8_t cheats = game_get_cheats();
    SaveFile mini;
    bool ok = savefile_decode(data, (uint32_t)size, &mini);
    if (ok) {
        merge_party(&mini);
        ok = load_decoded(&mini, &(Extension){0});
    }
    if (ok)
        game_set_cheats(cheats); /* the same play session keeps its cheats */
    savefile_free(&mini);
    free(data);
    if (!ok)
        return SAVEGAME_ADVENTURE_FAILED;
    savegame_forget(); /* no file: the endgame offers no restart */
    return SAVEGAME_ADVENTURE_MADE;
}

bool savegame_quick_save(const char *dir)
{
    bool exists = g_session.saved_game_exists;
    uint32_t saved = g_session.last_save_time;
    bool ok = savegame_save_as(dir, QUICK_SAVE_FILE_NAME, NULL);
    g_session.saved_game_exists = exists;
    g_session.last_save_time = saved;
    return ok;
}

SavegameResult savegame_quick_load(const char *dir)
{
    bool exists = g_session.saved_game_exists;
    SavegameResult result = savegame_load_from(dir, QUICK_SAVE_FILE_NAME, NULL);
    if (result == SAVEGAME_LOADED) {
        g_session.saved_game_exists = exists;
        g_session.last_save_time = 0;
        g_session.last_load_time = 0;
    }
    return result;
}
