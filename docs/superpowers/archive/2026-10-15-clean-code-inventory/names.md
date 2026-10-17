# Names inventory for the clean-code epic (R3, R7)

Snapshot of `main` at b3d1852, 2026-10-15. Read-only inventory. Line numbers are of `port/src/` unless the path starts with `tests/` or `docs/`. Image addresses (`0x10xxxx`) are excluded.

Method: a script extracted every `#define`, enum member and `static const` (2713 constants), then matched constants with the same value and shared name tokens across files, and every constant in a `.c` file whose prefix is a family owned by a header. Each hit was then read in the code. "Uses" = word hits in `port/src` minus the definition. "tests" = test files with a word hit (Python copies of the value or ctypes bindings).

## 1. R3 duplicates and misplaced constants

| file:line | name | value | owner header | name to keep / action | uses | tests | docs |
|---|---|---|---|---|---|---|---|
| engine/group.c:35 | `SLOT_ACTION_HAND` | `1` | engine/game.h | SLOT_HAND_ACTION (game.h:23) | 2 | test_creatures.py | - |
| engine/projectile.h:25 | `CELL_CENTERED` | `0xff` | engine/game.h (cells) | one name: CELL_CENTERED or CELLS_CENTERED (group.h:50); keep one in game.h next to CELL_COUNT | 7 | - | - |
| engine/group.h:50 | `CELLS_CENTERED` | `0xff` | engine/game.h (cells) | merge with CELL_CENTERED | 16 | - | - |
| engine/group.h:58 | `GROUP_RECORD_DIRECTION_BYTE` | `14` | engine/map.h | GROUP_DIRECTION_BYTE (map.h:210) | 3 | - | - |
| engine/group.c:33 | `SKILL_PARRY_DEFENSE` | `7` | data/tables.h | new SKILL_PARRY in the skill enum (tables.h:32) | 2 | - | - |
| engine/group.c:34 | `ICON_DIAMOND_EDGE` | `39` | data/ids.h | ICON_DIAMOND_EDGE | 1 | - | - |
| engine/group.c:34 | `ICON_HALF_DAMAGE_ON_MATERIAL` | `40` | data/ids.h | one icon name with action.c ICON_WEAPON_HITS_NON_MATERIAL (=40), e.g. ICON_VORPAL_BLADE | 1 | - | - |
| engine/action.c:33 | `ICON_WEAPON_HITS_NON_MATERIAL` | `40` | data/ids.h | merge with group.c ICON_HALF_DAMAGE_ON_MATERIAL | 1 | - | - |
| engine/group.c:34 | `ICON_HARDCLEAVE` | `43` | data/ids.h | ICON_HARDCLEAVE | 1 | - | superpowers/2026-10-06-coding-standard-refactor-plan.md |
| engine/projectile.c:27 | `ICON_JUNK_SOLID_FIRST` | `0xb0` | data/ids.h | ICON_JUNK_SOLID_FIRST | 1 | - | - |
| engine/projectile.c:27 | `ICON_JUNK_SOLID_LAST` | `0xbf` | data/ids.h | ICON_JUNK_SOLID_LAST | 1 | - | - |
| engine/group.c:37 | `HIT_PROBABILITY_ALWAYS_HITS` | `0x8000` | engine/group.h (melee hit argument) | one name with action.c HIT_PROBABILITY_NON_MATERIAL | 1 | - | - |
| engine/action.c:34 | `HIT_PROBABILITY_NON_MATERIAL` | `0x8000` | engine/group.h | merge with HIT_PROBABILITY_ALWAYS_HITS (check meaning: both flag bit 15 of the hit probability argument) | 1 | - | - |
| engine/projectile.c:18 | `POTION_FUL_BOMB` | `19` | engine/inventory.h | POTION_FUL_BOMB next to POTION_* (inventory.h:11) | 2 | - | - |
| engine/projectile.c:18 | `POTION_VEN_BOMB` | `3` | engine/inventory.h | POTION_VEN_BOMB | 1 | - | - |
| engine/inventory.c:25 | `JUNK_WATERSKIN` | `1` | engine/inventory.h | JUNK_WATERSKIN next to JUNK_BONES (inventory.h:14) | 1 | - | - |
| engine/projectile.c:36 | `JUNK_BOULDER` | `25` | engine/inventory.h | JUNK_BOULDER | 1 | - | - |
| engine/magic.c:17 | `JUNK_ZOKATHRA` | `51` | engine/inventory.h | JUNK_ZOKATHRA | 1 | test_csb_magic_map.py | - |
| engine/action.c:36 | `JUNK_MAGICAL_BOX_BLUE` | `42` | engine/inventory.h | JUNK_MAGICAL_BOX_BLUE | 2 | - | - |
| engine/action.c:37 | `JUNK_MAGICAL_BOX_GREEN` | `43` | engine/inventory.h | JUNK_MAGICAL_BOX_GREEN | 1 | - | - |
| ui/view.c:1699 | `OBJECT_TYPE_ZOKATHRA` | `197` | engine/inventory.h | OBJECT_TYPE_ZOKATHRA (same object as JUNK_ZOKATHRA, object-type numbering) | 2 | - | superpowers/2026-10-04-csb-plan.md |
| engine/projectile.c:20 | `WEAPON_DAGGER` | `8` | data/tables.h (WEAPON_TYPE_COUNT) or engine/inventory.h | weapon type enum in one header | 2 | test_actions.py | - |
| engine/projectile.c:20 | `WEAPON_ARROW` | `27` | same | same | 2 | - | - |
| engine/projectile.c:20 | `WEAPON_SLAYER` | `28` | same | same | 2 | - | - |
| engine/projectile.c:20 | `WEAPON_POISON_DART` | `31` | same | same | 2 | - | - |
| engine/projectile.c:20 | `WEAPON_THROWING_STAR` | `32` | same | same | 2 | - | - |
| engine/projectile.c:36 | `WEAPON_TORCH` | `2` | same | same | 1 | - | - |
| engine/projectile.c:36 | `WEAPON_ROCK` | `30` | same | same | 1 | - | - |
| engine/group.c:27 | `OBJECT_INFO_FIRST_WEAPON` | `0x17` | data/tables.h (OBJECT_INFO_COUNT) | one set with inventory.c INFO_WEAPON (=23) | 1 | - | - |
| engine/group.c:27 | `OBJECT_INFO_FIRST_ARMOUR` | `0x45` | data/tables.h | merge with inventory.c INFO_ARMOUR (=69) | 2 | - | - |
| engine/group.c:27 | `OBJECT_INFO_FIRST_JUNK` | `0x7f` | data/tables.h | merge with inventory.c INFO_JUNK (=127) | 2 | - | - |
| engine/inventory.c:16 | `INFO_WEAPON` | `23` | data/tables.h | OBJECT_INFO_FIRST_WEAPON (with INFO_SCROLL, INFO_CONTAINER, INFO_POTION, INFO_ARMOUR, INFO_JUNK) | 1 | - | - |
| engine/inventory.c:16 | `INFO_ARMOUR` | `69` | data/tables.h | OBJECT_INFO_FIRST_ARMOUR | 1 | - | - |
| engine/inventory.c:16 | `INFO_JUNK` | `127` | data/tables.h | OBJECT_INFO_FIRST_JUNK | 1 | - | - |
| engine/inventory.c:32 | `STAT_MANA` | `STAT_COUNT` | engine/game.h | STAT_MANA next to the STAT_* enum (game.h:19) | 4 | - | - |
| engine/group.c:21 | `GRAPHIC_FLIP_NON_ATTACK` | `0x0004` | data/tables.h (CreatureInfo.graphic_info) | one name with ui/view.c:402 CREATURE_FLIP_NON_ATTACK | 1 | - | - |
| ui/view.c:402 | `CREATURE_FLIP_NON_ATTACK` | `0x4` | data/tables.h | merge with GRAPHIC_FLIP_NON_ATTACK; also CREATURE_HAS_SIDE/BACK/ATTACK, CREATURE_SPECIAL_D2_FRONT, CREATURE_D2_FRONT_FLIPPED (view.c:402-403) and GRAPHIC_FLIP_ATTACK/DURING_ATTACK (group.c:21) are graphic_info bits: one enum in tables.h | 1 | - | - |
| engine/group.c:24 | `ASPECT_OFFSET_MASK` | `7` | data/tables.h (graphic_info) | one name with ui/view.c:405 ASPECT_SHIFT_MASK | 1 | - | - |
| engine/group.c:24 | `ASPECT_VERTICAL_OFFSET_SHIFT` | `3` | data/tables.h | one name with ui/view.c:405 ASPECT_SHIFT_Y | 1 | - | - |
| ui/view.c:405 | `ASPECT_SHIFT_MASK` | `0x7` | data/tables.h | merge with ASPECT_OFFSET_MASK | 2 | - | - |
| ui/view.c:405 | `ASPECT_SHIFT_Y` | `3` | data/tables.h | merge with ASPECT_VERTICAL_OFFSET_SHIFT | 1 | - | - |
| ui/view.c:1700 | `CREATURE_NOT_ON_MAP` | `0x4000` | engine/group.h (creature attributes, group.h:16) | CREATURE_NOT_ON_MAP in that enum | 1 | test_csb_magic_map.py | - |
| engine/group.c:60 | `CREATURE_SPELL_MAX_KINETIC_ENERGY` | `255` | engine/projectile.h | one KINETIC_ENERGY_MAXIMUM with magic.c:42 SPELL_KINETIC_ENERGY_MAXIMUM and ui/view.c:422 KINETIC_ENERGY_MAX | 1 | - | - |
| engine/magic.c:42 | `SPELL_KINETIC_ENERGY_MAXIMUM` | `255` | engine/projectile.h | merge | 1 | - | - |
| ui/view.c:422 | `KINETIC_ENERGY_MAX` | `255` | engine/projectile.h | merge | 1 | - | - |
| engine/group.c:32 | `DOOR_DAMAGE` | `5` | engine/map.h | one DOOR_CLOSE_DAMAGE with timeline_process.c:20 (check: both the damage of a closing door to creatures) | 1 | - | - |
| engine/timeline_process.c:20 | `DOOR_CLOSE_DAMAGE` | `5` | same | merge | 1 | - | - |
| engine/group.c:74 | `MOVE_CHAIN_LIMIT` | `99` | engine/move.h | SQUARE_CHAIN_MAXIMUM (move.c:18) | 1 | - | - |
| engine/move.c:18 | `SQUARE_CHAIN_MAXIMUM` | `99` | engine/move.h | keep, export | 1 | - | - |
| engine/action.c:62 | `PROJECTILE_DISABLED_MOVEMENT_TICKS` | `4` | engine/projectile.h | SHOT_DISABLED_MOVEMENT_TICKS (projectile.c:48) | 1 | - | - |
| engine/projectile.c:48 | `SHOT_DISABLED_MOVEMENT_TICKS` | `4` | engine/projectile.h | keep, export | 1 | - | - |
| engine/projectile.c:48 | `FUSE_ACTION_ATTACK` | `255` | engine/projectile.h | one name with ui/startend.c:55 FUSE_LAST_ATTACK | 1 | - | - |
| ui/startend.c:55 | `FUSE_LAST_ATTACK` | `255` | engine/projectile.h | merge | 3 | - | - |
| engine/inventory.c:29 | `SHIELD_POTION_STRONG` | `50` | engine/magic.h | SHIELD_STRONG (magic.c:21) | 1 | - | - |
| engine/magic.c:21 | `SHIELD_STRONG` | `50` | engine/magic.h | keep, export | 2 | - | - |
| engine/inventory.c:30 | `MAXIMUM_POTION_MANA` | `900` | engine/champion.h | MAXIMUM_MANA (champion.c:26) | 1 | - | - |
| engine/champion.c:26 | `MAXIMUM_MANA` | `900` | engine/champion.h | keep, export | 2 | - | - |
| engine/champion.c:23 | `INITIAL_FOOD_WATER` | `1500` | engine/champion.h | one name with savegame.c:22 NEW_ADVENTURE_FOOD_WATER | 2 | - | - |
| engine/savegame.c:22 | `NEW_ADVENTURE_FOOD_WATER` | `1500` | engine/champion.h | merge | 2 | - | - |
| engine/recruit.c:20 | `RECRUIT_STAT_MINIMUM` | `30` | engine/champion.h | one name with savegame.c:24 NEW_ADVENTURE_STAT_MINIMUM (check) | 3 | - | - |
| engine/savegame.c:24 | `NEW_ADVENTURE_STAT_MINIMUM` | `30` | engine/champion.h | merge if same rule | 1 | - | - |
| engine/champion.c:27 | `FOOD_WATER_STARVING` | `(-512)` | engine/champion.h | one name with ui/ui_champion.c:40 FOOD_WATER_LOW | 2 | - | - |
| ui/ui_champion.c:40 | `FOOD_WATER_LOW` | `(-512)` | engine/champion.h | FOOD_WATER_STARVING | 1 | - | - |
| engine/champion.c:59 | `HEAVY_LOAD_NUMERATOR` | `5` | engine/champion.h | one name with ui/ui_champion.c:41 LOAD_WARNING_NUMERATOR | 1 | - | - |
| engine/champion.c:60 | `HEAVY_LOAD_DENOMINATOR` | `8` | engine/champion.h | one name with ui/ui_champion.c:42 LOAD_WARNING_DENOMINATOR | 1 | - | - |
| ui/ui_champion.c:41 | `LOAD_WARNING_NUMERATOR` | `5` | engine/champion.h | HEAVY_LOAD_NUMERATOR | 1 | - | - |
| ui/ui_champion.c:42 | `LOAD_WARNING_DENOMINATOR` | `8` | engine/champion.h | HEAVY_LOAD_DENOMINATOR | 1 | - | - |
| engine/savegame.c:19 | `MIN_ACTIVE_GROUP_COUNT` | `110` | engine/game.h | ACTIVE_GROUP_MAX_COUNT (game.h:13), check meaning | 4 | - | csb-engine-diff.md superpowers/2026-10-04-csb-plan.md |
| data/spells.c:9 | `SPELL_EXPERIENCE_POWER_FACTOR` | `8` | data/spells.h | one set with engine/magic.c:54-56 CAST_EXPERIENCE_* | 1 | - | - |
| data/spells.c:10 | `SPELL_EXPERIENCE_LEVEL_FACTOR` | `16` | data/spells.h | merge | 1 | - | - |
| data/spells.c:11 | `SPELL_EXPERIENCE_RANDOM_MASK` | `7` | data/spells.h | merge | 1 | - | - |
| engine/magic.c:54 | `CAST_EXPERIENCE_POWER_FACTOR` | `8` | data/spells.h | SPELL_EXPERIENCE_POWER_FACTOR (R10.2 one spell API) | 1 | - | - |
| engine/magic.c:55 | `CAST_EXPERIENCE_LEVEL_FACTOR` | `16` | data/spells.h | SPELL_EXPERIENCE_LEVEL_FACTOR | 1 | - | - |
| engine/magic.c:56 | `CAST_EXPERIENCE_RANDOM_MASK` | `7` | data/spells.h | SPELL_EXPERIENCE_RANDOM_MASK | 1 | - | - |
| data/spells.c:14 | `SPELL_WISDOM_LIMIT` | `115` | engine/champion.h | check vs champion.c:79 PSYCHIC_WISDOM_LIMIT | 2 | - | - |
| engine/champion.c:79 | `PSYCHIC_WISDOM_LIMIT` | `115` | engine/champion.h | merge if same cap | 1 | - | - |
| engine/magic.c:27 | `POTION_POWER_PER_LEVEL` | `40` | engine/inventory.h | one name with ui/ui_champion.c:50 POTION_POWER_SYMBOL_STEP | 1 | - | - |
| ui/ui_champion.c:50 | `POTION_POWER_SYMBOL_STEP` | `40` | engine/inventory.h | POTION_POWER_PER_LEVEL | 1 | - | - |
| engine/action.c:20 | `MESSAGE_COLOR_ACTION` | `4` | engine/message.h | one MESSAGE_COLOR_CYAN with magic.c:20 MESSAGE_COLOR_SPELL (ui/ui.h COLOR_CYAN=4) | 1 | - | - |
| engine/magic.c:20 | `MESSAGE_COLOR_SPELL` | `4` | engine/message.h | merge | 3 | - | - |
| engine/action.c:23 | `SOUND_WAR_CRY` | `17` | engine/sound.h | SOUND_WAR_CRY | 1 | - | - |
| engine/action.c:24 | `SOUND_BLOW_HORN` | `18` | engine/sound.h | SOUND_BLOW_HORN | 1 | - | - |
| ui/startend.c:86 | `SOUND_SCREAM_DEATH` | `7` | engine/sound.h | SOUND_SCREAM (sound.h:18) | 1 | - | - |
| ui/startend.c:86 | `SOUND_FUSE` | `14` | engine/sound.h | SOUND_BUZZ or SOUND_TELEPORT (both 14 in sound.h:12,18: also a duplicate there) | 2 | - | - |
| ui/gameloop.c:155 | `SOUND_ENTRANCE_DOOR` | `3` | engine/sound.h | new SOUND_* in sound.h | 1 | - | - |
| engine/sound.c:21 | `SOUND_MAX_VOLUME` | `64` | engine/sound.h | one SOUND_MAX_VOLUME; also ui/gameloop.c:137 BUTTON_VOLUME, ui/gameloop.c:155 DOOR_VOLUME, ui/logo.c:24 LOGO_VOLUME (all 64 = full volume) | 2 | - | - |
| ui/gameloop.c:137 | `BUTTON_VOLUME` | `64` | engine/sound.h | SOUND_MAX_VOLUME | 2 | - | - |
| ui/gameloop.c:155 | `DOOR_VOLUME` | `64` | engine/sound.h | SOUND_MAX_VOLUME | 2 | - | - |
| ui/logo.c:24 | `LOGO_VOLUME` | `64` | engine/sound.h | SOUND_MAX_VOLUME | 2 | - | - |
| engine/sound.c:17 | `SOUND_SAMPLE_HEADER` | `2` | engine/sound.h | check vs ui/anim.c:26 SAMPLE_HEADER (both the length word before a sample) | 3 | - | - |
| ui/anim.c:26 | `SAMPLE_HEADER` | `2` | engine/sound.h | SOUND_SAMPLE_HEADER | 6 | - | - |
| ui/startend.c:61 | `MAX_SKILL_LEVEL` | `16` | data/tables.h | one name with ui/ui_champion.c:37 SKILL_LEVEL_MAXIMUM | 2 | - | - |
| ui/ui_champion.c:37 | `SKILL_LEVEL_MAXIMUM` | `16` | data/tables.h | keep, move | 2 | - | - |
| ui/ui_champion.c:38 | `BASE_SKILL_COUNT` | `4` | data/tables.h | SKILL_BASE_COUNT (tables.h:32) | 1 | - | - |
| engine/recruit.c:21 | `SKILL_BASE_EXPERIENCE` | `125` | data/tables.h or engine/champion.h | move if used elsewhere | 1 | - | - |
| ui/startend.c:70 | `THE_END_WHITE` | `0xfff` | ui/ui.h | RGB_WHITE (ui.h:43) | 2 | - | - |
| platform/platform.c:21 | `POINTER_MAX` | `32` | ui/gameloop.h | POINTER_SIZE (gameloop.h:8) | 4 | - | - |
| data/tables.c:29 | `CREATURE_INFO_BYTES` | `26` | data/tables.h | one name with engine/map.c:15 CREATURE_INFO_SIZE | 1 | - | - |
| engine/map.c:15 | `CREATURE_INFO_SIZE` | `26` | data/tables.h | CREATURE_INFO_BYTES | 1 | - | - |
| engine/map.c:16 | `CREATURE_INFO_ATTRIBUTES` | `2` | data/tables.h | CREATURE_AT_ATTRIBUTES (tables.c:37) | 1 | - | - |
| data/csbimage.c:22 | `KAOS_ACTION_SET_BYTES` | `8` | data/tables.h | ACTION_SET_BYTES (tables.c:33) | 1 | - | - |
| ui/command.c:50 | `VIEW_CELL_COUNT` | `4` | engine/game.h | CELL_COUNT (game.h:9) | 2 | - | - |
| ui/command.c:50 | `VIEW_CELL_FRONT_FIRST` | `2` | ui/view.h | view.c:392 VIEW_CELL_* enum: export from view.h | 4 | - | - |
| ui/view.c:120 | `SQUARE_RANDOM_ORNAMENT` | `8` | engine/map.h | square bit next to SQUARE_* (map.h:17-24) | 2 | - | superpowers/2026-10-06-coding-standard-refactor-plan.md |
| ui/view.c:120 | `PIT_INVISIBLE` | `4` | engine/map.h | next to PIT_IMAGINARY, PIT_OPEN (map.h:20) | 1 | test_csb_magic_map.py | superpowers/2026-10-06-coding-standard-refactor-plan.md |
| ui/view.c:121 | `WALL_ORNAMENT_NORTH_FACE` | `8` | engine/map.h | WALL_ORNAMENT_BELOW (map.h:19), same bit | 1 | - | - |
| ui/view.c:656 | `DIRECTION_BITS` | `2` | engine/map.h | next to DIRECTION_* (map.h:12) | 1 | - | - |
| ui/view.c:656 | `POSITION_BITS` | `5` | engine/map.h | MAP_COORD_MASK (map.h:14, "5 bits") | 2 | - | - |
| ui/view.c:667 | `THING_CELL_BITS` | `0xc000` | engine/map.h | next to THING_CELL (map.h:39) | 2 | - | - |
| ui/view.c:155 | `DOOR_ANIMATED` | `0x400` | data/tables.h (DoorInfo.attributes) | next to the door attribute bits; projectile.c:23 DOOR_PROJECTILES_PASS (=2) is the same field | 1 | - | - |
| engine/projectile.c:23 | `DOOR_PROJECTILES_PASS` | `2` | data/tables.h | same | 1 | - | - |
| engine/projectile.c:25 | `OBJECT_PASSES_DOOR_GRATE` | `0x0100` | data/tables.h (ObjectInfo) | move | 1 | - | - |
| engine/projectile.c:21 | `CREATURE_KEEPS_THROWN_SHARP_WEAPONS` | `0x0400` | engine/group.h (creature attributes) | move into the CREATURE_* attribute enum | 2 | - | - |
| ui/view.c:1671 | `PRESSURE_PAD` | `1` | data/ids.h? (floor ornament ordinals) | check other users of floor ornament ordinals | 1 | - | - |
| ui/layout.c:6 | `LAYOUT_GRAPHIC` | `696` | data/ids.h | GFX_LAYOUT | 1 | - | csb-data.md |
| platform/main.c:29 | `ICON_GRAPHIC` | `616` | data/ids.h | GFX_APP_ICON (a graphic id, not an object icon) | 1 | - | - |
| ui/dialog.c:21 | `GFX_DIALOG` | `0` | data/ids.h | move | 1 | - | - |
| ui/dialog.c:22 | `ZONE_DIALOG_ON_SCREEN` | `5` | data/ids.h | move (also ZONE_VERSION 450, ZONE_PATCH_* 451-453, ZONE_CHOICE_* 462-467, ZONE_MESSAGE_ONE_CHOICE 469, ZONE_MESSAGE 471: dialog.c:23-36) | 1 | - | - |
| ui/dialog.c:27 | `LAYOUT_PATCH_ONE_CHOICE` | `54` | data/ids.h | move (also LAYOUT_PATCH_TWO_CHOICES 55, LAYOUT_PATCH_FOUR_CHOICES 56) | 1 | - | - |
| ui/startend.c:36 | `GFX_CREDITS` | `5` | data/ids.h | move (also GFX_THE_END 6, GFX_CHAMPION_FRAME 346: startend.c:37-38) | 1 | - | csb-port-integration.md csb-data.md |
| ui/startend.c:39 | `ZONE_THE_END` | `5` | data/ids.h | move (also ZONE_RESTART_BUTTON 437, ZONE_QUIT_BUTTON 438, ZONE_CHAMPION_FRAME_FIRST 412, ZONE_CHAMPION_PORTRAIT_FIRST 416: startend.c:40-43) | 1 | - | - |
| ui/command.c:47 | `ZONE_THROW_LEFT` | `745` | data/ids.h | move (also ZONE_THROW_DOOR_LEFT 748, command.c:48) | 3 | - | - |
| ui/view.c:109 | `LAYOUT_STAIRS_UP_FRONT_D3L2` | `-33` | data/ids.h | move (also LAYOUT_STAIRS_*_D3R2/D3L2 -34..-36, LAYOUT_INSCRIPTION_WALL -51: view.c:109-110) | 2 | - | - |
| ui/view.c:31 | `ORNAMENT_VI_ALTAR` | `2` | data/ids.h or engine/map.h | check against command.c vi altar handling (R8) | 1 | test_csb_magic_map.py | - |
| ui/ui.c:46 | `ICON_SHEET_COUNT` | `7` | data/ids.h | next to GFX_OBJECT_ICONS_FIRST (check) | 1 | - | - |
| ui/ui.c:44 | `SLOT_BOX_COUNT` | `46` | ui/ui.h | next to UI_SLOT_BOX_* (ui.h:47) | 5 | - | - |
| ui/ui.c:45 | `SLOT_BOX_SCREEN_COUNT` | `8` | ui/ui.h | UI_SLOT_BOX_INVENTORY_FIRST (ui.h:47, =8) | 1 | - | - |
| ui/command.c:46 | `HAND_SLOT_BOX_COUNT` | `(2 * CHAMPION_MAX_COUNT)` | ui/ui.h | same as SLOT_BOX_SCREEN_COUNT / UI_SLOT_BOX_INVENTORY_FIRST | 1 | - | - |
| engine/inventory.c:319 | `HAND_BOX_COUNT` | `8` | ui/ui.h | same value (engine cannot include ui: keep in engine/game.h) | 2 | - | - |
| engine/savefile.c:33 | `DIALOG_CHOICE_SAVE` | `1` | ui/startend.h? (layer: engine) | check vs startend.c:83 SAVE_CHOICE_CANCEL=1/SAVE_CHOICE_SAVE=2 | 1 | - | - |
| engine/savefile.c:32 | `HEADER_BYTE_NEW_ADVENTURE` | `2` | engine/savefile.h | same value as ui/startend.h:30 SAVED_NEW_ADVENTURE (other meaning: return code); rename with H_BYTE_12A (R7) | 1 | - | - |
| data/ids.h:226 | `ZONE_SPELL_CASTER_TAB_FIRST` | `224` | data/ids.h | ZONE_SPELL_TAB_FIRST (ids.h:68), same value and comment | 2 | - | - |
| engine/group.h:57 | `GROUP_RECORD_BEHAVIOR_BYTE` | `15` | engine/map.h (owns the record layout) | GROUP_LAST_CREATURE_BYTE (map.h:210) is the same byte; one name, e.g. GROUP_BEHAVIOR_BYTE, in map.h | 5 | - | - |
| data/tables.h:20 | `SYMBOL_STEP_COUNT` | `4` | data/tables.h | CHAMPION_SYMBOL_COUNT (champion.h:12): same 4 rune steps, check | 2 | - | - |
| engine/projectile.h:54 | `PROJECTILE_EVENT_X_SHIFT` | `11` | engine/game.h | POSITION_X_SHIFT/POSITION_Y_SHIFT/POSITION_XY_MASK (game.h:65): same packed x/y format (with R6 accessors) | 2 | - | - |
| engine/sound.h:12 | `SOUND_TELEPORT` | `14` | engine/sound.h | same sound id as SOUND_BUZZ (sound.h:18); keep both only if the game meaning differs | 2 | - | - |

Notes on section 1:
- Checked and not duplicates (same value only): `FLEE_DELAY_FACTOR` (action.c:90) vs `FLEE_DELAY_FEAR_STEP` (group.c:51), two formulas; `THROW_DISABLED_TICKS` (action.c:58) is the action disable, not the movement disable; `MESSAGE_SIZE` (action.c:21) is a buffer; `HIDDEN_SKILLS_PER_BASE` (recruit.c:22) equals `SKILL_BASE_COUNT` by chance (keep, move to data/tables.h next to the skill enum); `SAVED_NEW_ADVENTURE` (ui/startend.h:30) is a return code, not the header byte.
- `DOOR_DAMAGE` (group.c:32, creatures under a closing door) and `DOOR_CLOSE_DAMAGE` (timeline_process.c:20, champions under a closing door): same game value 5; merge only if the plan accepts one "closing door damage" for both.
- `HIT_PROBABILITY_NON_MATERIAL` (action.c:34) is set by action.c:233 and read as `HIT_PROBABILITY_ALWAYS_HITS` by group.c:1205: one flag, two names.
- `PROJECTILE_DISABLED_MOVEMENT_TICKS` (action.c:62) and `SHOT_DISABLED_MOVEMENT_TICKS` (projectile.c:48) both set `g_projectile_disabled_movement_ticks`.
- `MIN_ACTIVE_GROUP_COUNT` (savegame.c:19) is the DM `ACTIVE_GROUP_MAX_COUNT` (game.h:13, used at game.c:296).
- `RECRUIT_STAT_MINIMUM` (recruit.c:20) and `NEW_ADVENTURE_STAT_MINIMUM` (savegame.c:24) both set `statistics[..][STAT_MINIMUM]` to 30.
- `SPELL_WISDOM_LIMIT` (data/spells.c:14, used spells.c:80) and `PSYCHIC_WISDOM_LIMIT` (champion.c:79, used champion.c:677) are both the wisdom cap 115; data/ cannot include engine/, so the owner is data/spells.h or data/tables.h.
- `DoorInfo.attributes` is `uint8_t` (data/tables.h:82, tests/test_tables.py:35), but ui/view.c:1549 reads the door attribute as a word (`kaos_u16`) and tests `DOOR_ANIMATED = 0x400` (view.c:155); projectile.c:280 reads `door_attributes()` with `DOOR_PROJECTILES_PASS = 2`. One door attribute enum belongs in data/tables.h; check which field each reads.
- Layer limits: engine/ cannot include ui/ (ui/ui.h `COLOR_CYAN` cannot serve engine `MESSAGE_COLOR_*`; `HAND_BOX_COUNT` in engine/inventory.c:319 stays in engine/). platform/ may include ui/gameloop.h (`POINTER_SIZE`).
- Families already in the right place (no action): `CREATURE_AT_*` (data/tables.c:37-41, private layout of the table loader), `REC_*` (engine/champion.c:116-121), `P_*`, `E_*`, `A_*` (engine/savegame.c:30-37), `H_*` (engine/savefile.c:10-26), `WS_*`, `WALLSET_*`, `VIEW_SQUARE_*`, `VIEW_WALL_*`, `MAP_TILE_*`, `CELLS_*`, `DOOR_BACK_*`, `DOOR_FRONT_*` (ui/view.c, private to the view).
- The same-name check found one name defined twice: `SEND_FLAGS` (platform/control.c:47 and :49), an `#ifdef` pair; no action.

## 2. R7 renames

### 2a. Struct fields and constants with event numbers, byte offsets or bit numbers

| file:line | name | kind | proposed name | meaning / users | uses | tests | docs |
|---|---|---|---|---|---|---|---|
| engine/game.h:86 | `event73_count_thieves_eye` | Party field | `thieves_eye_count` | count of active Thieves Eye spells; magic.c:244, action.c:335 add, timeline_process.c:436 removes | 6 | - | game-state.md |
| engine/game.h:86 | `event79_count_footprints` | Party field | `footprints_count` | count of active Footprints spells; magic.c:269, move.c:103, timeline_process.c:444 | 6 | - | game-state.md |
| engine/game.h:91 | `event71_count_invisibility` | Party field | `invisibility_count` | count of active Invisibility spells; magic.c:249, group.c:750/1098, ui_champion.c:179/205 | 9 | - | game-state.md |
| engine/savefile.c:14 | `H_BYTE_12A` | header offset 0x12a | `H_SAVE_KIND` | the header byte that tells a saved game (1) from a new CSB adventure made by CEDT (2); savefile.c:160,176 | 3 | - | - |
| engine/savefile.c:31 | `HEADER_BYTE_12A` | value 1 | `SAVE_KIND_GAME` | default value of that byte | 2 | - | - |
| engine/savefile.c:32 | `HEADER_BYTE_NEW_ADVENTURE` | value 2 | `SAVE_KIND_NEW_ADVENTURE` | value for a new adventure | 2 | - | - |
| data/tables.h:98 | `byte_6` | ActionSet field | `flags (or unused_flags)` | byte 6 of an action set; copied to 0x103fea, no reader known (docs/tables.md:138); csbimage.c:23 calls it KAOS_ACTION_SET_FLAGS | 1 | test_tables.py | tables.md |
| engine/champion.c:98-100 | `TIME_BIT_6, TIME_BIT_7, TIME_BIT_8` | bits of g_game_time | `optional: one comment-named shuffle, e.g. TIME_EFFECT_CHANCE_BITS; or keep (bit names, not offsets)` | champion.c:1001 shuffles time bits 6-8 to a chance 0-112 | 2 | - | - |
| engine/savegame.c:35 | `E_B, E_C` | byte offsets of TimelineEvent b, c in the save | `rename with the R6 accessors of TimelineEvent.b/.c (E_DATA_B/E_DATA_C)` | savegame.c:137-147 | 3 | - | - |
| data/csbimage.c:22-23 | `KAOS_ACTION_SET_BYTES, APPB_ACTION_SET_*` | layout | `keep; KAOS_ACTION_SET_BYTES merges with ACTION_SET_BYTES (R3)` |  | 2 | - | - |

Notes: tests/test_champion.py:54 `Party` already uses the names `thieves_eye`, `footprints`, `invisibility` for these three fields (only the field names differ; layout is the same). tests/test_tables.py:37 `ActionSet` names `byte_6`. docs/game-state.md:34,35,45 and docs/tables.md:138 need the new names; docs/kaos-symbols.csv:291-292 and docs/symbols/range-7-9.md keep the original `g_event7x_*` names (they are original symbols). The other `*_BYTE` names in map.h/group.h (`GROUP_DIRECTION_BYTE`, `MAP_*_BYTE`) name the field, not the offset: no rename.

### 2b. Noun-only or cryptic function names

Static functions: "uses" = calls `name(` in the same file. Public functions: calls in all of port/src. "tests" = word hits in tests/*.py (ctypes binding by name for public functions; for generic static names a hit is usually an unrelated word). "docs" = `` `name` `` or `name(` in docs/*.md.

| file:line | name | proposed | scope | uses | tests | docs |
|---|---|---|---|---|---|---|
| ui/view.c:235 | `square` | `get_square` | static | 5 | test_actions.py test_automap.py test_creatures.py test_control.py test_champion.py test_csb_magic_map.py test_csb_rules.py test_dungeon.py test_gameloop.py test_music.py test_move.py test_recruit.py test_view.py (word hit only; static) | - |
| ui/view.c:252 | `random_ordinal` | `get_random_ornament_ordinal` | static | 2 | - | - |
| ui/view.c:267 | `random_wall_ornaments` | `set_random_wall_ornaments` | static | 1 | - | superpowers/2026-10-06-coding-standard-refactor-plan.md  |
| ui/view.c:301 | `aspect` | `set_square_aspect` | static | 20 | test_creatures.py (word hit only; static) | superpowers/2026-10-04-csb-plan.md  |
| ui/view.c:353 | `wall` | `draw_wall_set_bitmap` | static | 65 | test_actions.py test_automap.py test_champion.py test_control.py test_csb_ending.py test_csb_magic_map.py test_gameloop.py test_recruit.py (word hit only; static) | llm-playbook.md  |
| ui/view.c:374 | `in_zone` | `draw_bitmap_in_zone` | static | 11 | - | - |
| ui/view.c:436 | `shrunk` | `get_shrunk_bitmap` | static | 7 | - | - |
| ui/view.c:447 | `object_pal` | `get_object_palette_changes` | static | 2 | - | - |
| ui/view.c:452 | `object_aspect` | `get_object_aspect` | static | 2 | - | - |
| ui/view.c:455 | `projectile_aspect` | `get_projectile_aspect` | static | 3 | - | superpowers/2026-10-04-csb-plan.md  |
| ui/view.c:501 | `object` | `draw_object` | static | 2 | test_control.py test_gameloop.py test_modes.py test_move.py test_panels.py test_save.py (word hit only; static) | llm-playbook.md  |
| ui/view.c:538 | `group` | `load_group_aspect` | static | 1 | test_actions.py test_creatures.py test_control.py test_csb_magic_map.py test_csb_text.py test_save.py test_standard.py (word hit only; static) | superpowers/2026-10-06-coding-standard-refactor-plan.md  |
| ui/view.c:557 | `creature_cell` | `get_creature_cell` | static | 3 | - | - |
| ui/view.c:558 | `creature_direction` | `get_creature_direction` | static | 1 | - | - |
| ui/view.c:561 | `ordinal_in_cell` | `get_creature_ordinal_in_cell` | static | 1 | - | - |
| ui/view.c:577 | `creature` | `draw_creature` | static | 1 | test_actions.py test_creatures.py test_control.py test_csb_text.py test_gameloop.py (word hit only; static) | llm-mode.md llm-playbook.md  |
| ui/view.c:611 | `creatures` | `draw_creatures_in_cell` | static | 1 | test_actions.py test_creatures.py test_control.py test_modes.py test_view.py (word hit only; static) | llm-playbook.md  |
| ui/view.c:707 | `projectile` | `draw_projectile` | static | 1 | test_creatures.py (word hit only; static) | llm-playbook.md  |
| ui/view.c:770 | `explosion_bitmap` | `get_explosion_bitmap` | static | 1 | - | - |
| ui/view.c:777 | `explosion_on_party` | `draw_explosion_on_party` | static | 1 | - | - |
| ui/view.c:796 | `explosions` | `draw_explosions` | static | 1 | test_control.py test_creatures.py test_csb_rules.py (word hit only; static) | - |
| ui/view.c:850 | `things` | `draw_square_things` | static | 44 | test_actions.py test_champion.py test_creatures.py test_csb_rules.py test_csb_text.py test_csb_magic_map.py test_dungeon.py test_move.py test_recruit.py test_panels.py test_view.py (word hit only; static) | - |
| ui/view.c:900 | `field` | `draw_teleporter_field` | static | 16 | test_port_data.py (word hit only; static) | superpowers/2026-10-04-csb-plan.md  |
| ui/view.c:906 | `scaled` | `get_scaled_bitmap` | static | 5 | - | - |
| ui/view.c:915 | `wall_text` | `draw_wall_text` | static | 1 | - | - |
| ui/view.c:933 | `wall_portrait` | `draw_wall_portrait` | static | 1 | - | - |
| ui/view.c:944 | `wall_ornament` | `draw_wall_ornament` | static | 15 | - | - |
| ui/view.c:988 | `floor_ornament` | `draw_floor_ornament` | static | 23 | test_control.py (word hit only; static) | llm-playbook.md llm-mode.md  |
| ui/view.c:1003 | `door_ornament` | `draw_door_ornament` | static | 2 | - | - |
| ui/view.c:1019 | `door_button` | `draw_door_button` | static | 4 | - | - |
| ui/view.c:1035 | `door` | `draw_door` | static | 12 | test_control.py test_csb_magic_map.py test_dungeon.py test_entrance.py test_move.py test_view.py (word hit only; static) | llm-playbook.md superpowers/2026-10-06-coding-standard-refactor-plan.md  |
| ui/view.c:1064 | `level_change` | `get_location_after_level_change` | static | 1 | - | - |
| ui/view.c:1079 | `ceiling_pit` | `draw_ceiling_pit` | static | 11 | - | - |
| ui/view.c:1086 | `stairs_front` | `draw_stairs_front` | static | 11 | - | - |
| ui/view.c:1096 | `d3l2` | `draw_square_d3l2 (R9 replaces d3l2..d0c with one table-driven draw_view_square)` | static | 0 | - | - |
| ui/view.c:1123 | `d3r2` | `draw_square_d3r2` | static | 0 | - | - |
| ui/view.c:1150 | `d3l` | `draw_square_d3l` | static | 0 | - | - |
| ui/view.c:1180 | `d3r` | `draw_square_d3r` | static | 0 | - | - |
| ui/view.c:1211 | `d3c` | `draw_square_d3c` | static | 0 | - | - |
| ui/view.c:1239 | `d2l2` | `draw_square_d2l2` | static | 0 | - | - |
| ui/view.c:1247 | `d2r2` | `draw_square_d2r2` | static | 0 | - | - |
| ui/view.c:1255 | `d2l` | `draw_square_d2l` | static | 0 | - | - |
| ui/view.c:1289 | `d2r` | `draw_square_d2r` | static | 0 | - | - |
| ui/view.c:1320 | `d2c` | `draw_square_d2c` | static | 0 | - | - |
| ui/view.c:1350 | `d1l` | `draw_square_d1l` | static | 0 | - | - |
| ui/view.c:1378 | `d1r` | `draw_square_d1r` | static | 0 | - | - |
| ui/view.c:1406 | `d1c` | `draw_square_d1c` | static | 0 | - | - |
| ui/view.c:1438 | `d0l` | `draw_square_d0l` | static | 0 | - | - |
| ui/view.c:1453 | `d0r` | `draw_square_d0r` | static | 0 | - | - |
| ui/view.c:1468 | `d0c` | `draw_square_d0c` | static | 0 | - | - |
| ui/view.c:1488 | `replacement_colors` | `set_creature_replacement_colors` | static | 4 | - | - |
| ui/view.c:1501 | `entry_slot` | `get_wall_set_slot` | static | 4 | - | - |
| ui/view.c:1633 | `step` | `get_relative_square` | static | 5 | test_control.py test_entrance.py test_music.py test_spells.py test_tables.py (word hit only; static) | tables.md  |
| ui/view.c:692 | `view_projectiles_moving` | `view_has_moving_projectiles` | public | 2 | - | - |
| ui/view.c:1640 | `view_wall_ornament` | `view_get_wall_ornament` | public | 2 | test_csb_magic_map.py  | - |
| ui/view.c:1654 | `view_wall_face_ornament_index` | `view_get_wall_face_ornament_index` | public | 2 | - | - |
| ui/view.c:1663 | `view_wall_ornament_index` | `view_get_wall_ornament_index` | public | 3 | - | - |
| ui/view.c:1669 | `view_floor_pressure_pad` | `view_is_pressure_pad` | public | 3 | - | - |
| ui/view.c:1676 | `view_floor_ornament` | `view_get_floor_ornament` | public | 3 | test_csb_magic_map.py  | - |
| ui/view.c:1704 | `map_tile` | `draw_map_tile` | static | 13 | - | - |
| ui/view.c:1712 | `map_footprints` | `has_map_footprints` | static | 1 | - | superpowers/2026-10-04-csb-plan.md  |
| ui/view.c:1721 | `map_projectile_tile` | `get_map_projectile_tile` | static | 1 | - | - |
| ui/view.c:1738 | `map_explosion_tile` | `get_map_explosion_tile` | static | 1 | - | - |
| ui/view.c:1750 | `map_square_things` | `draw_map_square_things` | static | 1 | - | superpowers/2026-10-06-coding-standard-refactor-plan.md  |
| ui/view.c:1777 | `map_square` | `draw_map_square` | static | 2 | - | - |
| data/graphics.c:55 | `gfx_item` | `gfx_get_item` | public | 6 | test_port_data.py test_sound.py test_ui.py test_view.py  | - |
| data/graphics.c:63 | `graphic_size` | `get_graphic_size` | static | 2 | - | - |
| data/graphics.c:156 | `gfx_bitmap` | `gfx_get_bitmap` | public | 26 | test_champion.py test_csb_magic_map.py test_ui.py  | - |
| data/ftl.c:75 | `nib` | `read_nibble` | static | 3 | - | - |
| data/ftl.c:239 | `at` | `get_image_pointer` | static | 4 | csb_diff.py test_automap.py test_control.py test_csb_ending.py test_csb_engine.py test_entrance.py test_gameloop.py test_panels.py test_ftl.py test_port_data.py test_text.py test_view.py test_ui.py (word hit only; static) | - |
| data/csbtext.c:87 | `appb_string` | `get_appb_string` | static | 5 | - | - |
| data/csbtext.c:95 | `appb_pointed_string` | `get_appb_pointed_string` | static | 3 | - | - |
| data/csbtext.c:169 | `csb_text_item` | `csb_text_get_item` | public | 2 | - | csb-engine-diff.md superpowers/2026-10-04-csb-plan.md  |
| data/spells.c:18 | `spells_address` | `get_spells_address` | static | 2 | - | superpowers/2026-10-04-csb-plan.md  |
| data/spells.c:27 | `spell_kind` | `spell_get_kind` | public | 1 | test_spells.py  | - |
| data/spells.c:28 | `spell_type` | `spell_get_type` | public | 1 | test_csb_magic_map.py test_spells.py  | - |
| data/spells.c:29 | `spell_disabled_ticks` | `spell_get_disabled_ticks` | public | 1 | test_spells.py  | - |
| data/spells.c:32 | `power_index` | `get_power_index` | static | 3 | - | - |
| data/spells.c:54 | `spell_symbol` | `spell_get_symbol` | public | 1 | test_spells.py  | - |
| data/spells.c:59 | `spell_symbol_mana_cost` | `spell_get_symbol_mana_cost` | public | 1 | test_spells.py  | - |
| data/spells.c:67 | `spell_required_level` | `spell_get_required_level` | public | 2 | test_spells.py  | - |
| data/spells.c:72 | `spell_experience` | `spell_get_experience` | public | 1 | test_spells.py  | superpowers/2026-10-06-coding-standard-refactor-plan.md  |
| data/spells.c:85 | `spell_failure_experience` | `spell_get_failure_experience` | public | 2 | test_spells.py  | - |
| data/tables.c:172 | `tables_creature_info` | `tables_get_creature_info (same for the 13 tables_* accessors at tables.c:173-190)` | public | 11 | test_tables.py  | tables.md rules-creatures.md  |
| data/tables.c:174 | `tables_object_info` | `tables_get_object_info` | public | 12 | test_champion.py test_gameloop.py test_panels.py  | tables.md  |
| data/tables.c:175 | `tables_weapon_info` | `tables_get_weapon_info` | public | 15 | - | tables.md  |
| data/tables.c:179 | `tables_door_info` | `tables_get_door_info` | public | 1 | - | tables.md  |
| data/tables.c:180 | `tables_spell` | `tables_get_spell` | public | 2 | test_csb_engine.py test_csb_magic_map.py  | tables.md  |
| engine/action.c:284 | `freeze_life` | `cast_freeze_life` | static | 1 | - | - |
| engine/action.c:302 | `heal` | `heal_champion` | static | 1 | test_actions.py (word hit only; static) | - |
| engine/action.c:328 | `window` | `open_window` | static | 1 | conftest.py (word hit only; static) | - |
| engine/champion.c:188 | `square_in_front` | `get_square_in_front` | static | 2 | - | - |
| engine/champion.c:231 | `candidate_slot` | `get_candidate_slot` | static | 1 | - | - |
| engine/champion.c:415 | `base_skill` | `get_base_skill` | static | 2 | - | - |
| engine/champion.c:421 | `object_skill_bonus` | `get_object_skill_bonus` | static | 1 | - | - |
| engine/champion.c:623 | `armour_info` | `get_armour_info` | static | 3 | test_tables.py (word hit only; static) | - |
| engine/dungeon.c:122 | `text_words` | `count_text_words` | static | 4 | - | - |
| engine/dungeon.c:126 | `column_first_thing_index` | `get_column_first_thing_index` | static | 2 | - | - |
| engine/dungeon.c:246 | `valid_thing` | `is_valid_thing` | static | 1 | - | - |
| engine/dungeon.c:255 | `carried_first_thing` | `get_carried_first_thing` | static | 1 | - | - |
| engine/dungeon.c:270 | `valid_teleporter` | `is_valid_teleporter` | static | 1 | - | - |
| engine/dungeon.c:417 | `dungeon_square` | `dungeon_get_square` | public | 13 | test_automap.py test_creatures.py test_control.py test_dungeon.py test_move.py  | - |
| engine/dungeon.c:423 | `dungeon_square_first_thing` | `dungeon_get_square_first_thing` | public | 8 | test_champion.py test_creatures.py test_csb_text.py test_dungeon.py test_move.py test_recruit.py test_ui.py test_view.py  | dungeon-format.md  |
| engine/explored.c:39 | `seen_square` | `get_seen_square` | static | 2 | - | - |
| engine/game.c:58 | `game_new` | `game_start_new` | public | 2 | test_automap.py test_champion.py test_creatures.py test_game.py test_move.py test_ui.py  | rules-movement.md game-state.md csb-port-integration.md superpowers/2026-10-06-coding-standard-refactor-plan.md  |
| engine/group.c:96 | `group_thing_of_active` | `group_get_active_group_thing` | static | 2 | - | - |
| engine/group.c:97 | `cell_of` | `get_creature_cell` | static | 12 | - | - |
| engine/group.c:108 | `creature_movement_sound` | `get_creature_movement_sound` | static | 2 | - | - |
| engine/group.c:673 | `line_slope` | `get_line_slope` | static | 3 | - | - |
| engine/group.c:768 | `party_scent_ordinal` | `get_party_scent_ordinal` | public | 3 | - | superpowers/2026-10-04-csb-plan.md  |
| engine/group.c:899 | `creature_spell_explosion` | `get_creature_spell_explosion` | static | 1 | - | - |
| engine/group.c:1381 | `archenemy_jump` | `jump_archenemy` | static | 3 | - | - |
| engine/inventory.c:185 | `object_modifier` | `get_object_modifier` | static | 1 | - | - |
| engine/inventory.c:238 | `slot_ptr` | `get_slot_pointer` | static | 3 | - | - |
| engine/magic.c:167 | `empty_flask_in_hand` | `find_empty_flask_in_hand` | static | 1 | - | - |
| engine/map.c:160 | `fatal` | `stop_fatal` | static | 1 | - | - |
| engine/message.c:43 | `new_row` | `start_new_row` | static | 2 | - | - |
| engine/projectile.c:59 | `door_attributes` | `get_door_attributes` | static | 1 | - | - |
| engine/projectile.c:302 | `flask_explosion_of` | `get_flask_explosion` | static | 1 | - | superpowers/2026-10-06-coding-standard-refactor-plan.md  |
| engine/projectile.c:700 | `square_object_in_cells` | `find_square_object_in_cells` | static | 2 | - | - |
| engine/recruit.c:50 | `hidden_skill_experience` | `get_hidden_skill_experience` | static | 1 | - | superpowers/2026-10-06-coding-standard-refactor-plan.md  |
| engine/savefile.c:36 | `field` | `get_header_field_offset` | static | 20 | test_port_data.py (word hit only; static) | superpowers/2026-10-04-csb-plan.md  |
| engine/savefile.c:70 | `header_chain` | `compute_header_chain` | static | 2 | - | - |
| engine/savegame.c:251 | `file_path` | `make_file_path` | static | 4 | - | - |
| engine/savegame.c:321 | `first_names_unique` | `are_first_names_unique` | static | 1 | - | - |
| engine/sound.c:62 | `sound_sample` | `get_sound_sample` | public | 2 | test_sound.py  | - |
| engine/sound.c:75 | `loudness_class` | `get_loudness_class` | static | 1 | - | - |
| engine/sound.c:108 | `voice_position` | `get_voice_position` | static | 2 | - | - |
| engine/sound.c:113 | `voice_busy` | `is_voice_busy` | static | 3 | - | - |
| engine/timeline_process.c:64 | `event_door_animation` | `process_door_animation_event` | public | 2 | - | rules-movement.md superpowers/2026-10-06-coding-standard-refactor-plan.md  |
| engine/timeline_process.c:104 | `event_door_destruction` | `process_door_destruction_event` | public | 2 | - | rules-movement.md  |
| engine/timeline_process.c:111 | `event_door` | `process_door_event` | public | 2 | - | rules-movement.md  |
| engine/timeline_process.c:125 | `event_fake_wall` | `process_fake_wall_event` | public | 2 | - | rules-movement.md  |
| engine/timeline_process.c:189 | `event_corridor` | `process_corridor_event` | public | 2 | - | rules-movement.md  |
| engine/timeline_process.c:243 | `gate` | `gate_sensor_effect (check)` | static | 1 | - | - |
| engine/timeline_process.c:256 | `event_wall` | `process_wall_event` | public | 2 | test_csb_ending.py  | rules-movement.md  |
| engine/timeline_process.c:404 | `event_vi_altar_rebirth` | `process_vi_altar_rebirth_event` | public | 2 | - | rules-movement.md rules-champions.md  |
| ui/automap.c:46 | `automap_visible_depths` | `automap_get_visible_depths` | public | 3 | test_automap.py test_gameloop.py  | superpowers/2026-10-06-coding-standard-refactor-plan.md  |
| ui/command.c:88 | `current_input` | `get_current_input` | static | 2 | - | - |
| ui/command.c:201 | `front_square` | `get_front_square` | static | 4 | - | - |
| ui/command.c:470 | `in_box` | `is_in_box` | static | 3 | - | - |
| ui/command.c:648 | `left_release` | `process_left_release` | static | 1 | - | - |
| ui/command.c:742 | `movement_blocked` | `is_movement_blocked` | static | 1 | - | - |
| ui/gameloop.c:85 | `gameloop_data_dir` | `gameloop_get_data_dir` | public | 5 | - | llm-mode.md csb-port-integration.md superpowers/2026-10-04-csb-plan.md  |
| ui/gameloop.c:295 | `gameloop_new_game` | `gameloop_start_new_game` | public | 4 | test_control.py test_csb_adventure.py test_csb_magic_map.py test_csb_engine.py test_csb_rules.py test_csb_save.py test_csb_text.py test_gameloop.py test_modes.py test_save.py test_select.py  | csb-port-integration.md  |
| ui/gameloop.c:374 | `gameloop_frame` | `gameloop_run_frame` | public | 2 | test_control.py test_csb_engine.py test_gameloop.py test_modes.py  | llm-mode.md superpowers/2026-10-06-coding-standard-refactor-plan.md  |
| ui/gameloop.c:470 | `planar_pointer` | `decode_planar_pointer` | static | 1 | - | - |
| ui/gameloop.c:487 | `object_pointer` | `make_object_pointer` | static | 1 | - | - |
| ui/gameloop.c:510 | `pointer_shape` | `get_pointer_shape` | static | 1 | - | - |
| ui/gameloop.c:530 | `gameloop_pointer` | `gameloop_get_pointer` | public | 2 | test_gameloop.py  | - |
| ui/gameloop.c:541 | `gameloop_pointer_palette` | `gameloop_get_pointer_palette` | public | 2 | - | - |
| ui/layout.c:23 | `zone_type` | `get_zone_type (also zone_parent, zone_x, zone_y: layout.c:24-26)` | static | 8 | - | - |
| ui/select.c:40 | `enabled` | `is_item_enabled` | static | 3 | - | - |
| ui/startend.c:301 | `csb_box` | `get_csb_box` | static | 1 | - | - |
| ui/startend.c:374 | `endgame` | `run_endgame` | public | 6 | test_csb_ending.py test_modes.py  | csb-port-integration.md kaos-map.md csb-engine-diff.md csb-data.md symbols/range-7-9.md superpowers/2026-10-04-csb-plan.md superpowers/2026-10-06-coding-standard-refactor-plan.md  |
| ui/startend.c:425 | `fuse_explosions` | `draw_fuse_explosions` | static | 3 | - | - |
| ui/startend.c:477 | `fuse_sequence` | `run_fuse_sequence` | public | 2 | test_save.py  | csb-port-integration.md csb-engine-diff.md symbols/range-7-9.md  |
| ui/startend.c:519 | `csb_ending` | `play_csb_ending` | static | 1 | - | - |
| ui/startend.c:528 | `endgame_sensor` | `run_endgame_sensor` | public | 1 | test_csb_ending.py  | csb-port-integration.md csb-data.md csb-engine-diff.md superpowers/2026-10-06-coding-standard-refactor-plan.md superpowers/2026-10-04-csb-plan.md  |
| ui/text.c:58 | `text_size` | `get_text_size` | public | 8 | test_csb_magic_map.py  | csb-port-integration.md  |
| ui/ui_champion.c:77 | `number` | `format_number` | static | 8 | test_csb_ending.py test_csb_adventure.py test_csb_rules.py test_standard.py (word hit only; static) | - |
| ui/ui_champion.c:113 | `value_pair` | `draw_value_pair` | static | 3 | - | - |
| ui/ui_champion.c:190 | `ui_champion_portrait` | `ui_draw_champion_portrait` | public | 3 | - | superpowers/2026-10-06-coding-standard-refactor-plan.md  |
| ui/ui_champion.c:340 | `food_water_bar` | `draw_food_water_bar` | static | 2 | - | - |
| ui/ui_champion.c:388 | `zone_point` | `get_zone_point` | static | 5 | - | - |
| ui/ui_champion.c:397 | `ui_skill_rank_name` | `ui_get_skill_rank_name` | public | 3 | test_csb_text.py  | - |
| ui/ui_champion.c:476 | `attributes_text` | `format_attributes_text` | static | 1 | - | - |
| ui/ui_champion.c:497 | `panel_object_text` | `draw_panel_object_text` | public | 3 | test_panels.py  | - |
| ui/ui_champion.c:575 | `rune_of_word` | `get_rune_of_word` | static | 1 | - | - |
| ui/ui_champion.c:710 | `largest_defense` | `get_largest_defense` | static | 1 | - | - |
| ui/ui_champion.c:717 | `largest_weight` | `get_largest_weight` | static | 1 | - | - |
| ui/ui_champion.c:746 | `panel_object_meter` | `draw_panel_object_meter` | public | 3 | - | - |
| ui/ui.c:69 | `ui_screen` | `ui_get_screen` | public | 39 | - | - |
| ui/ui.c:70 | `ui_viewport` | `ui_get_viewport` | public | 29 | test_csb_magic_map.py test_panels.py  | - |
| ui/ui.c:234 | `ui_icon_zone_box` | `ui_get_icon_zone_box` | public | 5 | - | - |
| ui/ui.c:246 | `ui_slot_box_zone` | `ui_get_slot_box_zone` | public | 3 | test_gameloop.py test_panels.py test_save.py  | - |
| ui/ui.c:271 | `junk_charges` | `get_junk_charges` | static | 0 | - | - |
| ui/ui.c:292 | `ui_point_in_zone` | `ui_is_point_in_zone` | public | 8 | - | - |
| ui/video.c:13 | `video_color` | `video_get_color` | public | 2 | test_modes.py test_ui.py  | - |
| ui/video.c:19 | `video_argb` | `video_convert_argb` | public | 5 | test_modes.py test_port.py  | - |
| platform/control.c:121 | `game_over` | `is_game_over` | static | 6 | test_control.py test_modes.py (word hit only; static) | llm-mode.md llm-playbook.md superpowers/2026-10-04-csb-plan.md  |
| platform/control.c:138 | `viewport_origin` | `get_viewport_origin` | static | 2 | - | - |
| platform/control.c:237 | `champion_arg` | `parse_champion_arg` | static | 5 | - | - |
| platform/control.c:251 | `movement` | `parse_movement` | static | 2 | test_tables.py (word hit only; static) | tables.md  |
| platform/control.c:385 | `look` | `send_look` | static | 0 | test_control.py (word hit only; static) | llm-playbook.md llm-mode.md  |
| platform/control.c:604 | `witness` | `record_witnessed` | static | 2 | - | - |
| platform/control.c:871 | `snapshot` | `take_snapshot` | static | 2 | - | - |
| platform/control.c:892 | `news` | `send_news` | static | 3 | - | - |
| platform/main.c:197 | `frame_hook` | `run_frame_hook` | static | 0 | - | csb-port-integration.md  |
| platform/main.c:280 | `game_mode_of` | `get_game_mode` | static | 1 | - | csb-port-integration.md  |
| platform/main.c:330 | `adventure_dir` | `get_adventure_dir` | static | 3 | - | csb-port-integration.md superpowers/2026-10-04-csb-plan.md  |
| platform/state_json.c:373 | `state_json_state` | `state_json_write_state (and the *_json builders: write_<x>_json)` | public | 3 | - | llm-mode.md  |
| platform/state_json.c:678 | `state_json_view` | `state_json_write_view` | public | 3 | - | llm-mode.md  |
| platform/state_json.c:725 | `state_json_visible_things` | `state_json_write_visible_things` | public | 4 | - | - |
| platform/state_json.c:741 | `state_json_visible_groups` | `state_json_write_visible_groups` | public | 3 | - | - |
| platform/state_json.c:821 | `state_json_string` | `state_json_get_string` | public | 1 | test_control.py  | llm-mode.md  |

Notes on 2b:
- The test column for static names (`square`, `object`, `door`, `field`, `step`, ...) cannot be a ctypes binding; they need no test change. Public names with test hits need the ctypes binding renamed in those files.
- Accessor families left as they are unless the plan says otherwise: `kaos_u8/u16/s16/u32/ptr` (data/ftl.c:250-252, engine/sound.c:51-54, ui/ui_champion.c:84), `rec_*` (engine/map.h), `min16`, `max16`, `abs16`, `scale2x`, `scale3x`, `sum8`, the JSON writers `*_json` of platform/state_json.c and platform/control.c (rename to `write_<x>_json` in one sweep if wanted: object_json, runes_json, skills_json, champion_json, messages_json, spell_json, inventory_json, ornament_json, wall_face_json, wall_faces_json, floor_ornament_json, square_things_json, piles_json, seen_json, health_lost_json, look_json).
- `d3l2` .. `d0c` (ui/view.c:1096-1468) go away in R9 (one table and one draw function); a rename before R9 is wasted work.
- Functions that already have a verb and were checked: `approach_target`, `follow_destination_squares`, `enter_destination`, `postpone_for_freeze_life`, `regenerate_mana`, `harm_non_material`, `flash_lords`, `highlight_zone`, `blocks_sight`, `align`, `anchor`, `clip_to`.
- Public `tables_*` accessors (data/tables.c:172-190, 14 functions): `tables_creature_sound`, `tables_armour_info`, `tables_junk_weight`, `tables_food_value`, `tables_symbol_mana_cost_multiplier`, `tables_action_info`, `tables_slot_mask`, `tables_wound_defense_factor`, `tables_fixed_possession`, `tables_symbol_base_mana_cost` take the same `tables_get_` change.

### 2c. Cryptic file-static variables

| file:line | name | proposed | note | word hits in file (includes locals of the same name) |
|---|---|---|---|---|
| ui/view.c:196 | `vp` | `viewport` | g_viewport_bitmap | 26 |
| ui/view.c:197 | `map` | `view_map` | current map | 59 |
| ui/view.c:197 | `mw` | `map_width` |  | 10 |
| ui/view.c:197 | `mh` | `map_height` |  | 10 |
| ui/view.c:199 | `slot_gfx` | `slot_graphics` |  | 10 |
| ui/view.c:201 | `wall_orn` | `wall_ornaments` |  | 5 |
| ui/view.c:201 | `floor_orn` | `floor_ornaments` |  | 7 |
| ui/view.c:201 | `door_orn` | `door_ornaments` |  | 5 |
| ui/view.c:202 | `door_gfx` | `door_graphics` |  | 3 |
| ui/view.c:203 | `door_attr` | `door_attributes` |  | 3 |
| ui/view.c:204 | `parity` | `use_flipped_walls` | g_use_flipped_wall_bitmaps | 4 |
| ui/view.c:204 | `rel_x` | `zone_relative_x` | g_zone_relative_x | 7 |
| ui/view.c:204 | `rel_y` | `zone_relative_y` | g_zone_relative_y | 6 |
| ui/view.c:205 | `pals` | `dungeon_palettes` | g_dungeon_palettes | 6 |
| ui/view.c:206 | `creature_d3` | `creature_palette_changes_d3` | g_palette_changes_creature_d3 | 5 |
| ui/view.c:206 | `creature_d2` | `creature_palette_changes_d2` | g_palette_changes_creature_d2 | 5 |
| platform/platform.c:8 | `win` | `window` |  | 3 |
| platform/platform.c:9 | `ren` | `renderer` |  | 12 |
| platform/platform.c:10 | `tex` | `texture` |  | 10 |
| platform/platform.c:20 | `big` | `scaled_screen` | upscaled frame buffer | 4 |
| data/graphics.c:20 | `dat` | `graphics_data` | GRAPHICS.DAT bytes | 7 |
| data/graphics.c:21 | `dat_len` | `graphics_data_size` |  | 5 |
| ui/anim.c:63 | `file` | `chunk_file` | the loaded ENDA.DAT chunk file | 14 |
| ui/anim.c:64 | `pos` | `chunk_offset` | offset of the next chunk | 11 |
| ui/anim.c:64 | `file_len` | `chunk_file_size` |  | 9 |
| ui/layout.c:19 | `data` | `layout_data` | graphic 696 | 12 |
| ui/layout.c:20 | `size` | `layout_size` |  | 12 |
| platform/main.c:157 | `data` | `data_dir` | game data folder; also data_override, saves (saves_dir), control (control_socket) | 27 |
| engine/dungeon.c:51 | `load_ptr` | `load_cursor` | read position in the dungeon file | 8 |

File-static variables are not reachable from ctypes: no test changes. No docs mention them (grep of docs/ for `vp`, `mw`, `mh`, `pals`: no code-name hits). 205 file-static variables in total (script: scratchpad `stat.py`); the rest have descriptive names.

## 3. Comments that only name an original symbol (R7.3)

Script: every `/* ... */` in port/src (dr_mp3.h excluded). Kind A: the comment is only one or more original names (`/* g_viewport_bitmap */`). Kind B: the comment opens with an original function or variable name followed by `:` or a short rest (`/* draw_door: ... */`), where the name is not a port function (or is in a symbol CSV).

| file | kind A | kind B |
|---|---|---|
| data/csbimage.c | 80 | 9 |
| ui/view.c | 17 | 12 |
| ui/ui.h | 14 | 2 |
| data/graphics.h | 3 | 4 |
| ui/gameloop.c | 2 | 5 |
| ui/view.h | 2 | 0 |
| engine/champion.h | 2 | 0 |
| engine/message.h | 2 | 0 |
| ui/ui.c | 1 | 0 |
| ui/ui_champion.c | 1 | 0 |
| engine/savefile.c | 1 | 0 |
| engine/savegame.c | 1 | 2 |
| ui/startend.c | 0 | 4 |
| ui/dialog.c | 0 | 2 |
| ui/gameloop.h | 0 | 2 |
| ui/layout.c | 0 | 2 |
| engine/action.c | 0 | 2 |
| engine/dungeon.c, engine/game.c, engine/magic.c, engine/savefile.h, ui/anim.c, platform/main.c | 0 | 1 each |
| total | 126 | 52 |

Kind B has 2 false positives: ui/anim.c:67 (`input_stops`, a port field) and platform/main.c:156 (`title_pending`, a port variable). data/csbimage.c kind A (80) are the labels of the KAOS/APPB address-pair copy table (`/* g_mouse_pointer_zones */` per row): they name which original variable each row copies; the plan decides if a table of addresses keeps them (they are the only way to read that table) or gets game-term labels.

Examples, kind A:
- ui/view.c:196 `static Bitmap vp; /* g_viewport_bitmap */`
- ui/view.c:63 `/* g_palette_changes_creature_d3 */`
- ui/ui.h:56 `/* g_mouse_input_interface */` (ui.h:56-67 is a block of 12)
- engine/message.h:9 `#define MESSAGE_ROW_HEIGHT 7 /* g_message_area_row_height */`
- engine/savefile.c:31 `/* g_save_header_byte_12a default */`

Examples, kind B:
- ui/view.c:251 `/* get_random_ornament_ordinal: a pseudo-random ornament ordinal ...`
- ui/view.c:987 `/* draw_floor_ornament: the floor ornament on a floor view; ...`
- ui/view.c:1632 `/* update_map_coordinates_after_relative_movement: the square forward and right ...`
- ui/gameloop.c:429 `/* game_loop: one tick of the game. ...`
- data/graphics.h:34 `/* g_object_names: graphic 0x2b6 */`

docs/kaos-symbols.csv check: of 109 distinct names in kind A comments, 106 are in docs/kaos-symbols.csv or docs/csb-appb-symbols.csv; the 3 misses are parse fragments (`_border_zone` of ui/ui.h:111 "get_slot_box_zone, _border_zone", `default`) and `g_slots_to_steal_from` (not in either CSV: add it). Of 48 distinct kind B names, 46 are in a CSV; the 2 misses are the false positives above. Sample of 10 checked by hand in docs/kaos-symbols.csv:

| name | address in kaos-symbols.csv |
|---|---|
| g_viewport_bitmap | 0x1001d0 |
| g_palette_changes_creature_d3 | 0x102fb0 |
| g_wall_ornament_coord_sets | 0x102bc4 |
| g_mouse_input_interface | 0x1031f8 |
| g_text_char_gap | 0x102964 |
| g_message_area_row_height | 0x10296c |
| g_save_header_byte_12a | 0x1040fa |
| get_random_ornament_ordinal | 0x10b4aa |
| draw_floor_ornament | 0x118b34 |
| update_map_coordinates_after_relative_movement | 0x10a85c |

Conclusion: R7.3 is almost only comment edits; one CSV addition (`g_slots_to_steal_from`, its comment is data/csbimage.c:41).

## 4. ctypes.Structure classes in tests/

| class | file:line | C struct it mirrors |
|---|---|---|
| Box | tests/test_automap.py:21 | `Box`, ui/draw.h:7 |
| Bitmap | tests/test_automap.py:25 | `Bitmap`, data/graphics.h:17 |
| TimelineEvent | tests/test_creatures.py:41 | `TimelineEvent`, engine/game.h:94 |
| Skill | tests/test_champion.py:37 | `Skill`, engine/game.h:30 |
| Champion | tests/test_champion.py:41 | `Champion`, engine/game.h:35 |
| Party | tests/test_champion.py:54 | `Party`, engine/game.h:84 (field names `thieves_eye`, `footprints`, `invisibility` differ from C) |
| Bitmap | tests/test_champion.py:426 | `Bitmap`, data/graphics.h:17 |
| CsbCopy | tests/test_csb_engine.py:39 | `CsbCopy`, data/csbimage.h:15 |
| Box | tests/test_csb_engine.py:43 | `Box`, ui/draw.h:7 |
| Spell | tests/test_csb_magic_map.py:57 | `Spell`, data/tables.h:86 |
| Bitmap | tests/test_csb_magic_map.py:62 | `Bitmap`, data/graphics.h:17 |
| Box | tests/test_csb_magic_map.py:66 | `Box`, ui/draw.h:7 |
| Box | tests/test_gameloop.py:31 | `Box`, ui/draw.h:7 |
| ActionInfo | tests/test_csb_rules.py:30 | `ActionInfo`, data/tables.h:104 |
| RecruitData | tests/test_recruit.py:23 | `RecruitData`, engine/recruit.h:10 |
| Skill | tests/test_game.py:29 | `Skill`, engine/game.h:30 |
| Champion | tests/test_game.py:33 | `Champion`, engine/game.h:35 |
| Event | tests/test_game.py:49 | `TimelineEvent`, engine/game.h:94 |
| ActiveGroup | tests/test_game.py:57 | `ActiveGroup`, engine/game.h:102 (only `group_index`, then `rest` = 14 bytes) |
| Box | tests/test_select.py:28 | `Box`, ui/draw.h:7 |
| Box | tests/test_save.py:107 | `Box`, ui/draw.h:7 |
| FtlImage | tests/test_port_data.py:22 | `FtlImage`, data/ftl.h:9 |
| Bitmap | tests/test_port_data.py:26 | `Bitmap`, data/graphics.h:17 |
| SaveFile | tests/test_savefile.py:25 | `SaveFile`, engine/savefile.h:31 |
| Bitmap | tests/test_view.py:21 | `Bitmap`, data/graphics.h:17 |
| Box | tests/test_view.py:25 | `Box`, ui/draw.h:7 |
| Skill | tests/test_ui.py:23 | `Skill`, engine/game.h:30 |
| Champion | tests/test_ui.py:27 | `Champion`, engine/game.h:35 |
| Spell | tests/test_spells.py:20 | `SpellRecord`, data/spells.h:26 (other field names than data/tables.h `Spell`: R10.2) |
| Bitmap | tests/test_text.py:23 | `Bitmap`, data/graphics.h:17 |
| MessageArea | tests/test_text.py:27 | `MessageArea`, engine/message.h:18 |

Built by the helper `S()` (tests/test_tables.py:23, `type("S", (ctypes.Structure,), ...)`), not by `class`:

| name | file:line | C struct |
|---|---|---|
| CreatureInfo | tests/test_tables.py:27 | `CreatureInfo`, data/tables.h:35 |
| CreatureSound | tests/test_tables.py:31 | `CreatureSound`, data/tables.h:55 |
| ObjectInfo | tests/test_tables.py:32 | `ObjectInfo`, data/tables.h:60 |
| WeaponInfo | tests/test_tables.py:33 | `WeaponInfo`, data/tables.h:67 |
| ArmourInfo | tests/test_tables.py:34 | `ArmourInfo`, data/tables.h:75 |
| DoorInfo | tests/test_tables.py:35 | `DoorInfo`, data/tables.h:81 |
| Spell | tests/test_tables.py:36 | `Spell`, data/tables.h:86 |
| ActionSet | tests/test_tables.py:37 | `ActionSet`, data/tables.h:95 (field `byte_6`: R7.2) |
| ActionInfo | tests/test_tables.py:38 | `ActionInfo`, data/tables.h:104 |

Duplicated mirrors: `Box` 7 copies, `Bitmap` 6, `Champion`/`Skill` 3 each, `TimelineEvent` 2, `Spell` (tables.h) 2, `ActionInfo` 2. The R7 struct renames touch only `Party` (already named differently in the test) and `ActionSet.byte_6` (tests/test_tables.py:37).

Line numbers of the C structs are the line of `typedef struct {`.
