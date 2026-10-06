#ifndef CHAMPION_H
#define CHAMPION_H
#include <stdint.h>
#include "game.h"
#include "tables.h"

/* Champion rules of KAOS (CHAMPION.C): docs/rules-champions.md. */

#define CHAMPION_RECORD_SIZE 0x140
#define ACTION_NONE 255
#define CHAMPION_SYMBOL_COUNT 4
#define FOOD_WATER_MINIMUM (-1024)
#define CHAMPION_COLORS 0x100ce0 /* KAOS g_champion_colors[4] */

enum {
    SLOT_HAND_READY, SLOT_HAND_ACTION, SLOT_HEAD, SLOT_TORSO, SLOT_LEGS, SLOT_FEET,
    SLOT_POUCH_2, SLOT_QUIVER_LINE2_1, SLOT_QUIVER_LINE1_2, SLOT_QUIVER_LINE2_2, SLOT_NECK, SLOT_POUCH_1,
    SLOT_QUIVER_LINE1_1, SLOT_BACKPACK_FIRST, SLOT_COUNT = 30, SLOT_CHEST_FIRST = 30, SLOT_CHEST_COUNT = 8,
};
enum { SKILL_IGNORE_OBJECT_MODIFIERS = 0x4000, SKILL_IGNORE_TEMPORARY_EXPERIENCE = 0x8000 };
enum { WOUND_READY_HAND = 1, WOUND_ACTION_HAND = 2, WOUND_HEAD = 4, WOUND_TORSO = 8, WOUND_LEGS = 0x10, WOUND_FEET = 0x20 };
enum { WOUND_DEFENSE_USE_SHARP = 0x8000 };
enum { ATTACK_RAW, ATTACK_FIRE, ATTACK_SELF, ATTACK_BLUNT, ATTACK_SHARP, ATTACK_MAGIC, ATTACK_PSYCHIC, ATTACK_LIGHTNING };
enum { PANEL_RESURRECT = 0xa0, PANEL_REINCARNATE = 0xa1, PANEL_CANCEL = 0xa2 };
enum { CHAMPION_ATTR_MALE = 0x0010 };
/* Champion attribute redraw flags (Champion.attributes). */
enum {
    CHAMPION_ATTR_ACTION_DISABLED = 0x0008,
    CHAMPION_ATTR_NAME_TITLE = 0x0080,
    CHAMPION_ATTR_STATISTICS = 0x0100,
    CHAMPION_ATTR_LOAD = 0x0200,
    CHAMPION_ATTR_ICON = 0x0400,
    CHAMPION_ATTR_PANEL = 0x0800,
    CHAMPION_ATTR_STATUS_BOX = 0x1000,
    CHAMPION_ATTR_WOUNDS = 0x2000,
    CHAMPION_ATTR_VIEWPORT = 0x4000,
    CHAMPION_ATTR_ACTION_HAND = 0x8000,
    CHAMPION_ATTR_REDRAW_MASK = 0xff80,      /* all redraw flags */
    CHAMPION_ATTR_KEEP_MASK = 0x007f,        /* flags that stay after a redraw */
    CHAMPION_ATTR_INVENTORY_ALL = 0x5f80,    /* everything the inventory shows */
};

#define CHAMPION_PORTRAIT_WIDTH 32   /* g_portrait_width */
#define CHAMPION_PORTRAIT_HEIGHT 29  /* g_portrait_height */
/* The portrait bitmap of each champion (the portrait bitmap pointer at champion +0x114). */
extern uint8_t g_champion_portraits[CHAMPION_MAX_COUNT][CHAMPION_PORTRAIT_WIDTH * CHAMPION_PORTRAIT_HEIGHT];
/* Copies portrait portrait_index of graphic 26 into the portrait of the champion. */
void champion_set_portrait(int16_t champion_index, int16_t portrait_index);
extern int16_t g_candidate_champion_ordinal;   /* 0: no candidate */
extern int16_t g_champion_pending_damage[CHAMPION_MAX_COUNT];
extern uint16_t g_champion_pending_wounds[CHAMPION_MAX_COUNT];
extern int16_t g_champion_skill_level_up[CHAMPION_MAX_COUNT][SKILL_BASE_COUNT];
/* Champions that died in the tick, one bit per champion index: the user interface releases them after the tick. */
extern uint16_t g_dead_champions;

/* Original 0x140-byte big-endian record (save files, emulator tests). */
void champion_pack(const Champion *c, uint8_t *out);
void champion_unpack(const uint8_t *in, Champion *c);

void champion_add_candidate_to_party(int16_t portrait_index);
void champion_click_in_resurrect_reincarnate_panel(int16_t command);
void champion_resurrect(uint16_t champion_index);
int16_t champion_get_index_in_cell(int16_t cell);
void command_set_leader(int16_t champion_index);
void champion_wake_up(void);

int16_t champion_get_skill_level(int16_t champion_index, uint16_t skill_and_flags);
void champion_add_skill_experience(uint16_t champion_index, uint16_t skill, uint16_t experience);
uint16_t champion_get_maximum_load(const Champion *champion);
int16_t champion_get_movement_ticks(const Champion *champion);
int16_t champion_get_dexterity(const Champion *champion);
int16_t champion_get_strength(int16_t champion_index, int16_t slot);
int16_t champion_get_wound_defense(int16_t champion_index, uint16_t slot_and_flags);

int16_t champion_add_pending_damage_and_wounds(int16_t champion_index, int16_t attack, uint16_t allowed_wounds, int16_t attack_type);
void champion_apply_and_draw_pending_damage_and_wounds(void);
void champion_kill(uint16_t champion_index);
void champion_drop_all_objects(uint16_t champion_index);
void champion_poison(int16_t champion_index, uint16_t attack);
void champion_unpoison(int16_t champion_index);
int16_t champion_damage_all(uint16_t attack, uint16_t allowed_wounds, int16_t attack_type);
void champion_decrement_stamina(int16_t champion_index, int16_t decrement);
void champion_fall_damage(int16_t use_rope);

void champion_delete_scent(uint16_t scent_index);
void champion_apply_time_effects(void);

#endif
