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
#define GFX_MAGIC_MAP_TILES 12          /* CSB: the 7 x 7 pixel tiles of the magic map */
#define GFX_PANEL_SCROLL 23
#define GFX_PANEL_MAGIC_MAP 24          /* CSB */
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
#define GFX_FONT 695                     /* 6 rows of 128 bytes, one byte per character */
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
#define ZONE_MAGIC_MAP 614               /* CSB: the top left square of the 7 x 7 grid */
#define ZONE_MAGIC_MAP_RUNE_FIRST 615    /* CSB: + KU, ROS, DAIN, NETA */
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
#define ICON_EMPTY_FLASK 195
#define ICON_BONES 147
#define ICON_WATER_FLASK 163
#define ICON_EMPTY_BODY_SLOT_FIRST 212   /* + 2 * slot, + 1 wounded; slots 0-5 */
#define ICON_EMPTY_SLOT 204
#define ICON_MOUTH 205                   /* + 1: the second image of the blinking mouth */
#define ICON_EMPTY_NECK_POUCH_BASE 198   /* + slot for slots 10-13 */
#define ICON_EMPTY_ACTION_HAND 201       /* action area */

/* Strings of graphic 700. */
#define TEXT_CLASS_FIRST 0               /* + index of g_base_skill_string_indices */
#define TEXT_SKILL_LEVEL_FIRST 4         /* + index of g_skill_level_string_indices: level 2-16 */
#define TEXT_SKILL_LEVEL_COUNT 15
#define TEXT_DIRECTION_FIRST 22          /* + index of g_direction_string_indices */
#define TEXT_STATISTIC_FIRST 41          /* + index of g_statistic_string_indices */
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
#define TEXT_FLIP_HEADS 69
#define TEXT_FLIP_TAILS 70
/* Spell failure messages: a prefix, the name, and a suffix (+ 1). */
#define TEXT_SPELL_FAILURE_WIZARD 51
#define TEXT_SPELL_FAILURE_PRIEST 53
#define TEXT_SPELL_FAILURE_MEANINGLESS 55
#define TEXT_SPELL_FAILURE_FLASK 57
#define TEXT_SPELL_FAILURE_MAGIC_MAP 59  /* CSB */
#define TEXT_NO_CREATURE 107             /* CSB scroll text CREANAME */
#define TEXT_CREATURE_NAME_FIRST 108     /* + creature type */

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
#define ZONE_ENTRANCE_ENTER 435          /* the pressed ENTER button */
#define ZONE_ENTRANCE_RESUME 436         /* the pressed RESUME button */
#define ZONE_ENTRANCE_VIEWPORT 432       /* the dungeon view behind the doors, in the doors part */

/* Commands and the game loop: graphics, icons, strings. */
#define GFX_ENTRANCE_LEFT_DOOR 2
#define GFX_ENTRANCE_RIGHT_DOOR 3
#define GFX_ENTRANCE 4
#define GFX_ENTRANCE_BUTTONS 11          /* the pressed buttons: ENTER, then RESUME, 13 rows each */
#define ICON_EYE 202
#define ICON_EYE_LOOKING 203
#define TEXT_WAKE_UP 19
#define TEXT_GAME_FROZEN 20
/* Save, load, and endgame texts. */
#define TEXT_RESTART_THIS_GAME 77
#define TEXT_GAME_LOADED 78
#define TEXT_NOT_MASTER_DISK 79
#define TEXT_CANT_FIND_SAVED_GAME 80
#define TEXT_UNABLE_TO_SAVE 81
#define TEXT_UNABLE_TO_FORMAT 82
#define TEXT_FORMAT_ANYWAY 83
#define TEXT_WRITE_PROTECTED 84
#define TEXT_NOT_SAME_GAME 85
#define TEXT_GAME_DAMAGED 86
#define TEXT_SAVING_GAME 87
#define TEXT_LOADING_GAME 88
#define TEXT_FORMATTING_DISK 89
#define TEXT_DISK_UNREADABLE 90
#define TEXT_NO_DISK_IN_DRIVE 91
#define TEXT_GAME_SAVE_DISK 92
#define TEXT_NOT_SAVED_OK_TO_QUIT 93     /* empty in CSB */
#define TEXT_CANT_MODIFY_GAME_DISK 94
#define TEXT_IS_GAME_DISK 95
#define TEXT_PUT_GAME_DISK 96
#define TEXT_PUT_SAVE_DISK 97
#define TEXT_PUT_BLANK_DISK 98
#define TEXT_LOAD_SAVED_GAME 101
#define TEXT_SAVE 102                    /* CSB: SAVE AND PLAY */
#define TEXT_QUIT 103                    /* CSB: SAVE AND QUIT */
#define TEXT_FORMAT_FLOPPY 104
#define TEXT_OK 105
#define TEXT_CANCEL 106

/* Dungeon view squares: graphics. Square names: D0-D3 rows from the party, L/C/R lanes, L2/R2 two lanes aside. */
#define GFX_PIT_D3L2 49                  /* D3R2 and D3R: mirrored */
#define GFX_PIT_D3L 50
#define GFX_PIT_D3C 51
#define GFX_PIT_D2L 52                   /* D2R: mirrored, same for the D1 and D0 pits */
#define GFX_PIT_D2C 53
#define GFX_PIT_D1L 54
#define GFX_PIT_D1C 55
#define GFX_PIT_D0L 56
#define GFX_PIT_D0C 57
#define GFX_INVISIBLE_PIT_D2L 58
#define GFX_INVISIBLE_PIT_D2C 59
#define GFX_INVISIBLE_PIT_D1L 60
#define GFX_INVISIBLE_PIT_D1C 61
#define GFX_INVISIBLE_PIT_D0L 62
#define GFX_INVISIBLE_PIT_D0C 63
#define GFX_CEILING_PIT_D2L 64
#define GFX_CEILING_PIT_D2C 65
#define GFX_CEILING_PIT_D1L 66
#define GFX_CEILING_PIT_D1C 67
#define GFX_CEILING_PIT_D0L 68
#define GFX_CEILING_PIT_D0C 69
#define GFX_FLOOR_SET_FIRST 78           /* + 2 * floor set: floor, then ceiling */
#define GFX_WALLSET_FIRST 86             /* + GFX_WALLSET_SIZE * wall set: 7 door frames, 15 walls, 18 stairs */
#define GFX_WALLSET_SIZE 40
#define GFX_DOOR_FIRST 246               /* + 3 * door set + depth (D3, D2, D1) */
#define GFX_WALL_TEXT_FONT 258
#define GFX_WALL_ORNAMENT_FIRST 259      /* + 2 * ornament: side view, front view */
#define GFX_FOOTPRINTS 379
#define GFX_FLOOR_ORNAMENT_FIRST 385     /* + 6 * ornament + floor view graphic */
#define GFX_DOOR_MASK_DESTROYED 439
#define GFX_DOOR_MASK_THIEVES_EYE 440
#define GFX_DOOR_ORNAMENT_FIRST 441      /* + ornament */
#define GFX_DOOR_BUTTON 453

/* Dungeon view squares: zones in the viewport. */
#define ZONE_SIZED 0x4000                /* zone flag: the item size comes from the zone relative x, y */
#define ZONE_CEILING 700
#define ZONE_FLOOR 701
#define ZONE_WALL_D3L2 702               /* also the field of a teleporter on the square */
#define ZONE_WALL_D3R2 703
#define ZONE_WALL_D3C 704
#define ZONE_WALL_D3L 705
#define ZONE_WALL_D3R 706
#define ZONE_WALL_D2L2 707
#define ZONE_WALL_D2R2 708
#define ZONE_WALL_D2C 709
#define ZONE_WALL_D2L 710
#define ZONE_WALL_D2R 711
#define ZONE_WALL_D1C 712
#define ZONE_WALL_D1L 713
#define ZONE_WALL_D1R 714
#define ZONE_WALL_D0C 715
#define ZONE_WALL_D0L 716
#define ZONE_WALL_D0R 717
#define ZONE_DOOR_FRAME_LEFT_D3L 718
#define ZONE_DOOR_FRAME_RIGHT_D3L 719
#define ZONE_DOOR_FRAME_LEFT_D3R 720
#define ZONE_DOOR_FRAME_RIGHT_D3R 721
#define ZONE_DOOR_FRAME_LEFT_D3C 722
#define ZONE_DOOR_FRAME_RIGHT_D3C 723
#define ZONE_DOOR_FRAME_LEFT_D2C 724
#define ZONE_DOOR_FRAME_RIGHT_D2C 725
#define ZONE_DOOR_FRAME_LEFT_D1C 726
#define ZONE_DOOR_FRAME_RIGHT_D1C 727
#define ZONE_DOOR_FRAME_D0C 728          /* the party stands in the door frame */
#define ZONE_DOOR_FRAME_TOP_D2L 729
#define ZONE_DOOR_FRAME_TOP_D2C 730
#define ZONE_DOOR_FRAME_TOP_D2R 731
#define ZONE_DOOR_FRAME_TOP_D1L 732
#define ZONE_DOOR_FRAME_TOP_D1C 733
#define ZONE_DOOR_FRAME_TOP_D1R 734
#define ZONE_INSCRIPTION_WALL_D1C 735    /* the blank wall behind a wall text */
#define ZONE_WALL_PORTRAIT 737           /* champion portrait on the D1C wall */
#define ZONE_STAIRS_UP_FRONT_D3L2 800
#define ZONE_STAIRS_UP_FRONT_D3R2 801
#define ZONE_STAIRS_UP_FRONT_D3L 802
#define ZONE_STAIRS_UP_FRONT_D3C 803
#define ZONE_STAIRS_UP_FRONT_D3R 804
#define ZONE_STAIRS_UP_FRONT_D2L 805
#define ZONE_STAIRS_UP_FRONT_D2C 806
#define ZONE_STAIRS_UP_FRONT_D2R 807
#define ZONE_STAIRS_UP_FRONT_D1L 808
#define ZONE_STAIRS_UP_FRONT_D1C 809
#define ZONE_STAIRS_UP_FRONT_D1R 810
#define ZONE_STAIRS_UP_FRONT_D0C_LEFT 811
#define ZONE_STAIRS_UP_FRONT_D0C_RIGHT 812
#define ZONE_STAIRS_DOWN_DELTA 13        /* down stairs front zone = up stairs front zone + 13 */
#define ZONE_STAIRS_SIDE_D2L 826
#define ZONE_STAIRS_SIDE_D2R 827
#define ZONE_STAIRS_UP_SIDE_D1L 828
#define ZONE_STAIRS_UP_SIDE_D1R 829
#define ZONE_STAIRS_DOWN_SIDE_D1L 830
#define ZONE_STAIRS_DOWN_SIDE_D1R 831
#define ZONE_STAIRS_SIDE_D0L 832
#define ZONE_STAIRS_SIDE_D0R 833
#define ZONE_PIT_D3L2 850
#define ZONE_PIT_D3R2 851
#define ZONE_PIT_D3L 852
#define ZONE_PIT_D3C 853
#define ZONE_PIT_D3R 854
#define ZONE_PIT_D2L 855
#define ZONE_PIT_D2C 856
#define ZONE_PIT_D2R 857
#define ZONE_PIT_D1L 858
#define ZONE_PIT_D1C 859
#define ZONE_PIT_D1R 860
#define ZONE_PIT_D0L 861
#define ZONE_PIT_D0C 862
#define ZONE_PIT_D0R 863
#define ZONE_CEILING_PIT_D2L 864
#define ZONE_CEILING_PIT_D2C 865
#define ZONE_CEILING_PIT_D2R 866
#define ZONE_CEILING_PIT_D1L 867
#define ZONE_CEILING_PIT_D1C 868
#define ZONE_CEILING_PIT_D1R 869
#define ZONE_CEILING_PIT_D0L 870
#define ZONE_CEILING_PIT_D0C 871
#define ZONE_CEILING_PIT_D0R 872
#define ZONE_WALL_TEXT_LINE_FIRST 1000   /* + text line */
#define ZONE_WALL_ORNAMENT_FIRST 1004    /* + ZONE_WALL_ORNAMENT_VIEWS * coordinate set + view wall */
#define ZONE_WALL_ORNAMENT_VIEWS 15
#define ZONE_FLOOR_ORNAMENT_FIRST 1500   /* + ZONE_FLOOR_ORNAMENT_VIEWS * coordinate set + floor view */
#define ZONE_FLOOR_ORNAMENT_VIEWS 11
#define ZONE_DOOR_BUTTON_FIRST 1950      /* + coordinate set + button view */
#define ZONE_DOOR_ORNAMENT_FIRST 2000    /* + ZONE_DOOR_ORNAMENT_DEPTHS * coordinate set + depth (D3, D2, D1) */
#define ZONE_DOOR_ORNAMENT_DEPTHS 3
#define ZONE_DOOR_FIRST 3700             /* + ZONE_DOOR_VIEW_ZONES * door view + door state */
#define ZONE_DOOR_VIEW_ZONES 10
#define ZONE_DOOR_LEFT_HALF 3            /* + door zone of the state: the halves of a sliding door */
#define ZONE_DOOR_RIGHT_HALF 6

#endif
