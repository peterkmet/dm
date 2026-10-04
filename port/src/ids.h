#ifndef IDS_H
#define IDS_H

/* Graphic numbers (Graphics.DAT items), layout zone numbers (graphic 696), object icons, and
   string numbers (graphic 700). Each module adds its own section. */

/* Interface graphics. */
#define GFX_STATUS_BOX_DEAD 8
#define GFX_SPELL_AREA 9
#define GFX_ACTION_AREA_NAMES 10
#define GFX_MOVEMENT_ARROWS 13
#define GFX_DAMAGE_ACTION 14
#define GFX_DAMAGE_STATUS_BOX 15
#define GFX_DAMAGE_PORTRAIT 16
#define GFX_INVENTORY 17
#define GFX_PANEL_ARROW 18
#define GFX_PANEL_EYE 19
#define GFX_PANEL_EMPTY 20
#define GFX_PANEL_SCROLL 23
#define GFX_PANEL_CHEST 25
#define GFX_PANEL_OBJECT_CIRCLE 29
#define GFX_CHAMPION_PORTRAITS 26
#define GFX_CHAMPION_ICONS 28
#define GFX_PANEL_FOOD_LABEL 30
#define GFX_PANEL_WATER_LABEL 31
#define GFX_PANEL_POISONED_LABEL 32
#define GFX_SLOT_BOX_NORMAL 33
#define GFX_SLOT_BOX_WOUNDED 34
#define GFX_SLOT_BOX_ACTING_HAND 35
#define GFX_SHIELD_PARTY 37
#define GFX_SHIELD_FIRE 38
#define GFX_SHIELD_SPELL 39
#define GFX_PANEL_RESURRECT_REINCARNATE 40
#define GFX_OBJECT_ICONS_BEFORE_FIRST 41 /* + sheet number 1-7 */
#define GFX_OBJECT_NAMES 694
#define GFX_ACTION_NAMES 699
#define GFX_STRINGS 700

/* Interface zones on the screen. */
#define ZONE_SCREEN_TOP 401          /* status boxes row, above the viewport */
#define ZONE_SCREEN_RIGHT 403        /* spell and action areas, arrows */
#define ZONE_SCREEN_BOTTOM 405       /* message area row */
#define ZONE_VIEWPORT 7
#define ZONE_MOVEMENT_ARROWS 9
#define ZONE_ACTION_AREA 11
#define ZONE_SPELL_AREA 13
#define ZONE_MESSAGE_AREA 15
#define ZONE_LEADER_HAND_OBJECT_NAME 17
#define ZONE_ACTION_DAMAGE 75
#define ZONE_ACTION_AREA_TWO_ACTIONS 77
#define ZONE_ACTION_AREA_ONE_ACTION 79
#define ZONE_ACTING_CHAMPION_NAME 80
#define ZONE_ACTION_NAME_FIRST 85        /* + action list index 0-2 */
#define ZONE_ACTION_ICON_BOX_FIRST 89    /* + champion */
#define ZONE_ACTION_ICON_FIRST 93        /* + champion */
#define ZONE_CHAMPION_ICON_FIRST 113     /* + cell relative to the party direction */
#define ZONE_STATUS_BOX_FIRST 151        /* + champion */
#define ZONE_STATUS_BOX_NAME_FIRST 159   /* + champion */
#define ZONE_STATUS_BOX_NAME_TEXT_FIRST 163
#define ZONE_STATUS_BOX_DAMAGE_FIRST 167
#define ZONE_STATUS_BOX_PORTRAIT_FIRST 175
#define ZONE_PORTRAIT_DAMAGE_FIRST 179
#define ZONE_BAR_FIRST 195               /* + 4 * bar (health, stamina, mana) + champion */
#define ZONE_SPELL_AREA_TABS 221
#define ZONE_SPELL_TAB_FIRST 224         /* + 5 * caster + champion */
#define ZONE_SPELL_CASTER_NAME_FIRST 228 /* + 5 * caster */
#define ZONE_SPELL_SYMBOL_FIRST 255      /* + symbol 0-5 */
#define ZONE_SPELL_CHAMPION_SYMBOL_FIRST 261 /* + symbol 0-3 */

/* Inventory zones in the viewport. */
#define ZONE_PANEL 101
#define ZONE_PANEL_FOOD_BAR 103
#define ZONE_PANEL_WATER_BAR 104
#define ZONE_PANEL_FOOD_LABEL 500
#define ZONE_PANEL_WATER_LABEL 501
#define ZONE_PANEL_POISONED_LABEL 502
#define ZONE_PANEL_ARROW_OR_EYE 503
#define ZONE_PANEL_OBJECT_CIRCLE 504
#define ZONE_PANEL_OBJECT_ICON 505
#define ZONE_PANEL_OBJECT_NAME 506
#define ZONE_PANEL_OBJECT_TEXT 556       /* x of the object description lines */
#define ZONE_PANEL_SCROLL_CENTER 560
#define ZONE_MOUTH 545
#define ZONE_EYE 546
#define ZONE_HEALTH_LABEL 547            /* stamina + 1, mana + 2 */
#define ZONE_HEALTH_VALUE 550            /* stamina + 1, mana + 2 */
#define ZONE_NAME_TITLE 553
#define ZONE_LOAD_LABEL 554
#define ZONE_LOAD_VALUE 555
#define ZONE_PANEL_SKILLS 557
#define ZONE_PANEL_STATISTICS 559
#define ZONE_INVENTORY_SAVE 562          /* hidden for a candidate */
#define ZONE_INVENTORY_SLEEP 564
#define ZONE_INVENTORY_CLOSE 566

/* Object icons. */
#define ICON_CHEST_CLOSED 144
#define ICON_CHEST_OPEN 145
#define ICON_BONES 147
#define ICON_WATER_FLASK 163
#define ICON_EMPTY_BODY_SLOT_FIRST 212   /* + 2 * slot, + 1 wounded; slots 0-5 */
#define ICON_EMPTY_SLOT 204
#define ICON_EMPTY_NECK_POUCH_BASE 198   /* + slot for slots 10-13 */
#define ICON_EMPTY_ACTION_HAND 201       /* action area */

/* Strings of graphic 700. */
#define TEXT_PARTY_FACING 21
#define TEXT_CONSUMABLE 26               /* + object attribute bit: consumable, poisoned, broken, cursed */
#define TEXT_AND 30
#define TEXT_BURNT_OUT 31
#define TEXT_WATERSKIN_EMPTY 32          /* + charges: almost empty, almost full, full */
#define TEXT_WEIGHS 37
#define TEXT_KG_PERIOD 39
#define TEXT_LOAD 36
#define TEXT_DECIMAL_POINT 38
#define TEXT_KG 40
#define TEXT_HEALTH 48
#define TEXT_STAMINA 49
#define TEXT_MANA 50
#define TEXT_CANT_REACH 71
#define TEXT_NEED_AMMO 72

/* Dungeon view things: graphics. */
#define GFX_FIELD_MASK_FIRST 70          /* + field aspect mask */
#define GFX_FIELD_PATTERN_FIRST 76       /* + field aspect pattern: teleporter, fluxcage */
#define GFX_PROJECTILE_FIRST 454         /* + projectile aspect graphic + pose */
#define GFX_EXPLOSION_FIRST 486          /* + explosion aspect (fire, spell, poison) */
#define GFX_EXPLOSION_PATTERN_FIRST 489  /* + 3 * aspect + strength, the party square fill */
#define GFX_OBJECT_FIRST 498             /* + object aspect graphic; + 1 the alcove graphic */
#define GFX_CREATURE_FIRST 584           /* + creature aspect graphic: front, side, back, attack */

/* Dungeon view things: zones in the viewport. */
#define ZONE_VIEWPORT_BOX 4
#define ZONE_FIELD_FIRST 702             /* + field aspect index */
#define ZONE_OBJECT_FIRST 2500           /* + 4 * object coordinate set + view cell */
#define ZONE_ALCOVE_OBJECT_FIRST 2548    /* + 7 * object coordinate set + alcove coordinate set */
#define ZONE_PROJECTILE_FIRST 2900       /* + 4 * object coordinate set + view cell */
#define ZONE_REBIRTH_STEP1_FIRST 3000    /* + object coordinate set */
#define ZONE_REBIRTH_STEP2_FIRST 3007    /* + object coordinate set */
#define ZONE_EXPLOSION_CENTERED_FIRST 3014 /* + explosion coordinate set */
#define ZONE_EXPLOSION_FIRST 3031        /* + 2 * explosion coordinate set + side */
#define ZONE_CREATURE_FIRST 3200         /* + 65 * creature coordinate set + 5 * view square set + position */
#define ZONE_SHIFTED 0x8000              /* zone flag: the position moves by the creature or pile shift */
#define ZONE_OBJECT_CELLS 4              /* object and projectile zones per object coordinate set */
#define ZONE_ALCOVE_VIEW_SETS 7          /* alcove object zones per object alcove coordinate set */
#define ZONE_EXPLOSION_SIDES 2           /* near side, far side */
#define ZONE_CREATURE_POSITIONS 5        /* four view cells and the center */
#define ZONE_CREATURE_SET 65             /* creature zones per creature coordinate set */

/* Champion rules: object icons (object info type of each object). */
#define ICON_COMPASS_NORTH 0             /* + party direction */
#define ICON_TORCH_UNLIT 4               /* lit: + charge offset, up to 7 */
#define ICON_TORCH_LAST 7
#define ICON_WATERSKIN_EMPTY 8
#define ICON_WATERSKIN_FULL 9
#define ICON_JEWEL_SYMAL 10              /* + 1 when worn */
#define ICON_JEWEL_SYMAL_WORN 11
#define ICON_ILLUMULET 12                /* + 1 when worn */
#define ICON_ILLUMULET_WORN 13
#define ICON_FLAMITT 14                  /* + 1 with charges */
#define ICON_EYE_OF_TIME 16
#define ICON_STORMRING 18
#define ICON_STAFF_OF_CLAWS 20           /* 20-22 */
#define ICON_STAFF_OF_CLAWS_LAST 22
#define ICON_BOLT_BLADE 23
#define ICON_FURY 25
#define ICON_FIRESTAFF 27
#define ICON_FIRESTAFF_COMPLETE 28
#define ICON_SCROLL_OPEN 30              /* + 1 when closed */
#define ICON_SCROLL_CLOSED 31
#define ICON_LAST_WITH_STATE ICON_SCROLL_OPEN
#define ICON_DELTA 38
#define ICON_VORPAL_BLADE 40
#define ICON_INQUISITOR 41
#define ICON_MACE_OF_ORDER 45
#define ICON_STAFF 58                    /* 58-66: staffs and wands */
#define ICON_SCEPTRE_OF_LYF 66
#define ICON_CLOAK_OF_NIGHT 81
#define ICON_CROWN_OF_NERRA 104
#define ICON_ELVEN_BOOTS 119
#define ICON_GEM_OF_AGES 120
#define ICON_EKKHARD_CROSS 121
#define ICON_MOONSTONE 122
#define ICON_PENDANT_FERAL 124
#define ICON_RABBITS_FOOT 137
#define ICON_DEXHELM 140
#define ICON_FLAMEBAIN 141
#define ICON_POWERTOWERS 142
#define ICON_FOOD_FIRST 168              /* apple to dragon steak: food value table index */
#define ICON_FOOD_LAST 175
#define ICON_BOOTS_OF_SPEED 194

/* Champion rules: strings of graphic 700. Each message is a prefix, the name, and a suffix (+ 1). */
#define TEXT_LEVEL_GAINED 61             /* + 2 * base skill */
#define TEXT_RESURRECTED 73
#define TEXT_REINCARNATED 75

/* Commands and the game loop: zones. */
#define ZONE_SCREEN 2                    /* the whole screen */
#define ZONE_VIEWPORT_MESSAGE 6          /* "WAKE UP", "GAME FROZEN" in the viewport */
#define ZONE_MOVEMENT_ARROW_BEFORE_FIRST 67 /* + movement command 1-6: the arrow box */
#define ZONE_ACTION_NAME_BOX_FIRST 82    /* + action list index 0-2 */
#define ZONE_ACTION_PASS_BOX 98
#define ZONE_SPELL_CASTER_TAB_FIRST 224  /* + 5 * caster + champion */
#define ZONE_SPELL_SYMBOL_BOX_FIRST 245  /* + symbol 0-5 */
#define ZONE_SPELL_CAST_BOX 252
#define ZONE_SPELL_DELETE_BOX 254
#define ZONE_DROP_CELL_FIRST 740         /* + view cell 0-3: the floor where a dropped object lands */
#define ZONE_ENTRANCE_WORK 427           /* the doors part of the entrance on the screen */
#define ZONE_ENTRANCE_DOORS 429          /* size of the doors part */
#define ZONE_ENTRANCE_LEFT_DOOR 430
#define ZONE_ENTRANCE_RIGHT_DOOR 431

/* Commands and the game loop: graphics, icons, strings. */
#define GFX_ENTRANCE_LEFT_DOOR 2
#define GFX_ENTRANCE_RIGHT_DOOR 3
#define GFX_ENTRANCE 4
#define ICON_EYE 202
#define ICON_EYE_LOOKING 203
#define TEXT_WAKE_UP 19
#define TEXT_GAME_FROZEN 20
/* Save, load, and endgame texts. */
#define TEXT_RESTART_THIS_GAME 77
#define TEXT_GAME_LOADED 78
#define TEXT_CANT_FIND_SAVED_GAME 80
#define TEXT_UNABLE_TO_SAVE 81
#define TEXT_GAME_DAMAGED 86
#define TEXT_SAVING_GAME 87
#define TEXT_LOADING_GAME 88
#define TEXT_NOT_SAVED_OK_TO_QUIT 93
#define TEXT_LOAD_SAVED_GAME 101
#define TEXT_SAVE 102
#define TEXT_QUIT 103
#define TEXT_OK 105
#define TEXT_CANCEL 106

#endif
