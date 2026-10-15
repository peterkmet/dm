#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "savegame.h"
#include "bytes.h"
#include "adventure.h"
#include "champion.h"
#include "dungeon.h"
#include "explored.h"
#include "file.h"
#include "game.h"
#include "graphics.h"
#include "ids.h"
#include "inventory.h"
#include "map.h"
#include "random.h"
#include "savefile.h"

#define TIMELINE_ENTRY_BYTES       2 /* one event index per timeline entry */
#define EXTENSION_LENGTH_BYTES     4 /* the length of the automap block, after the block */
#define NEW_ADVENTURE_LUCK_MINIMUM 10
#define SAVED_FREE_INDEX_SHIFT     16 /* a free event slot of the file keeps the next free index in the high word */

/* Party record (g_party) offsets. */
enum {
    P_MAGICAL_LIGHT = 0,
    P_THIEVES_EYE = 2,
    P_FOOTPRINTS = 3,
    P_SHIELD = 4,
    P_FIRE_SHIELD = 6,
    P_SPELL_SHIELD = 8,
    P_SCENT_COUNT = 10,
    P_FREEZE_LIFE = 11,
    P_FIRST_SCENT = 12,
    P_LAST_SCENT = 13,
    P_SCENTS = 14,
    P_SCENT_STRENGTHS = 62,
    P_INVISIBILITY = 86,
};
/* Timeline event record offsets. */
enum { E_MAP_TIME = 0, E_TYPE = 4, E_PRIORITY = 5, E_DATA_B = 6, E_DATA_C = 8 };
/* Active group record offsets: group index, then 14 bytes in the order of ActiveGroup. */
enum { A_GROUP_INDEX = 0, A_BYTES = 2 };

bool g_saved_game_exists;
uint32_t g_last_save_game_time, g_last_load_game_time;
static uint8_t header_extra
    [SAVE_HEADER_EXTRA_BYTES]; /* the extra save header bytes of the loaded game, copied unchanged into the next save */

/* Block 0: game time, random seed, party position, leader, and the timeline and group counts. */
static void pack_globals(uint8_t *g)
{
    memset(g, 0, SAVE_GLOBALS_BYTES);
    WR32(g + G_GAME_TIME, g_game_time);
    WR32(g + G_RANDOM_SEED, g_random_seed);
    WR16(g + G_LEADER_HAND_OBJECT, g_leader_hand_object);
    WR16(g + G_PARTY_CHAMPION_COUNT, g_party_champion_count);
    WR16(g + G_PARTY_MAP_X, g_party_map_x);
    WR16(g + G_PARTY_MAP_Y, g_party_map_y);
    WR16(g + G_PARTY_DIRECTION, g_party_direction);
    WR16(g + G_PARTY_MAP_INDEX, g_party_map_index);
    WR16(g + G_LEADER_INDEX, g_leader_index);
    WR16(g + G_MAGIC_CASTER, g_magic_caster_champion_index);
    WR16(g + G_TIMELINE_COUNT, g_timeline_count);
    WR16(g + G_FIRST_UNUSED_EVENT, g_first_unused_event_index);
    WR16(g + G_EVENT_MAX_COUNT, g_event_max_count);
    WR16(g + G_ACTIVE_GROUP_COUNT, g_active_group_count);
    WR32(g + G_LAST_CREATURE_ATTACK_TIME, (uint32_t)g_last_creature_attack_time);
    WR32(g + G_LAST_PARTY_MOVEMENT_TIME, (uint32_t)g_last_party_movement_time);
    WR16(g + G_DISABLED_MOVEMENT_TICKS, g_disabled_movement_ticks);
    WR16(g + G_PROJECTILE_DISABLED_MOVEMENT_TICKS, g_projectile_disabled_movement_ticks);
    WR16(g + G_LAST_PROJECTILE_DIRECTION, g_last_projectile_disabled_movement_direction);
    WR16(g + G_MAX_ACTIVE_GROUP_COUNT, g_max_active_group_count);
}

/* Block 0 back into the globals; the game time also becomes the time of the last load. */
static void unpack_globals(const uint8_t *g)
{
    g_last_load_game_time = BE32(g + G_GAME_TIME);
    g_game_time = g_last_load_game_time;
    g_random_seed = BE32(g + G_RANDOM_SEED);
    g_leader_hand_object = BE16(g + G_LEADER_HAND_OBJECT);
    g_party_champion_count = BE16(g + G_PARTY_CHAMPION_COUNT);
    g_party_map_x = BE16(g + G_PARTY_MAP_X);
    g_party_map_y = BE16(g + G_PARTY_MAP_Y);
    g_party_direction = BE16(g + G_PARTY_DIRECTION);
    g_party_map_index = BE16(g + G_PARTY_MAP_INDEX);
    g_leader_index = BE16(g + G_LEADER_INDEX);
    g_magic_caster_champion_index = BE16(g + G_MAGIC_CASTER);
    g_timeline_count = BE16(g + G_TIMELINE_COUNT);
    g_first_unused_event_index = BE16(g + G_FIRST_UNUSED_EVENT);
    g_event_max_count = BE16(g + G_EVENT_MAX_COUNT);
    g_active_group_count = BE16(g + G_ACTIVE_GROUP_COUNT);
    g_last_creature_attack_time = BE32(g + G_LAST_CREATURE_ATTACK_TIME);
    g_last_party_movement_time = BE32(g + G_LAST_PARTY_MOVEMENT_TIME);
    g_disabled_movement_ticks = BE16(g + G_DISABLED_MOVEMENT_TICKS);
    g_projectile_disabled_movement_ticks = BE16(g + G_PROJECTILE_DISABLED_MOVEMENT_TICKS);
    g_last_projectile_disabled_movement_direction = BE16(g + G_LAST_PROJECTILE_DIRECTION);
    g_max_active_group_count = BE16(g + G_MAX_ACTIVE_GROUP_COUNT);
}

/* The 128 bytes from the party record; the bytes after the record stay 0. */
static void pack_party(uint8_t *p)
{
    memset(p, 0, SAVE_PARTY_BYTES);
    WR16(p + P_MAGICAL_LIGHT, g_party.magical_light_amount);
    p[P_THIEVES_EYE] = g_party.thieves_eye_count;
    p[P_FOOTPRINTS] = g_party.footprints_count;
    WR16(p + P_SHIELD, g_party.shield_defense);
    WR16(p + P_FIRE_SHIELD, g_party.fire_shield_defense);
    WR16(p + P_SPELL_SHIELD, g_party.spell_shield_defense);
    p[P_SCENT_COUNT] = g_party.scent_count;
    p[P_FREEZE_LIFE] = g_party.freeze_life_ticks;
    p[P_FIRST_SCENT] = g_party.first_scent_index;
    p[P_LAST_SCENT] = g_party.last_scent_index;
    for (int i = 0; i < PARTY_SCENT_COUNT; i++)
        WR16(p + P_SCENTS + 2 * i, g_party.scents[i]);
    memcpy(p + P_SCENT_STRENGTHS, g_party.scent_strengths, sizeof g_party.scent_strengths);
    p[P_INVISIBILITY] = g_party.invisibility_count;
}

static void unpack_party(const uint8_t *p)
{
    memset(&g_party, 0, sizeof g_party);
    g_party.magical_light_amount = BE16(p + P_MAGICAL_LIGHT);
    g_party.thieves_eye_count = p[P_THIEVES_EYE];
    g_party.footprints_count = p[P_FOOTPRINTS];
    g_party.shield_defense = BE16(p + P_SHIELD);
    g_party.fire_shield_defense = BE16(p + P_FIRE_SHIELD);
    g_party.spell_shield_defense = BE16(p + P_SPELL_SHIELD);
    g_party.scent_count = p[P_SCENT_COUNT];
    g_party.freeze_life_ticks = p[P_FREEZE_LIFE];
    g_party.first_scent_index = p[P_FIRST_SCENT];
    g_party.last_scent_index = p[P_LAST_SCENT];
    for (int i = 0; i < PARTY_SCENT_COUNT; i++)
        g_party.scents[i] = BE16(p + P_SCENTS + 2 * i);
    memcpy(g_party.scent_strengths, p + P_SCENT_STRENGTHS, sizeof g_party.scent_strengths);
    g_party.invisibility_count = p[P_INVISIBILITY];
}

/* A free slot keeps the next free index in the high word, as timeline_reset writes it. */
static void pack_event(const TimelineEvent *e, uint8_t *p)
{
    WR32(p + E_MAP_TIME, e->type ? e->map_time : e->map_time << SAVED_FREE_INDEX_SHIFT);
    p[E_TYPE] = e->type;
    p[E_PRIORITY] = e->priority;
    WR16(p + E_DATA_B, e->b);
    WR16(p + E_DATA_C, e->c);
}

static void unpack_event(const uint8_t *p, TimelineEvent *e)
{
    e->map_time = BE32(p + E_MAP_TIME);
    e->type = p[E_TYPE];
    e->priority = p[E_PRIORITY];
    e->b = BE16(p + E_DATA_B);
    e->c = BE16(p + E_DATA_C);
}

/* An active group: the group index, then the creature state bytes as they are. */
static void pack_active_group(const ActiveGroup *a, uint8_t *p)
{
    WR16(p + A_GROUP_INDEX, a->group_index);
    memcpy(p + A_BYTES, &a->directions, SAVE_ACTIVE_GROUP_BYTES - A_BYTES);
}

static void unpack_active_group(const uint8_t *p, ActiveGroup *a)
{
    a->group_index = BE16(p + A_GROUP_INDEX);
    memcpy(&a->directions, p + A_BYTES, SAVE_ACTIVE_GROUP_BYTES - A_BYTES);
}

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
                WR16(out, word);
            }
}

/* A portrait from the save: the reverse of portrait_to_interleaved. */
static void portrait_from_interleaved(const uint8_t *in, uint8_t *px)
{
    memset(px, 0, CHAMPION_PORTRAIT_WIDTH * CHAMPION_PORTRAIT_HEIGHT);
    for (int y = 0; y < CHAMPION_PORTRAIT_HEIGHT; y++)
        for (int c = 0; c < PORTRAIT_COLUMNS; c++)
            for (int plane = 0; plane < BITPLANE_COUNT; plane++, in += 2) {
                uint16_t word = BE16(in);
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
    uint16_t event_bytes = g_event_max_count * SAVE_EVENT_BYTES;
    uint16_t timeline_bytes = g_event_max_count * TIMELINE_ENTRY_BYTES;
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
    for (int16_t i = 0; i < g_event_max_count; i++) {
        pack_event(&g_timeline_events[i], events + i * SAVE_EVENT_BYTES);
        WR16(timeline + TIMELINE_ENTRY_BYTES * i, g_timeline[i]);
    }
    memset(portraits, 0, sizeof portraits);
    for (uint16_t i = 0; i < g_party_champion_count; i++)
        portrait_to_interleaved(g_champion_portraits[i], portraits + i * SAVE_PORTRAIT_BYTES);

    save_header_init(header, g_game_id, g_dungeon_id, header_extra);
    SaveFile s = {
        .header = header,
        .block = {globals,            groups,      champions,            events,      timeline      },
        .block_size = {SAVE_GLOBALS_BYTES, group_bytes, SAVE_CHAMPIONS_BYTES, event_bytes, timeline_bytes},
        .portraits = portraits,
        .dungeon = dungeon,
        .dungeon_size = dungeon_size,
    };
    file = save_encode(&s, size);
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

/* Outside vanilla, the automap block and its length follow the original save data. The original
   part stays the same. Returns the new size, or 0 when there is no memory. */
static uint32_t append_automap(uint8_t **data, uint32_t size)
{
    uint8_t *block = NULL, *grown;
    size_t length = 0;
    if (g_game_mode == GAME_MODE_VANILLA)
        return size;
    if (explored_save(&block, &length))
        return 0;
    grown = realloc(*data, size + length + EXTENSION_LENGTH_BYTES);
    if (!grown) {
        free(block);
        return 0;
    }
    memcpy(grown + size, block, length);
    WR32(grown + size + length, (uint32_t)length);
    free(block);
    *data = grown;
    return size + (uint32_t)(length + EXTENSION_LENGTH_BYTES);
}

/* The automap block at the end of a save file, or NULL. *size becomes the size of the original part. */
static const uint8_t *find_automap(const uint8_t *data, size_t *size, size_t *length)
{
    if (*size < SAVE_HEADER_BYTES + EXTENSION_LENGTH_BYTES)
        return NULL;
    *length = BE32(data + *size - EXTENSION_LENGTH_BYTES);
    if (*length > *size - SAVE_HEADER_BYTES - EXTENSION_LENGTH_BYTES)
        return NULL;
    const uint8_t *block = data + *size - EXTENSION_LENGTH_BYTES - *length;
    if (*length < strlen(EXPLORED_SAVE_MAGIC) || memcmp(block, EXPLORED_SAVE_MAGIC, strlen(EXPLORED_SAVE_MAGIC)))
        return NULL;
    *size -= *length + EXTENSION_LENGTH_BYTES;
    return block;
}

int16_t savegame_save(const char *dir)
{
    return savegame_save_as(dir, adventure_save_name(), adventure_backup_name());
}

/* Writes size bytes to dir/name; with a backup name, the old file becomes dir/backup first. Returns true on success. */
static bool write_save_file(const char *dir, const char *name, const char *backup_name, const uint8_t *data,
                            uint32_t size)
{
    char path[PATH_SIZE], backup[PATH_SIZE];
    make_file_path(path, dir, name);
    if (backup_name) {
        make_file_path(backup, dir, backup_name);
        remove(backup);
        rename(path, backup);
    }
    FILE *f = fopen(path, "wb");
    bool ok = f && fwrite(data, 1, size, f) == size;
    if (f && fclose(f))
        ok = false;
    return ok;
}

bool savegame_save_as(const char *dir, const char *name, const char *backup_name)
{
    uint32_t size = 0;
    uint8_t *data = encode_game(&size);
    if (!data)
        return false;
    size = append_automap(&data, size);
    bool ok = size && write_save_file(dir, name, backup_name, data, size);
    free(data);
    if (!ok)
        return false;
    g_saved_game_exists = true;
    g_last_save_game_time = g_game_time;
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
    int16_t turn = BE16(globals + G_PARTY_DIRECTION) - g_party_direction;
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
        portrait_to_interleaved(g_champion_portraits[i], mini->portraits + i * SAVE_PORTRAIT_BYTES);
    WR16(globals + G_PARTY_CHAMPION_COUNT, g_party_champion_count);
    save_header_new_adventure(mini->header, g_game_id);
}

int16_t savegame_make_adventure(const char *dir, const char *mini_path)
{
    if (!g_party_champion_count)
        return ADVENTURE_NO_CHAMPIONS;
    if (!are_first_names_unique())
        return ADVENTURE_NAMES_NOT_UNIQUE;
    size_t size = 0;
    uint32_t out_size = 0;
    uint8_t *data = file_read(mini_path, &size);
    uint8_t *out = NULL;
    SaveFile mini;
    if (data && save_decode(data, (uint32_t)size, &mini)) {
        merge_party(&mini);
        out = save_encode(&mini, &out_size);
        save_free(&mini);
    }
    bool ok = out && write_save_file(dir, adventure_save_name(), adventure_backup_name(), out, out_size);
    free(out);
    free(data);
    return ok && savegame_load(dir) == SAVEGAME_LOADED ? ADVENTURE_MADE : ADVENTURE_FAILED;
}

/* Allocates the timeline and the active groups for the saved counts, as timeline_init and
   game_init_active_groups do for a loaded game. CSB allocates exactly the saved count. */
static bool allocate_tables(void)
{
    uint16_t groups = g_max_active_group_count;
    if (g_adventure == ADVENTURE_DM && groups < ACTIVE_GROUP_MAX_COUNT)
        groups = ACTIVE_GROUP_MAX_COUNT;
    if (g_event_max_count <= 0)
        return false;
    free(g_timeline_events);
    free(g_timeline);
    free(g_active_groups);
    g_timeline_events = calloc((size_t)g_event_max_count, sizeof *g_timeline_events);
    g_timeline = calloc((size_t)g_event_max_count, sizeof *g_timeline);
    g_active_groups = malloc(groups * sizeof *g_active_groups);
    if (!g_timeline_events || !g_timeline || !g_active_groups)
        return false;
    for (uint16_t i = 0; i < groups; i++)
        g_active_groups[i].group_index = -1;
    return true;
}

/* The state of a decoded save file. Returns false when the dungeon part or the counts are bad. */
static bool restore_game(const SaveFile *s)
{
    if (!dungeon_load_saved(s->dungeon, s->dungeon_size))
        return false;
    unpack_globals(s->block[SAVE_BLOCK_GLOBALS]);
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
    for (int16_t i = 0; i < g_event_max_count; i++) {
        unpack_event(s->block[SAVE_BLOCK_EVENTS] + i * SAVE_EVENT_BYTES, &g_timeline_events[i]);
        g_timeline[i] = BE16(s->block[SAVE_BLOCK_TIMELINE] + TIMELINE_ENTRY_BYTES * i);
    }
    timeline_reset();
    for (uint16_t i = 0; i < g_party_champion_count && i < CHAMPION_MAX_COUNT; i++)
        portrait_from_interleaved(s->portraits + i * SAVE_PORTRAIT_BYTES, g_champion_portraits[i]);
    g_game_id = save_header_game_id(s->header);
    g_dungeon_id = save_header_dungeon_id(s->header);
    memcpy(header_extra, save_header_extra(s->header), save_header_extra_size());
    g_party_dead = false;
    g_party_is_sleeping = false;
    g_saved_game_exists = true;
    return true;
}

int16_t savegame_load(const char *dir)
{
    return savegame_load_from(dir, adventure_save_name(), adventure_backup_name());
}

/* Puts back the game of a snapshot after a failed load. The saved-game flags stay. The current
   map is selected again, because the dungeon has new buffers. */
static void restore_snapshot(const uint8_t *data, uint32_t size)
{
    bool exists = g_saved_game_exists;
    bool dead = g_party_dead;
    int16_t current = g_current_map_index;
    uint32_t loaded = g_last_load_game_time;
    SaveFile s;
    if (save_decode(data, size, &s))
        restore_game(&s);
    save_free(&s);
    map_reset();
    if (current >= 0)
        map_set_current_map(current);
    g_saved_game_exists = exists;
    g_party_dead = dead;
    g_last_load_game_time = loaded;
}

int16_t savegame_load_from(const char *dir, const char *name, const char *backup_name)
{
    char path[PATH_SIZE], backup[PATH_SIZE];
    size_t size = 0;
    uint32_t snapshot_size = 0;
    make_file_path(path, dir, name);
    if (backup_name)
        make_file_path(backup, dir, backup_name);
    uint8_t *data = file_read(path, &size);
    bool from_backup = !data && backup_name;
    if (from_backup)
        data = file_read(backup, &size);
    if (!data)
        return SAVEGAME_MISSING;
    size_t automap_length = 0;
    const uint8_t *automap = find_automap(data, &size, &automap_length);
    SaveFile s;
    uint8_t *snapshot = NULL;
    bool ok = save_decode(data, (uint32_t)size, &s);
    if (ok) {
        /* restore_game frees the running dungeon before it checks the dungeon part */
        snapshot = g_dungeon_header ? encode_game(&snapshot_size) : NULL;
        ok = restore_game(&s);
        if (!ok && snapshot)
            restore_snapshot(snapshot, snapshot_size);
    }
    save_free(&s);
    if (ok && (!automap || explored_load(automap, automap_length)))
        explored_reset();
    free(snapshot);
    free(data);
    if (!ok)
        return SAVEGAME_DAMAGED;
    if (from_backup)
        rename(backup, path);
    return SAVEGAME_LOADED;
}
