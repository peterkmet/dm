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

/* Dungeon view: aspects, scales, and shifts. */
#define KAOS_CREATURE_REPLACEMENT_COLOR_SETS 0x102efa /* 13 sets: a color per light level, then the D2 and D3 change */
#define KAOS_CREATURE_ASPECTS                0x102e58 /* 27 records of 6 bytes */
#define KAOS_OBJECT_ASPECTS                  0x102c68 /* 85 records of 4 bytes */
#define KAOS_PROJECTILE_ASPECTS              0x102db8 /* 4-byte records, index 1-14 */
#define KAOS_FIELD_ASPECTS                   0x102b26 /* 12 records of 8 bytes */
#define KAOS_CREATURE_SHIFT_SETS             0x102fd0 /* int8_t[3][8] by distance */
#define KAOS_PILE_SHIFTS                     0x102e38 /* 16 pairs of shift indices of stacked objects */
#define KAOS_OBJECT_SCALES                   0x102e24 /* object scale by row */
#define KAOS_PROJECTILE_SCALES               0x102e2a /* projectile scale by row */
#define KAOS_EXPLOSION_SCALES                0x102e32 /* explosion scale by depth */
#define KAOS_REBIRTH_SCALES                  0x102fe8 /* rebirth step 2, then step 1 scale by object coordinate set */
#define KAOS_GROUP_DIRECTIONS_PACKED         0x10192a /* the 4 directions of a group, 2 bits per creature */

/* Dungeon view: view squares, wall sets, ornaments, and the viewport. */
#define KAOS_VIEW_SQUARE_LANE            0x102a7e /* int8_t[24] each */
#define KAOS_VIEW_SQUARE_DEPTH           0x102a96
#define KAOS_VIEW_SQUARE_OBJECT_COORD    0x102aae
#define KAOS_VIEW_SQUARE_ALCOVE_COORD    0x102ac6
#define KAOS_VIEW_SQUARE_CREATURE_COORD  0x102ade
#define KAOS_VIEW_SQUARE_EXPLOSION_COORD 0x102af6
#define KAOS_VIEW_SQUARE_FIELD_ASPECT    0x102b0e
#define KAOS_WALL_SET_LAYOUT_IDS         0x102a22 /* int16_t[46] */
#define KAOS_WALL_ORNAMENT_DEPTHS        0x102ba6 /* depth of each wall ornament view (0-4) */
#define KAOS_PLAQUE_HEIGHTS              0x102c57 /* one byte before the table: a line count starts at 1 */
#define KAOS_FLOOR_ORNAMENT_VIEW_OFFSETS 0x102bb4 /* graphic offset of each floor ornament view */
#define KAOS_ALCOVE_ORNAMENT_INDICES     0x102bc0 /* uint8_t[3] */
#define KAOS_FOUNTAIN_ORNAMENT_INDICES   0x102bc3 /* uint8_t[1] */
#define KAOS_WALL_ORNAMENT_COORD_SETS    0x102bc4
#define KAOS_FLOOR_ORNAMENT_COORD_SETS   0x102c00
#define KAOS_DOOR_ORNAMENT_COORD_SETS    0x102c0a
#define KAOS_DOOR_BUTTON_COORD_SETS      0x102c16
#define KAOS_WALL_TEXT_GLYPH_WIDTH       0x10296e
#define KAOS_WALL_TEXT_GLYPH_HEIGHT      0x102970
#define KAOS_VIEWPORT_X                  0x10293a /* viewport origin on the screen */
#define KAOS_VIEWPORT_Y                  0x10293c
#define KAOS_VIEWPORT_WIDTH              0x102948
#define KAOS_VIEWPORT_HEIGHT             0x10294a
#define KAOS_LAYOUT_BITMAPS              0x1025f2 /* 14 bytes each, layout bitmap -1 - n */

/* Palettes (16 colors) and palette changes (16-byte color maps). */
#define KAOS_INITIAL_COLORMAP                 0x100170 /* interface palette, then the pointer palette */
#define KAOS_CREDITS_PALETTE                  0x100a46
#define KAOS_ENTRANCE_PALETTE                 0x100a66
#define KAOS_DUNGEON_PALETTES                 0x100aa6 /* 6 light levels, full light first */
#define KAOS_PALETTE_CHANGES_D3               0x102c18
#define KAOS_PALETTE_CHANGES_D2               0x102c28
#define KAOS_PALETTE_CHANGES_DOOR_ORNAMENT_D3 0x102c38
#define KAOS_PALETTE_CHANGES_DOOR_ORNAMENT_D2 0x102c48
#define KAOS_PALETTE_CHANGES_SMOKE            0x102df4
#define KAOS_PALETTE_CHANGES_OBJECT_D3        0x102e04 /* objects and projectiles */
#define KAOS_PALETTE_CHANGES_OBJECT_D2        0x102e14
#define KAOS_PALETTE_CHANGES_CREATURE_D3      0x102fb0
#define KAOS_PALETTE_CHANGES_CREATURE_D2      0x102fc0
#define KAOS_INVISIBILITY_PALETTE_CHANGES     0x10247a
#define KAOS_PALETTE_CHANGES_ACTION_ICON      0x103ffa
#define KAOS_POINTER_SHADOW_COLORS            0x1005fe

#endif
