/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DM_ENGINE_CHAMPION_H
#define DM_ENGINE_CHAMPION_H
/* Prefixes: champion_ */
#include "data/tables.h"
#include "engine/game.h"
#include <stdbool.h>
#include <stdint.h>

/* Champion rules of KAOS (CHAMPION.C): docs/rules/champions.md. */

#define CHAMPION_RECORD_SIZE   320
#define ACTION_NONE            255
#define CHAMPION_SYMBOL_COUNT  4
#define FOOD_WATER_MINIMUM     (-1024)
#define INITIAL_FOOD_WATER     1500 /* food and water of a new champion: this + a random byte */
#define FOOD_WATER_RANDOM_MASK 0xff
#define FOOD_WATER_STARVING    (-512) /* below this the champion starves */
#define MAXIMUM_MANA           900
/* A load above 5/8 of the maximum load is heavy. */
#define HEAVY_LOAD_NUMERATOR   5
#define HEAVY_LOAD_DENOMINATOR 8

#define SKILL_IGNORE_OBJECT_MODIFIERS     0x4000
#define SKILL_IGNORE_TEMPORARY_EXPERIENCE 0x8000
#define WOUND_READY_HAND                  1
#define WOUND_ACTION_HAND                 2
#define WOUND_HEAD                        4
#define WOUND_TORSO                       8
#define WOUND_LEGS                        0x10
#define WOUND_FEET                        0x20
#define WOUND_DEFENSE_USE_SHARP           0x8000
#define ATTACK_RAW                        0
#define ATTACK_FIRE                       1
#define ATTACK_SELF                       2
#define ATTACK_BLUNT                      3
#define ATTACK_SHARP                      4
#define ATTACK_MAGIC                      5
#define ATTACK_PSYCHIC                    6
#define ATTACK_LIGHTNING                  7
#define PANEL_RESURRECT                   0xa0
#define PANEL_REINCARNATE                 0xa1
#define PANEL_CANCEL                      0xa2
#define CHAMPION_ATTR_MALE                0x0010
/* Champion attribute redraw flags (Champion.attributes). */
#define CHAMPION_ATTR_ACTION_DISABLED     0x0008
#define CHAMPION_ATTR_NAME_TITLE          0x0080
#define CHAMPION_ATTR_STATISTICS          0x0100
#define CHAMPION_ATTR_LOAD                0x0200
#define CHAMPION_ATTR_ICON                0x0400
#define CHAMPION_ATTR_PANEL               0x0800
#define CHAMPION_ATTR_STATUS_BOX          0x1000
#define CHAMPION_ATTR_WOUNDS              0x2000
#define CHAMPION_ATTR_VIEWPORT            0x4000
#define CHAMPION_ATTR_ACTION_HAND         0x8000
#define CHAMPION_ATTR_REDRAW_MASK         0xff80 /* all redraw flags */
#define CHAMPION_ATTR_KEEP_MASK           0x007f /* flags that stay after a redraw */
#define CHAMPION_ATTR_INVENTORY_ALL       0x5f80 /* everything the inventory shows */

#define CHAMPION_PORTRAIT_WIDTH  32 /* pixels of a champion portrait */
#define CHAMPION_PORTRAIT_HEIGHT 29 /* rows of a champion portrait */
/* The portrait bitmap of each champion (the portrait bitmap pointer at champion +0x114). */
extern uint8_t g_champion_portraits[CHAMPION_MAX_COUNT][CHAMPION_PORTRAIT_WIDTH * CHAMPION_PORTRAIT_HEIGHT];
/* Copies portrait portrait_index of graphic 26 into the portrait of the champion. */
void champion_set_portrait(int16_t champion_index, int16_t portrait_index);
extern int16_t g_candidate_champion_ordinal; /* 0: no candidate */
extern int16_t g_champion_pending_damage[CHAMPION_MAX_COUNT];
extern uint16_t g_champion_pending_wounds[CHAMPION_MAX_COUNT];
extern int16_t g_champion_skill_level_up[CHAMPION_MAX_COUNT][SKILL_BASE_COUNT];
/* Champions that died in the tick, one bit per champion index: the user interface releases them after the tick. */
extern uint16_t g_dead_champions;
/* The champions that died in the tick (one bit each); the notice is then cleared. */
uint16_t champion_take_dead_champions(void);
/* Asks the user interface to draw these parts of a champion again (CHAMPION_ATTR_* redraw flags). */
void champion_add_redraw_flags(uint16_t champion_index, uint16_t attributes);
/* The champion turns to the party direction. */
void champion_face_party_direction(Champion *champion);
/* The start of a game: no leader and an empty leader hand. Returns the hand object before, which a loaded
   game puts back. */
uint16_t champion_reset_data_to_start_game(void);

/* Original 0x140-byte big-endian record (save files, emulator tests). */
void champion_pack(const Champion *c, uint8_t *out);
void champion_unpack(const uint8_t *in, Champion *c);

/* The party stands in front of a mirror. The text on the party square holds the champion data. */
void champion_add_candidate_to_party(int16_t portrait_index);
/* Cancel removes the candidate. Resurrect and reincarnate take the possessions from the wall
   square and disable the mirror. The original also asks for a new name on reincarnation. */
void champion_click_in_resurrect_reincarnate_panel(int16_t command);
/* Vi altar: the champion comes back with no objects and with a lower maximum health. */
void champion_resurrect(uint16_t champion_index);
/* The living champion in a cell of the party square, or -1. */
int16_t champion_get_index_in_cell(int16_t cell);
/* The leader carries the hand object, so its weight moves to the new leader. */
void champion_set_leader(int16_t champion_index);
/* Input and screen changes of waking up belong to the interface. */
void champion_wake_up(void);
/* The party goes to sleep; the input and the sleep screen belong to the interface. */
void champion_go_to_sleep(void);
/* The leader drinks its fill at a fountain; with whole_party every living champion drinks. */
void champion_drink_from_fountain(bool whole_party);
/* Each tick, an idle champion (not the leader, not acting) turns to its last attacker. Returns one bit per
   champion that turned. */
uint16_t champion_turn_to_last_attackers(void);
/* A champion faces the party direction and moves to a cell; a champion in that cell takes its old cell.
   Returns that other champion, or -1. */
int16_t champion_move_to_cell(int16_t champion_index, int16_t cell);

/* Level 1 below 500 experience, one more level for each doubling. A hidden skill counts the
   mean of its own and its base skill experience. Sleeping champions have level 1. */
int16_t champion_get_skill_level(int16_t champion_index, uint16_t skill_and_flags);
/* Experience counts more on difficult maps and in recent fights. A hidden skill also trains its base skill. */
void champion_add_skill_experience(uint16_t champion_index, uint16_t skill, uint16_t experience);
/* Tenths of a kilogram, rounded up to whole kilograms. */
uint16_t champion_get_maximum_load(const Champion *champion);
/* Ticks between two party moves for this champion: more when the load is heavy. */
int16_t champion_get_movement_ticks(const Champion *champion);
/* Dexterity for hit and dodge rolls, lower with a heavy load, between 1 and 100. */
int16_t champion_get_dexterity(const Champion *champion);
/* Strength with the object in a hand: weight, weapon strength, and weapon skills. 0 to 100. */
int16_t champion_get_strength(int16_t champion_index, int16_t slot);
/* Defense of one body part (slot 0-5). Shields in the hands count for every part. */
int16_t champion_get_wound_defense(int16_t champion_index, uint16_t slot_and_flags);

/* Damage arrives at the end of the tick. Attack type ATTACK_RAW skips defense and wounds. */
int16_t champion_add_pending_damage_and_wounds(int16_t champion_index, int16_t attack, uint16_t allowed_wounds,
                                               int16_t attack_type);
/* The main loop calls this every tick. A champion at 0 health dies. */
void champion_apply_and_draw_pending_damage_and_wounds(void);
/* Closes the champion's inventory and action menu, drops all objects and the bones on the party
   square, and passes the leader and the spell casting to the first living champion. The party dies
   with its last champion. */
void champion_kill(uint16_t champion_index);
/* All objects fall to the floor in the champion's cell. */
void champion_drop_all_objects(uint16_t champion_index);
/* Each poison step deals attack / 64 (at least 1) and schedules the next step with attack - 1. */
void champion_poison(int16_t champion_index, uint16_t attack);
/* Removes all poison events of the champion. */
void champion_unpoison(int16_t champion_index);
/* Each champion takes attack +- attack / 8. Returns the number of champions that took damage. */
int16_t champion_damage_all(uint16_t attack, uint16_t allowed_wounds, int16_t attack_type);
/* Stamina below 0 becomes damage (half the deficit). */
void champion_decrement_stamina(int16_t champion_index, int16_t decrement);
/* The party falls into a pit. With a rope it loses stamina instead. */
void champion_fall_damage(bool use_rope);

/* Removes the oldest scents first; the footprint range moves with the list. */
void champion_delete_scent(uint16_t scent_index);
/* The main loop calls this every 64 ticks (16 while the party sleeps). */
void champion_apply_time_effects(void);

#endif
