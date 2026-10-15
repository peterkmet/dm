#ifndef KAOS_ADDRESSES_H
#define KAOS_ADDRESSES_H

/* Addresses of the KAOS.FTL image (and of the KAOS-layout CSB image). Only data/ includes this header. */

#define KAOS_BASE 0x100000

/* Creature, object, and door tables. */
#define KAOS_CREATURE_INFO   0x10192e
#define KAOS_CREATURE_SOUNDS 0x101bec
#define KAOS_OBJECT_INFO     0x101068
#define KAOS_WEAPON_INFO     0x1014a0
#define KAOS_ARMOUR_INFO     0x1015b4
#define KAOS_JUNK_WEIGHTS    0x10169c
#define KAOS_FOOD_VALUES     0x1016d2
#define KAOS_DOOR_INFO       0x1016e2 /* g_door_info[], high byte attributes */

/* Spells and actions. */
#define KAOS_SYMBOL_BASE_MANA_COST       0x103c66 /* uint8_t[4][6] */
#define KAOS_SYMBOL_MANA_COST_MULTIPLIER 0x103c7e /* uint8_t[6], by power symbol */
#define KAOS_SPELLS                      0x103c84 /* SpellRecord[25], 8 bytes each */
#define KAOS_ACTION_SETS            0x103d4c /* 44 records of 8 bytes: 3 actions, pad, 2 minimum levels, flags, pad */
#define KAOS_ACTIONS_DISABLED       0x103eac /* ticks that each action disables the champion */
#define KAOS_ACTION_DAMAGE_FACTOR   0x103ed8
#define KAOS_ACTION_HIT_PROBABILITY 0x103f04
#define KAOS_ACTION_STAMINA         0x103f30
#define KAOS_ACTION_DEFENSE         0x103f5c
#define KAOS_ACTION_SKILL_INDEX     0x103f88
#define KAOS_ACTION_EXPERIENCE_GAIN 0x103fb4

/* Champion tables. */
#define KAOS_LIGHT_POWER_TO_LIGHT_AMOUNT 0x100cb4
#define KAOS_PALETTE_LIGHT_THRESHOLDS    0x100cd4
#define KAOS_SLOT_MASKS                  0x100c68
#define KAOS_WOUND_DEFENSE_FACTOR        0x100ce4
#define KAOS_FIXED_POSSESSIONS           0x101c10
#define KAOS_CHAMPION_COLORS             0x100ce0 /* uint8_t[4] */
#define KAOS_SLOT_DROP_ORDER             0x100d0e /* uint16_t[30] */
#define KAOS_TORCH_CHARGE_TO_ICON_OFFSET 0x100ba0 /* uint8_t[16] */
#define KAOS_PORTRAIT_WIDTH              0x102958
#define KAOS_PORTRAIT_HEIGHT             0x10295a

/* Movement and creature tables. */
#define KAOS_DIRECTION_TO_STEP_EAST          0x101038 /* int16_t[4] */
#define KAOS_DIRECTION_TO_STEP_NORTH         0x101040 /* int16_t[4] */
#define KAOS_MOVEMENT_ARROW_TO_STEP_FORWARD  0x103c56 /* int16_t[4] */
#define KAOS_MOVEMENT_ARROW_TO_STEP_RIGHT    0x103c5e /* int16_t[4] */
#define KAOS_ORDERED_CELLS_TO_ATTACK         0x100b66 /* uint8_t[8][4] */
#define KAOS_WOUND_PROBABILITY_INDEX_TO_MASK 0x100b86 /* uint8_t[4] */
#define KAOS_SLOTS_TO_STEAL_FROM             0x100b8a /* uint8_t[8] */
#define KAOS_SQUARE_TYPE_TO_EVENT_TYPE       0x100d4a /* uint8_t[8] */

/* Sounds. */
#define KAOS_SOUNDS        0x100d52 /* 12-byte entries: graphic, rate, priority */
#define KAOS_SOUND_VOLUMES 0x100696 /* uint8_t[25][25] by squares ahead and to the side */

/* Dungeon load and text decoding. */
#define KAOS_NEW_GAME_EXTRA_THINGS      0x101058 /* free records per thing type for a new game */
#define KAOS_MESSAGE_ESCAPE_STRINGS     0x1016ea /* 8-byte strings */
#define KAOS_ESCAPE_REPLACEMENT_CHARS   0x1017ea /* 2-byte strings */
#define KAOS_INSCRIPTION_ESCAPE_STRINGS 0x10182a /* 8-byte strings */

/* Dialogs. */
#define KAOS_DIALOG_MOUSE_INPUTS 0x103bd4 /* 8 pointers to the dialog mouse tables */
#define KAOS_DIALOG_TABLES       0x103a84 /* the 8 dialog mouse tables, equal bytes in both engines */

#endif
