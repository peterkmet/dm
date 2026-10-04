#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "savegame.h"
#include "champion.h"
#include "dungeon.h"
#include "ftl.h"
#include "game.h"
#include "graphics.h"
#include "ids.h"
#include "inventory.h"
#include "random.h"
#include "recruit.h"
#include "savefile.h"
#include "ui.h"

#define PORTRAIT_SIZE 0x102958        /* g_portrait_width, g_portrait_height */
#define MIN_ACTIVE_GROUP_COUNT 110    /* load_game raises the saved maximum to at least this */
#define BITPLANE_COUNT 4
#define PIXELS_PER_WORD 16
#define PATH_SIZE 1024

/* Block 0: byte offsets of the globals. */
enum {
    G_GAME_TIME = 0, G_RANDOM_SEED = 4, G_LEADER_HAND_OBJECT = 8, G_PARTY_CHAMPION_COUNT = 10, G_PARTY_MAP_X = 12,
    G_PARTY_MAP_Y = 14, G_PARTY_DIRECTION = 16, G_PARTY_MAP_INDEX = 18, G_LEADER_INDEX = 20, G_MAGIC_CASTER = 22,
    G_TIMELINE_COUNT = 24, G_FIRST_UNUSED_EVENT = 26, G_EVENT_MAX_COUNT = 28, G_ACTIVE_GROUP_COUNT = 30,
    G_LAST_CREATURE_ATTACK_TIME = 32, G_LAST_PARTY_MOVEMENT_TIME = 36, G_DISABLED_MOVEMENT_TICKS = 40,
    G_PROJECTILE_DISABLED_MOVEMENT_TICKS = 42, G_LAST_PROJECTILE_DIRECTION = 44, G_MAX_ACTIVE_GROUP_COUNT = 46,
};
/* Party record (0x101ed4) offsets. */
enum {
    P_MAGICAL_LIGHT = 0, P_THIEVES_EYE = 2, P_FOOTPRINTS = 3, P_SHIELD = 4, P_FIRE_SHIELD = 6, P_SPELL_SHIELD = 8,
    P_SCENT_COUNT = 10, P_FREEZE_LIFE = 11, P_FIRST_SCENT = 12, P_LAST_SCENT = 13, P_SCENTS = 14, P_SCENT_STRENGTHS = 62,
    P_INVISIBILITY = 86,
};
/* Timeline event record offsets. */
enum { E_MAP_TIME = 0, E_TYPE = 4, E_PRIORITY = 5, E_B = 6, E_C = 8 };
/* Active group record offsets: group index, then 14 bytes in the order of ActiveGroup. */
enum { A_GROUP_INDEX = 0, A_BYTES = 2 };

int16_t g_saved_game_exists;
uint32_t g_last_save_game_time, g_last_load_game_time;
static uint8_t header_extra[SAVE_HEADER_EXTRA_BYTES];   /* g_save_header_extra */

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] << 8 | p[1]); }
static uint32_t rd32(const uint8_t *p) { return (uint32_t)rd16(p) << 16 | rd16(p + 2); }
static void wr16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static void wr32(uint8_t *p, uint32_t v) { wr16(p, (uint16_t)(v >> 16)); wr16(p + 2, (uint16_t)v); }

static void pack_globals(uint8_t *g)
{
    memset(g, 0, SAVE_GLOBALS_BYTES);
    wr32(g + G_GAME_TIME, g_game_time);
    wr32(g + G_RANDOM_SEED, g_random_seed);
    wr16(g + G_LEADER_HAND_OBJECT, g_leader_hand_object);
    wr16(g + G_PARTY_CHAMPION_COUNT, g_party_champion_count);
    wr16(g + G_PARTY_MAP_X, (uint16_t)g_party_map_x);
    wr16(g + G_PARTY_MAP_Y, (uint16_t)g_party_map_y);
    wr16(g + G_PARTY_DIRECTION, (uint16_t)g_party_direction);
    wr16(g + G_PARTY_MAP_INDEX, (uint16_t)g_party_map_index);
    wr16(g + G_LEADER_INDEX, (uint16_t)g_leader_index);
    wr16(g + G_MAGIC_CASTER, (uint16_t)g_magic_caster_champion_index);
    wr16(g + G_TIMELINE_COUNT, g_timeline_count);
    wr16(g + G_FIRST_UNUSED_EVENT, (uint16_t)g_first_unused_event_index);
    wr16(g + G_EVENT_MAX_COUNT, (uint16_t)g_event_max_count);
    wr16(g + G_ACTIVE_GROUP_COUNT, g_active_group_count);
    wr32(g + G_LAST_CREATURE_ATTACK_TIME, (uint32_t)g_last_creature_attack_time);
    wr32(g + G_LAST_PARTY_MOVEMENT_TIME, (uint32_t)g_last_party_movement_time);
    wr16(g + G_DISABLED_MOVEMENT_TICKS, (uint16_t)g_disabled_movement_ticks);
    wr16(g + G_PROJECTILE_DISABLED_MOVEMENT_TICKS, (uint16_t)g_projectile_disabled_movement_ticks);
    wr16(g + G_LAST_PROJECTILE_DIRECTION, (uint16_t)g_last_projectile_disabled_movement_direction);
    wr16(g + G_MAX_ACTIVE_GROUP_COUNT, g_max_active_group_count);
}

static void unpack_globals(const uint8_t *g)
{
    g_game_time = g_last_load_game_time = rd32(g + G_GAME_TIME);
    g_random_seed = rd32(g + G_RANDOM_SEED);
    g_leader_hand_object = rd16(g + G_LEADER_HAND_OBJECT);
    g_party_champion_count = rd16(g + G_PARTY_CHAMPION_COUNT);
    g_party_map_x = (int16_t)rd16(g + G_PARTY_MAP_X);
    g_party_map_y = (int16_t)rd16(g + G_PARTY_MAP_Y);
    g_party_direction = (int16_t)rd16(g + G_PARTY_DIRECTION);
    g_party_map_index = (int16_t)rd16(g + G_PARTY_MAP_INDEX);
    g_leader_index = (int16_t)rd16(g + G_LEADER_INDEX);
    g_magic_caster_champion_index = (int16_t)rd16(g + G_MAGIC_CASTER);
    g_timeline_count = rd16(g + G_TIMELINE_COUNT);
    g_first_unused_event_index = (int16_t)rd16(g + G_FIRST_UNUSED_EVENT);
    g_event_max_count = (int16_t)rd16(g + G_EVENT_MAX_COUNT);
    g_active_group_count = rd16(g + G_ACTIVE_GROUP_COUNT);
    g_last_creature_attack_time = (int32_t)rd32(g + G_LAST_CREATURE_ATTACK_TIME);
    g_last_party_movement_time = (int32_t)rd32(g + G_LAST_PARTY_MOVEMENT_TIME);
    g_disabled_movement_ticks = (int16_t)rd16(g + G_DISABLED_MOVEMENT_TICKS);
    g_projectile_disabled_movement_ticks = (int16_t)rd16(g + G_PROJECTILE_DISABLED_MOVEMENT_TICKS);
    g_last_projectile_disabled_movement_direction = (int16_t)rd16(g + G_LAST_PROJECTILE_DIRECTION);
    g_max_active_group_count = rd16(g + G_MAX_ACTIVE_GROUP_COUNT);
}

/* The 128 bytes from the party record; the bytes after the record stay 0. */
static void pack_party(uint8_t *p)
{
    memset(p, 0, SAVE_PARTY_BYTES);
    wr16(p + P_MAGICAL_LIGHT, (uint16_t)g_party.magical_light_amount);
    p[P_THIEVES_EYE] = g_party.event73_count_thieves_eye;
    p[P_FOOTPRINTS] = g_party.event79_count_footprints;
    wr16(p + P_SHIELD, (uint16_t)g_party.shield_defense);
    wr16(p + P_FIRE_SHIELD, (uint16_t)g_party.fire_shield_defense);
    wr16(p + P_SPELL_SHIELD, (uint16_t)g_party.spell_shield_defense);
    p[P_SCENT_COUNT] = g_party.scent_count;
    p[P_FREEZE_LIFE] = g_party.freeze_life_ticks;
    p[P_FIRST_SCENT] = g_party.first_scent_index;
    p[P_LAST_SCENT] = g_party.last_scent_index;
    for (int i = 0; i < 24; i++) wr16(p + P_SCENTS + 2 * i, g_party.scents[i]);
    memcpy(p + P_SCENT_STRENGTHS, g_party.scent_strengths, sizeof g_party.scent_strengths);
    p[P_INVISIBILITY] = g_party.event71_count_invisibility;
}

static void unpack_party(const uint8_t *p)
{
    memset(&g_party, 0, sizeof g_party);
    g_party.magical_light_amount = (int16_t)rd16(p + P_MAGICAL_LIGHT);
    g_party.event73_count_thieves_eye = p[P_THIEVES_EYE];
    g_party.event79_count_footprints = p[P_FOOTPRINTS];
    g_party.shield_defense = (int16_t)rd16(p + P_SHIELD);
    g_party.fire_shield_defense = (int16_t)rd16(p + P_FIRE_SHIELD);
    g_party.spell_shield_defense = (int16_t)rd16(p + P_SPELL_SHIELD);
    g_party.scent_count = p[P_SCENT_COUNT];
    g_party.freeze_life_ticks = p[P_FREEZE_LIFE];
    g_party.first_scent_index = p[P_FIRST_SCENT];
    g_party.last_scent_index = p[P_LAST_SCENT];
    for (int i = 0; i < 24; i++) g_party.scents[i] = rd16(p + P_SCENTS + 2 * i);
    memcpy(g_party.scent_strengths, p + P_SCENT_STRENGTHS, sizeof g_party.scent_strengths);
    g_party.event71_count_invisibility = p[P_INVISIBILITY];
}

/* A free slot keeps the next free index in the high word, as timeline_reset writes it. */
static void pack_event(const TimelineEvent *e, uint8_t *p)
{
    wr32(p + E_MAP_TIME, e->type ? e->map_time : e->map_time << 16);
    p[E_TYPE] = e->type;
    p[E_PRIORITY] = e->priority;
    wr16(p + E_B, e->b);
    wr16(p + E_C, e->c);
}

static void unpack_event(const uint8_t *p, TimelineEvent *e)
{
    e->map_time = rd32(p + E_MAP_TIME);
    e->type = p[E_TYPE];
    e->priority = p[E_PRIORITY];
    e->b = rd16(p + E_B);
    e->c = rd16(p + E_C);
}

static void pack_active_group(const ActiveGroup *a, uint8_t *p)
{
    wr16(p + A_GROUP_INDEX, (uint16_t)a->group_index);
    memcpy(p + A_BYTES, &a->directions, SAVE_ACTIVE_GROUP_BYTES - A_BYTES);
}

static void unpack_active_group(const uint8_t *p, ActiveGroup *a)
{
    a->group_index = (int16_t)rd16(p + A_GROUP_INDEX);
    memcpy(&a->directions, p + A_BYTES, SAVE_ACTIVE_GROUP_BYTES - A_BYTES);
}

/* A portrait of graphic 26 in the interleaved form of champion_portraits_to_interleaved: per row
   and per 16-pixel column, one word for each bit plane. */
static void portrait_interleaved(int16_t portrait_index, uint8_t *out)
{
    const Bitmap *sheet = gfx_bitmap(GFX_CHAMPION_PORTRAITS);
    int16_t w = kaos_s16(PORTRAIT_SIZE), h = kaos_s16(PORTRAIT_SIZE + 2), x0, y0;
    memset(out, 0, SAVE_PORTRAIT_BYTES);
    if (!sheet) return;
    recruit_portrait_origin(portrait_index, &x0, &y0);
    int16_t columns = (int16_t)((w + PIXELS_PER_WORD - 1) / PIXELS_PER_WORD);
    for (int16_t y = 0; y < h; y++)
        for (int16_t c = 0; c < columns; c++)
            for (int plane = 0; plane < BITPLANE_COUNT; plane++, out += 2) {
                uint16_t word = 0;
                for (int16_t k = 0; k < PIXELS_PER_WORD; k++) {
                    int16_t x = (int16_t)(c * PIXELS_PER_WORD + k), sx = (int16_t)(x0 + x), sy = (int16_t)(y0 + y);
                    if (x < w && sx < sheet->w && sy < sheet->h && sheet->px[sy * sheet->w + sx] >> plane & 1)
                        word |= (uint16_t)(1u << (PIXELS_PER_WORD - 1 - k));
                }
                wr16(out, word);
            }
}

/* The index of the portrait of graphic 26 that has these bitmap bytes, or 0. */
static int16_t portrait_index_of(const uint8_t *bytes)
{
    const Bitmap *sheet = gfx_bitmap(GFX_CHAMPION_PORTRAITS);
    int16_t w = kaos_s16(PORTRAIT_SIZE), h = kaos_s16(PORTRAIT_SIZE + 2);
    if (!sheet || w <= 0 || h <= 0) return 0;
    uint8_t candidate[SAVE_PORTRAIT_BYTES];
    int16_t count = (int16_t)((sheet->w / w) * (sheet->h / h));
    for (int16_t i = 0; i < count; i++) {
        portrait_interleaved(i, candidate);
        if (!memcmp(candidate, bytes, SAVE_PORTRAIT_BYTES)) return i;
    }
    return 0;
}

/* Builds the five blocks, the portraits, and the dungeon of the current game. The leader load
   does not count the object in the leader hand, as in save_game. */
static uint8_t *encode_game(uint32_t *size)
{
    uint8_t header[SAVE_HEADER_BYTES], globals[SAVE_GLOBALS_BYTES], champions[SAVE_CHAMPIONS_BYTES];
    uint8_t portraits[SAVE_PORTRAITS_BYTES];
    uint16_t group_bytes = (uint16_t)(g_max_active_group_count * SAVE_ACTIVE_GROUP_BYTES);
    uint16_t event_bytes = (uint16_t)(g_event_max_count * SAVE_EVENT_BYTES), timeline_bytes = (uint16_t)(g_event_max_count * 2);
    uint8_t *groups = malloc(group_bytes), *events = malloc(event_bytes), *timeline = malloc(timeline_bytes);
    uint32_t dungeon_size = 0;
    uint8_t *dungeon = dungeon_save(&dungeon_size), *file = NULL;
    if (!groups || !events || !timeline || !dungeon) goto done;

    timeline_reset();
    pack_globals(globals);
    for (uint16_t i = 0; i < g_max_active_group_count; i++) pack_active_group(&g_active_groups[i], groups + i * SAVE_ACTIVE_GROUP_BYTES);
    for (int i = 0; i < CHAMPION_MAX_COUNT; i++) champion_pack(&g_champions[i], champions + i * CHAMPION_RECORD_SIZE);
    if (!g_leader_empty_handed && g_leader_index != -1) {
        Champion leader = g_champions[g_leader_index];
        leader.load -= get_object_weight(g_leader_hand_object);
        champion_pack(&leader, champions + g_leader_index * CHAMPION_RECORD_SIZE);
    }
    pack_party(champions + CHAMPION_MAX_COUNT * CHAMPION_RECORD_SIZE);
    for (int16_t i = 0; i < g_event_max_count; i++) {
        pack_event(&g_timeline_events[i], events + i * SAVE_EVENT_BYTES);
        wr16(timeline + 2 * i, g_timeline[i]);
    }
    memset(portraits, 0, sizeof portraits);
    for (uint16_t i = 0; i < g_party_champion_count; i++)
        portrait_interleaved(g_champion_portrait_indices[i], portraits + i * SAVE_PORTRAIT_BYTES);

    save_header_init(header, g_game_id, g_dungeon_id, header_extra);
    SaveFile s = {
        .header = header,
        .block = { globals, groups, champions, events, timeline },
        .block_size = { SAVE_GLOBALS_BYTES, group_bytes, SAVE_CHAMPIONS_BYTES, event_bytes, timeline_bytes },
        .portraits = portraits, .dungeon = dungeon, .dungeon_size = dungeon_size,
    };
    file = save_encode(&s, size);
done:
    free(groups);
    free(events);
    free(timeline);
    free(dungeon);
    return file;
}

static void file_path(char *path, const char *dir, const char *name)
{
    snprintf(path, PATH_SIZE, "%s/%s", dir, name);
}

int16_t savegame_save(const char *dir)
{
    char path[PATH_SIZE], backup[PATH_SIZE];
    uint32_t size = 0;
    uint8_t *data = encode_game(&size);
    if (!data) return 0;
    file_path(path, dir, SAVE_FILE_NAME);
    file_path(backup, dir, SAVE_BACKUP_FILE_NAME);
    remove(backup);
    rename(path, backup);
    FILE *f = fopen(path, "wb");
    int16_t ok = f && fwrite(data, 1, size, f) == size;
    if (f && fclose(f)) ok = 0;
    free(data);
    if (!ok) return 0;
    g_saved_game_exists = 1;
    g_last_save_game_time = g_game_time;
    return 1;
}

/* Allocates the timeline and the active groups for the saved counts, as timeline_init and
   group_init_active_groups do for a loaded game. */
static int allocate_tables(void)
{
    uint16_t groups = g_max_active_group_count < MIN_ACTIVE_GROUP_COUNT ? MIN_ACTIVE_GROUP_COUNT : g_max_active_group_count;
    if (g_event_max_count <= 0) return 0;
    free(g_timeline_events);
    free(g_timeline);
    free(g_active_groups);
    g_timeline_events = calloc((size_t)g_event_max_count, sizeof *g_timeline_events);
    g_timeline = calloc((size_t)g_event_max_count, sizeof *g_timeline);
    g_active_groups = malloc(groups * sizeof *g_active_groups);
    if (!g_timeline_events || !g_timeline || !g_active_groups) return 0;
    for (uint16_t i = 0; i < groups; i++) g_active_groups[i].group_index = -1;
    return 1;
}

/* The state of a decoded save file. Returns 0 when the dungeon part or the counts are bad. */
static int restore_game(const SaveFile *s)
{
    if (!dungeon_load_saved(s->dungeon, s->dungeon_size)) return 0;
    unpack_globals(s->block[SAVE_BLOCK_GLOBALS]);
    if (!allocate_tables()) return 0;
    for (uint16_t i = 0; i < g_max_active_group_count; i++)
        unpack_active_group(s->block[SAVE_BLOCK_GROUPS] + i * SAVE_ACTIVE_GROUP_BYTES, &g_active_groups[i]);
    if (g_max_active_group_count < MIN_ACTIVE_GROUP_COUNT) g_max_active_group_count = MIN_ACTIVE_GROUP_COUNT;
    for (int i = 0; i < CHAMPION_MAX_COUNT; i++) champion_unpack(s->block[SAVE_BLOCK_CHAMPIONS] + i * CHAMPION_RECORD_SIZE, &g_champions[i]);
    unpack_party(s->block[SAVE_BLOCK_CHAMPIONS] + CHAMPION_MAX_COUNT * CHAMPION_RECORD_SIZE);
    for (int16_t i = 0; i < g_event_max_count; i++) {
        unpack_event(s->block[SAVE_BLOCK_EVENTS] + i * SAVE_EVENT_BYTES, &g_timeline_events[i]);
        g_timeline[i] = rd16(s->block[SAVE_BLOCK_TIMELINE] + 2 * i);
    }
    timeline_reset();
    for (uint16_t i = 0; i < g_party_champion_count && i < CHAMPION_MAX_COUNT; i++)
        g_champion_portrait_indices[i] = portrait_index_of(s->portraits + i * SAVE_PORTRAIT_BYTES);
    g_game_id = save_header_game_id(s->header);
    g_dungeon_id = save_header_dungeon_id(s->header);
    memcpy(header_extra, save_header_extra(s->header), SAVE_HEADER_EXTRA_BYTES);
    g_party_dead = g_party_is_sleeping = 0;
    g_saved_game_exists = 1;
    return 1;
}

int16_t savegame_load(const char *dir)
{
    char path[PATH_SIZE], backup[PATH_SIZE];
    size_t size = 0;
    file_path(path, dir, SAVE_FILE_NAME);
    file_path(backup, dir, SAVE_BACKUP_FILE_NAME);
    uint8_t *data = read_file(path, &size);
    int from_backup = !data;
    if (from_backup) data = read_file(backup, &size);
    if (!data) return SAVEGAME_MISSING;
    SaveFile s;
    int ok = save_decode(data, (uint32_t)size, &s) && restore_game(&s);
    save_free(&s);
    free(data);
    if (!ok) return SAVEGAME_DAMAGED;
    if (from_backup) rename(backup, path);
    return SAVEGAME_LOADED;
}
