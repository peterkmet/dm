# CSB engine compared with the DM engine

The differences between the Chaos Strikes Back (CSB) engine and the Dungeon Master v3.60 (DM) engine that matter to the port.

- DM engine: `game/KAOS.FTL`. Image `reference/decomp/dm/KAOS.bin`, base `0x100000`, data base (A5) `0x104160`. Names: [kaos-symbols.csv](../reference/kaos-symbols.csv).
- CSB engine: `game-csb/APPB.FTL`. Image `reference/decomp/csb-a/APPB.bin`, base `0x100000`, data base (A5) `0x10511e`, code base `0x105ede`. Names: [csb-appb-symbols.csv](../reference/csb-appb-symbols.csv).

All addresses are image addresses. "K" marks a DM address, "C" marks a CSB address. "UNVERIFIED" marks a claim that the code reading did not prove.

## Summary

- The CSB engine is the DM engine with a small set of changes. The game rule tables are byte-identical, except the action sets, the action experience gain, and the spells.
- The two modules come from different compilers. CSB keeps local variables on the stack, and DM keeps them in registers. A byte compare of the code does not work. The function mapping uses code-generation independent features (see [Function mapping](#function-mapping)).
- The large CSB changes are: inline English text instead of the string table in `Graphics.DAT`, the save file header, the end of the game (an `ENDA.DAT` animation in place of the DM victory screen), no Fuse and no Lord Chaos, a magic map (four new spells), and copy protection.
- The CSB adventure starts from `CSBGAME.DAT`, which the utility disk (disk B, module `CEDT.FTL`) makes. A new game on disk A starts the prison (`Dungeon.DAT`, dungeon ID 12, see [new-adventure.md](new-adventure.md)). Champion import is in `CEDT.FTL`, not in the engine.

## Function mapping

`docs/reference/csb-appb-symbols.csv` holds the CSB-to-DM function mapping.

The mapping comes from a comparison of both modules: the disassembly (`machine68k`), the function starts, and for each function its call targets, immediates, stack arguments, referenced globals, and branch count. A global alignment of the two address-ordered function lists gives the pairs (the linkers keep the source file order); call-graph passes and LVO offsets add more; a list of pairs from code readings corrects the rest. Globals pair by equal data bytes and by their position in paired functions.

### Classes

| Class | Rule | Count |
|---|---|---|
| identical | Same mapped calls (runtime helpers ignored), same set of non-zero constants, same arguments | 458 |
| constants | Same mapped calls; the constants or the arguments differ | 179 |
| logic | The mapped calls differ | 70 |
| CSB-only | No DM partner | 49 |
| DM-only | No CSB partner | 45 |

The mapping has 752 DM functions and 756 CSB functions (the module map counts 749 in its nine ranges; the rest are outside the ranges). The classes are a heuristic:

- A spot check of "identical" pairs (for example `visible_party_distance`, K`0x10ceee`, C`0x10ed9a`) found the same logic.
- Most "constants" pairs differ only in code generation: bit-field masks (DM `lsr #6; and #31`, CSB `and #$7c0; lsr #6`), sign extension (`-1` against `0xffff`), and two word pushes that DM merges into one long push.
- Many "logic" pairs differ only because DM calls `get_string` and CSB uses inline text, or because DM calls `mouse_begin_change` (an empty stub in DM, K`0x108702`) where CSB hides the pointer (C`0x109128`).

The [differences and the port](#differences-and-the-port) section lists the real differences. Three code readings checked every rule-relevant pair that the mapping did not classify as identical.

### Parts of APPB that are not the game engine

In DM, `KAOS.FTL` holds the game and `APPB.FTL` holds the intro. In CSB, `APPB.FTL` holds the game and these extra parts:

| Part | CSB functions |
|---|---|
| Title (FTL logo shrink, then fade) | `show_title` C`0x12a6e2` |
| Entrance screen and door animation | `process_entrance` C`0x12a44e`, `entrance_draw` C`0x12a320`, `entrance_open_doors` C`0x12a1cc`, `entrance_draw_dungeon_view` C`0x12a116`, `entrance_show_credits` C`0x12a672` |
| Display and video driver setup | `display_init_view`, `display_load_view`, `display_switch_view`, `video_open`, `video_close` (C`0x10b4da`–`0x10b5d8`, C`0x118a8c`–`0x118b88`) |
| Memory heap (simpler than DM) | `heap_init` C`0x117754`, `heap_setup_regions` C`0x117814`, `compute_graphics_memory` C`0x12b532` |
| Copy protection | 16 functions with subsystem `protect`, see [Copy protection](#copy-protection) |

The DM-only functions are the language support (`set_language_file_names`, `load_string_table`, `get_string`, `graphics_get_language_index`), the DM heap and graphics area manager (`heap_*`, `gfx_area_*`), Fuse and Lord Chaos (`group_fluxcage_action`, `group_fuse_action`, `group_is_fluxcage_on_square`, `group_get_lord_chaos_on_square`, `fuse_sequence`, `fuse_sequence_update`), the DM victory screen (`endgame_print_text`, `endgame_draw_button`), and `format_save_disk_dialog`.

## Data tables

The port reads the table addresses (`KAOS_*` in `src/data/include/kaos_addresses.h`) from the KAOS image. For CSB, `g_csb_copies` (`src/data/csb_image.c`) maps each address to its APPB address. The CSB addresses come from a content search with the exact table size and from the code references of the paired functions.

### Equal tables at new addresses

These tables have the same bytes in CSB. Only the address changes.

| Port name | DM | CSB |
|---|---|---|
| `KAOS_CREATURE_INFO` (27 × 26 bytes) | `0x10192e` | `0x101e32` |
| `KAOS_CREATURE_SOUNDS` | `0x101bec` | `0x1020f0` |
| `KAOS_OBJECT_INFO` | `0x101068` | `0x10156a` |
| `KAOS_WEAPON_INFO` | `0x1014a0` | `0x1019a2` |
| `KAOS_ARMOUR_INFO` | `0x1015b4` | `0x101ab6` |
| `KAOS_JUNK_WEIGHTS` | `0x10169c` | `0x101ba0` |
| `KAOS_FOOD_VALUES` | `0x1016d2` | `0x101bd6` |
| `KAOS_DOOR_INFO` | `0x1016e2` | `0x101be6` |
| `KAOS_FIXED_POSSESSIONS` | `0x101c10` | `0x102114` |
| `KAOS_NEW_GAME_EXTRA_THINGS` | `0x101058` | `0x10155a` |
| `KAOS_MESSAGE_ESCAPE_STRINGS`, `KAOS_ESCAPE_REPLACEMENT_CHARS`, `KAOS_INSCRIPTION_ESCAPE_STRINGS` | `0x1016ea`, `0x1017ea`, `0x10182a` | `0x101bee`, `0x101cee`, `0x101d2e` |
| `KAOS_SPELLS` (first 25 records) | `0x103c84` | `0x1046c2` |
| `KAOS_SYMBOL_BASE_MANA_COST`, `KAOS_SYMBOL_MANA_COST_MULTIPLIER` | `0x103c66`, `0x103c7e` | `0x1046a4`, `0x1046bc` |
| `KAOS_ACTION_DISABLED_TICKS` … `KAOS_ACTION_SKILL_INDEX` (6 tables) | `0x103eac` + 44·n | `0x1049e0` + 44·n |
| `KAOS_LIGHT_POWER_TO_LIGHT_AMOUNT`, `KAOS_PALETTE_LIGHT_THRESHOLDS` | `0x100cb4`, `0x100cd4` | `0x10118a`, `0x1011aa` |
| `KAOS_SLOT_MASKS`, `KAOS_WOUND_DEFENSE_FACTOR`, `KAOS_CHAMPION_COLORS`, `KAOS_SLOT_DROP_ORDER` | `0x100c68`, `0x100ce4`, `0x100ce0`, `0x100d0e` | `0x10113e`, `0x1011ba`, `0x1011b6`, `0x1011e4` |
| `KAOS_SOUNDS`, `KAOS_SOUND_VOLUMES` | `0x100d52`, `0x100696` | `0x10122a`, `0x1009f2` |
| `KAOS_SQUARE_TYPE_TO_EVENT_TYPE` | `0x100d4a` | `0x101222` |
| `KAOS_ORDERED_CELLS_TO_ATTACK`, `KAOS_WOUND_PROBABILITY_INDEX_TO_MASK`, `KAOS_SLOTS_TO_STEAL_FROM` | `0x100b66`, `0x100b86`, `0x100b8a` | `0x10103a`, `0x10105a`, `0x10105e` |
| `KAOS_TORCH_CHARGE_TO_ICON_OFFSET`, `KAOS_ICON_SHEET_FIRST_ICONS`, `KAOS_SLOT_BOXES` | `0x100ba0`, `0x100b92`, `0x100bb0` | `0x101074`, `0x101066`, `0x101084` |
| `KAOS_DIRECTION_TO_STEP_EAST`, `KAOS_DIRECTION_TO_STEP_NORTH`, `KAOS_GROUP_DIRECTIONS_PACKED` | `0x101038`, `0x101040`, `0x10192a` | `0x10153a`, `0x101542`, `0x101e2e` |
| `KAOS_MOVEMENT_ARROW_TO_STEP_FORWARD`, `..._RIGHT` | `0x103c56`, `0x103c5e` | `0x104694`, `0x10469c` |
| `KAOS_KEYS_INTERFACE`, `KAOS_KEYS_MOVEMENT`, `KAOS_KEYS_SLEEPING`, `KAOS_KEYS_FROZEN` | `0x103948`, `0x103964`, `0x103980`, `0x10398c` | `0x1043c4`, `0x1043e0`, `0x1043fc`, `0x104408` |
| Name entry: `KAOS_KEYS_NAME_ENTRY`, `KAOS_RENAME_CURSOR`, `KAOS_NAME_ENTRY_CHARACTERS`, `KAOS_NAME_UNDERSCORES`, `KAOS_TITLE_UNDERSCORES` | `0x103994`, `0x100cea`, `0x100cee`, `0x1024ba`, `0x1024c2` | `0x104410`, `0x1011c0`, `0x1011c4`, `0x102dd0`, `0x102dd8` (`champion_rename` C`0x116312`: A5-`0xd0e`, A5-`0x3f5e`, command + A5-`0x4002`, A5-`0x234e`, A5-`0x2346`) |
| `KAOS_ARROW_POINTER_BITMAP`, `KAOS_HAND_POINTER_BITMAP`, `KAOS_MOUSE_POINTER_ZONES`, `KAOS_MOUSE_POINTER_HOTSPOTS`, `KAOS_POINTER_SHADOW_COLORS` | `0x1002b0` … `0x1005fe` | `0x1005f0`, `0x100714`, `0x10084a`, `0x10091a`, `0x100984` |
| `KAOS_DUNGEON_PALETTES` | `0x100aa6` | `0x100f78` |
| `KAOS_LAYOUT_BITMAPS` | `0x1025f2` | `0x102f50` |
| `KAOS_PALETTE_CHANGES_INVISIBILITY`, `KAOS_PALETTE_CHANGES_ACTION_ICON` | `0x10247a`, `0x103ffa` | `0x102d8a` (`champion_build_icon_bitmap` pushes it at C`0x1150b6`, as DM does), `0x104b52` |
| View tables: `KAOS_WALL_ORNAMENT_DEPTHS` … `KAOS_PLAQUE_HEIGHTS`, `KAOS_OBJECT_ASPECTS`, `KAOS_PROJECTILE_ASPECTS`, `KAOS_PALETTE_CHANGES_*`, `KAOS_CREATURE_ASPECTS`, `KAOS_CREATURE_REPLACEMENT_COLOR_SETS`, `KAOS_CREATURE_SHIFT_SETS` | `0x102ba6` … `0x102fd0` | `0x10373a` … `0x103b60` |

Small scalar values (`KAOS_VIEWPORT_X`, `KAOS_PORTRAIT_WIDTH`, `KAOS_WALL_TEXT_GLYPH_WIDTH`, `KAOS_DAMAGE_SPLAT_*`, `KAOS_BAR_SHADOW_OFFSET`, `KAOS_ENDGAME_BUTTON_*`) are equal. Their CSB addresses come from the code references, because a content search gives many hits. Example: `KAOS_PORTRAIT_WIDTH` is C`0x1032bc`, `KAOS_VIEWPORT_WIDTH` is C`0x1032a6`, `KAOS_WALL_TEXT_GLYPH_WIDTH` is C`0x1032d2`.

### Tables with a different layout

DM pads each byte array to an even length. CSB does not. The content is equal, but the stride between tables changes:

| Table | DM | CSB | Change |
|---|---|---|---|
| View square tables `KAOS_VIEW_SQUARE_LANE` … `KAOS_VIEW_SQUARE_FIELD_ASPECT` (23 used bytes) | `0x102a7e` + 24·n | `0x103616` + 23·n | No pad byte |
| `KAOS_FIELD_ASPECTS` (12 × 8) | `0x102b26` | `0x1036b7` (odd address) | Address only |
| `KAOS_OBJECT_SCALES`, `KAOS_PROJECTILE_SCALES`, `KAOS_EXPLOSION_SCALES`, `KAOS_PILE_SHIFTS` | `0x102e24`, `0x102e2a`, `0x102e32`, `0x102e38` | `0x1039b6`, `0x1039bb`, `0x1039c2`, `0x1039c7` | No pad bytes |
| `KAOS_REBIRTH_STEP_2_SCALES` (2 × 7) | `0x102fe8`, `0x102ff0` | `0x103b78`, `0x103b7f` | No pad byte |
| `KAOS_WALL_SET_LAYOUT_IDS` (46 words) | `0x102a22`, one array | `0x10343c` (31 words) and `0x1034dc` (15 words) | Split into two arrays, equal values |
| `KAOS_ACTION_SETS` (44 records) | `0x103d4c`, 8 bytes each | `0x1047ac`, 6 bytes each | CSB has no pad bytes: 3 actions, 2 minimum skill levels, 1 flag byte. The index code is `uVar3 * 6 + A5 - 0x972` (C`menus_set_action_list` caller) |

### Tables with different values

| Table | DM | CSB | Difference |
|---|---|---|---|
| `KAOS_ACTION_EXPERIENCE_GAIN` | `0x103fb4` | `0x104ae8` (read at C`0x126704`) | BLOW HORN (4): 1 → 0. HEAL (36): 5 → 0. CALM (37): 1 → 0. BRANDISH (41): 3 → 0. |
| `SPELLS` | 25 records | 29 records at C`0x1046c2` | 4 new spells of kind 4, see [Magic map](#magic-map) |
| `KAOS_ENTRANCE_PALETTE` | `0x100a66` | `0x100f38` | Color 5: `0x0c0` → `0x422` |
| `KAOS_CREDITS_PALETTE` | `0x100a46` | `0x100f18` | All colors differ (CSB credits picture) |
| `KAOS_KEYS_ENTRANCE` | `0x10393c`, 2 entries | `0x1043bc`, 1 entry | CSB has only command `0xc8` |
| `KAOS_DIALOG_VERSION_TEXT` | `0x10402c` "V3.6" | `0x104c24` "V3.1" | Text |
| `KAOS_DIALOG_MOUSE_INPUTS` | `0x103bd4` | `0x104650` (read in `dialog_get_choice` at C`0x127b06`) | Different pointer values. The 8 tables they point to (C`0x104500`, 336 bytes) have the same bytes as the DM tables (K`0x103a84`). |
| String index tables `KAOS_SKILL_LEVEL_STRING_INDICES`, `KAOS_BASE_SKILL_STRING_INDICES`, `KAOS_STATISTIC_STRING_INDICES`, `KAOS_DIRECTION_STRING_INDICES` | indices into graphic 700 | pointer tables | See [Text](#text) |

### Graphics.DAT

CSB `Graphics.DAT` has 728 items. DM has 749. The item numbers up to 727 have the same meaning, so the graphic ids in `src/data/include/ids.h` stay valid. [Chaos Strikes Back data](data.md#item-comparison) lists the items that differ.

## Differences and the port

### Engine selection and table addresses

Port:

- `kaos_load` (`src/data/kaos.c`) loads `APPB.FTL` when `g_adventure` is CSB. `csb_image_build` (`src/data/csb_image.c`) then makes a KAOS-layout image, with each table that the port reads at its DM address. The `KAOS_*` names of `src/data/include/kaos_addresses.h` do not change.
- `g_csb_copies` in `src/data/csb_image.c` is the single address table: KAOS address (its `KAOS_*` name), APPB address, size. The APPB addresses of the converted tables have the prefix `CSB_APPB_`. Strided and split tables have one row per part.
- Conversions in code: the action sets (6-byte CSB records to the 8-byte DM layout), and the dialog pointers (moved to the DM tables).
- The 29 CSB spells are at `0x104160`, after the KAOS data. `spells.c` and `tables.c` read 29 spells from there for CSB.
- `adventure_get(g_adventure)->engine` gives the module file name: `KAOS.FTL` or `APPB.FTL`.

Addresses that the tables above do not list. Each is verified by a code reference (the CSB function that reads it) or by the CSV:

| Port name | DM | CSB | Reference |
|---|---|---|---|
| `KAOS_INTERFACE_PALETTE`, `KAOS_POINTER_PALETTE` (interface, then pointer palette) | `0x100170`, `0x100190` | `0x102f0a`, `0x102f2a` | `g_initial_colormap` and + 32 (`display_load_view`, `input_event_handler`) |
| `KAOS_ARROW_POINTER_WIDTH`, `KAOS_HAND_POINTER_WIDTH` (the 4 size bytes, then the bitmap) | `0x1002ac`, `0x1003d0` | `0x1005ec`, `0x100710` | `init_mouse_pointer` C`0x10898c`, `0x10899a` |
| `KAOS_CREDITS_PALETTE`, `KAOS_ENTRANCE_PALETTE` (CSB values) | `0x100a46`, `0x100a66` | `0x100f18`, `0x100f38` | `entrance_show_credits`, `entrance_draw` |
| `KAOS_SLOTS_TO_STEAL_FROM` | `0x100b8a` | `0x10105e` | `steal_from_champion` C`0x10e7f6` |
| `KAOS_LIST_SEPARATOR` ", " | `0x101ebe` | `0x10264a` | `build_object_attributes_string` C`0x110dcc` |
| `KAOS_BONES_SEPARATOR` " ", `KAOS_ATTRIBUTES_SUFFIX` ")", `KAOS_ATTRIBUTES_PREFIX` "(" | `0x101ec6`, `0x101ec8`, `0x101eca` | `0x10267a`, `0x1026ec`, `0x1026ea` | `draw_panel_object` C`0x111cba`, `0x111f12`, `0x111f16` |
| `KAOS_VIEWPORT_X` | `0x10293a` | `0x103298` | `command_init_mouse_input_table` |
| `KAOS_DIALOG_MESSAGE_LINE_PAD` | `0x102966` | `0x1032ca` | `dialog_draw` C`0x128176` |
| `KAOS_DAMAGE_SPLAT_LARGE_WIDTH`, `KAOS_DAMAGE_SPLAT_SMALL_WIDTH` | `0x102978`, `0x10297c` | `0x1032dc`, `0x1032e0` | `menus_draw_action_damage` |
| `KAOS_BAR_SHADOW_OFFSET` | `0x102980` | `0x1032e4` | `draw_panel_horizontal_bar` C`0x11203e` |
| `KAOS_WALL_ORNAMENT_DEPTHS` (14), `KAOS_FLOOR_ORNAMENT_VIEW_OFFSETS` (11), `KAOS_ALCOVE_ORNAMENT_INDICES` (3), `KAOS_FOUNTAIN_ORNAMENT` (1) | `0x102ba6`, `0x102bb4`, `0x102bc0`, `0x102bc3` | `0x10373a`, `0x103748`, `0x103753`, `0x103757` | `draw_wall_ornament`, `draw_floor_ornament`, `load_current_map_graphics`, CSV |
| `KAOS_FLOOR_ORNAMENT_COORD_SETS` (9), `KAOS_DOOR_ORNAMENT_COORD_SETS` (12), `KAOS_DOOR_BUTTON_COORD_SETS` (1) | `0x102c00`, `0x102c0a`, `0x102c16` | `0x103794`, `0x10379d`, `0x1037a9` | `load_current_map_graphics`, `draw_door_button` |
| `KAOS_PLAQUE_HEIGHTS` | `0x102c58` | `0x1037ea` | `draw_wall_ornament` C`0x11b1ee` |
| Entrance mouse input (4 entries, no 0xd8) | `0x10318c` | `0x103c24` | `process_entrance` C`0x12a550` |
| Endgame mouse input (RESTART only) | `0x1031d4` | `0x103c60` | `endgame` C`0x1284c2` |
| Other mouse input tables | `0x1031f8` … `0x10378c` | `0x103c78` … `0x10420c` | CSV names |
| `KAOS_KEYS_ENTRANCE` (1 entry), `KAOS_KEYS_INTERFACE`, `KAOS_KEYS_MOVEMENT` | `0x10393c`, `0x103948`, `0x103964` | `0x1043bc`, `0x1043c4`, `0x1043e0` | `process_entrance`, `start_game` |
| `KAOS_ACTION_EXPERIENCE_GAIN`, `KAOS_DIALOG_VERSION_TEXT`, `KAOS_DIALOG_MESSAGE_COLORS` | `0x103fb4`, `0x10402c`, `0x104026` | `0x104ae8`, `0x104c24`, `0x104c20` | `menus_is_action_performed`, `dialog_draw` |

Not in the CSB image:

- `KAOS_ENDGAME_BUTTON_BORDER`, `KAOS_ENDGAME_BUTTON_INNER_BORDER` (K`0x102988`, `0x10298c`): only DM `endgame_draw_button` reads them. CSB has no such function. `startend.c` draws RESTART THIS GAME in the CSB boxes (`csb_draw_restart_button`).
- The string index tables `KAOS_SKILL_LEVEL_STRING_INDICES`, `KAOS_BASE_SKILL_STRING_INDICES`, `KAOS_STATISTIC_STRING_INDICES`, `KAOS_DIRECTION_STRING_INDICES`: CSB has pointer tables. `csb_text_build` fills them with the text indices of the built strings (see [Text](#text)).

### Text

DM 3.60 takes all text from `Graphics.DAT` item 700 (`get_string`), action names from item 699, and object names from item 694. CSB has inline English text in the APPB data section. The texts are the same except those under "Text that differs".

| DM string (port id) | CSB source |
|---|---|
| 0–3 class names (`KAOS_BASE_SKILL_STRING_INDICES`) | Pointer table C`0x102d56`[4] in skill order: FIGHTER, NINJA, PRIEST, WIZARD |
| 4–18 skill levels (`KAOS_SKILL_LEVEL_STRING_INDICES`) | Pointer table C`0x102524`[level - 2] |
| 19 WAKE UP, 20 GAME FROZEN | C`0x104680`, C`0x104688` |
| 21 PARTY FACING, 22–25 directions | C`0x1026ae`, pointer table C`0x102578`[4] |
| 26–29 CONSUMABLE … CURSED, 30 AND, 31 BURNT OUT | C`0x10267c`, `0x102688`, `0x102692`, `0x10269a`, C`0x10264e`, C`0x1026a2` |
| 32–35 waterskin states | C`0x1026bc`, `0x1026c4`, `0x1026d4`, `0x1026e2` |
| 36 LOAD, 37 WEIGHS, 38 ".", 39 " KG.", 40 " KG" | C`0x102dc2`, `0x1026ee`, `0x1026f6`, `0x1026f8`, `0x102dcc` |
| 41–47 statistics (`KAOS_STATISTIC_STRING_INDICES`) | Pointer table C`0x102588`[7] |
| 48–50 HEALTH, STAMINA, MANA | C`0x102740`, `0x102748`, `0x102750` |
| 51–58 spell failure | Champion name + C`0x104b7a` + class name + C`0x104b9a`; C`0x104ba2`; C`0x104bc0` |
| 61–68 level gained | Champion name + C`0x102d3e` " JUST GAINED A " + class name + C`0x102d4e` " LEVEL!" |
| 69, 70 coin flip | C`0x104b2a`, C`0x104b3e` |
| 71 CAN'T REACH, 72 NEED AMMO | C`0x104b64`, C`0x104b70` |
| 73–76 resurrected, reincarnated | Champion name + C`0x102dec` or C`0x102dfa` |
| 77 RESTART THIS GAME | C`0x104c56` |
| 78, 80, 81, 86, 87, 88, 101, 105, 106 load and save messages | Pointer table C`0x104d08`… (texts C`0x104d70`–`0x104fa8`) |
| Action names (item 699) | NUL-separated list from C`0x1048b4`; index 0 = "N". Equal to DM item 699 |

Text that differs:

- Disk messages 94, 95, 96 say "CHAOS STRIKES BACK" in place of "DUNGEON MASTER" (C`0x104d8c`, `0x104e14`, `0x104ed4`).
- The save dialog has "SAVE AND PLAY" (C`0x104f88`) and "SAVE AND QUIT" (C`0x104f96`), not SAVE and QUIT. Text 93 (quit without save) does not exist.
- Texts 99, 100 ("A:", "B:") and 135–139 (memory errors) do not exist.
- Creature names (pointer table C`0x102164`[27], DM texts 108–134): 12 names differ, for example 109 SLIME DEVIL (DM SWAMP SLIME), 111 GAZER (WIZARD EYE), 127 ZYTAZ (MATERIALIZER).
- " NEEDS A MAGIC MAP IN ACTION HAND FOR THIS SPELL." (C`0x104bea`) and "NO CREATURE" (C`0x102654`) are not new. DM item 700 has them as texts 60 and 107, and the creature names as texts 108–134. The port uses these texts (`TEXT_NO_CREATURE` in `panel.c`, `SPELL_FAILURE_NEEDS_MAGIC_MAP` in `magic.c`).
- The CSB direction table C`0x102578` is in direction order (NORTH, EAST, SOUTH, WEST). DM texts 22–25 are NORTH, SOUTH, EAST, WEST.

Code references (each address verified: a `pea` or `lea` with A5 = C`0x10511e` in the function, found in the APPB code bytes):

| CSB address | Function |
|---|---|
| `0x104680`, `0x104688` | `command_draw_sleep_screen`, `command_process_queue` |
| `0x10264e` | `build_object_attributes_string` |
| `0x10267c`–`0x10269a`, `0x1026a2`, `0x1026ae`, `0x1026bc`–`0x1026e2`, `0x1026ee`–`0x1026f8`, `0x102578` | `draw_panel_object` |
| `0x102dc2`, `0x102dcc` | `champion_draw_state` |
| `0x102740`, `0x102748`, `0x102750` | `inventory_toggle` |
| `0x102524`, `0x102588`, `0x102d56` | `draw_champion_skills_and_statistics` (`0x102d56` also in `champion_add_skill_experience` and `menus_print_spell_failure_message`) |
| `0x104b7a`, `0x104b9a`, `0x104ba2`, `0x104bc0`, `0x104bea` | `menus_print_spell_failure_message` |
| `0x102d3e`, `0x102d4e` | `champion_add_skill_experience` |
| `0x104b2a`, `0x104b3e` | `menus_is_action_performed` |
| `0x104b64`, `0x104b70` | `menus_draw_action_damage` |
| `0x102dec`, `0x102dfa` | `champion_click_rebirth_panel` |
| `0x104c56` | `endgame` |
| `0x102654`, `0x102164` | `scroll_text_insert_creature_name` |
| `0x1048b4` | `menus_get_action_name` |
| `0x104d08` + 4·n | Read with `move.l` (no `lea`), for example `reference/decomp/csb-a/APPB.c` line 38716 (entry 0) and line 37116 (save dialog: entries 24, 21, 22) |

The pointer table C`0x104d08` has 26 entries; the DM text index of each entry is in `src/data/csb_text.c` (`CSB_APPB_DISK_MESSAGES`).

Port:

- `src/data/csb_text.c` `csb_text_build` (called by `csb_image_build`): builds DM texts 0–134 as one block of NUL-terminated strings (item 700 order) and the 44 action names (item 699). The spell failure and level gained texts join prefix, class name, and suffix. Each string index table entry k of the KAOS image gets text index first + k, so the class names (0–3) and directions (22–25) are in the CSB table order: text 2 is PRIEST, 3 is WIZARD, 23 is EAST, 24 is SOUTH. Only the index tables read these texts.
- `src/data/graphics.c` `load_strings`: for CSB, takes items 699 and 700 from `csb_text_get_item`, and ends each object name (item 694) at the byte with bit 7 set.
- `src/engine/champion.c` reads its messages with `gfx_get_string`. The other files that show text do not change.

### Actions

`menus_is_action_performed` (K`0x122892`, C`0x12663e`, switch at C`0x126764`, cases 2–42, default C`0x126caa`):

- INVOKE (27), FLUXCAGE (35), and FUSE (43) go to the default branch in CSB. They only apply the common part: disabled ticks, stamina, experience, and the state redraw. The function returns 1. DM uses `random16` for INVOKE and calls `group_fluxcage_action` and `group_fuse_action`.
- Action set 1 = {27, 43, 35} still exists in CSB (C`0x1047ac`).
- All other actions are the same as DM, including FIREBALL, DISPELL, LIGHTNING, SPIT, door bash, coin flip, shields, HEAL, LIGHT, WINDOW, FREEZE LIFE, THROW, SHOOT, and CLIMB DOWN.
- `menus_is_group_frightened_by_action`, `menus_decrement_charges`, and `menus_get_action_hand_charge_count` are the same as DM.

Port: `src/engine/action.c` `action_is_action_performed` (INVOKE, FLUXCAGE, FUSE do nothing in CSB), `src/data/tables.c` (experience gain table).

### Magic map

CSB has a magic map. DM has none.

New spells (records 25–28 of `g_spells`, C`0x1046c2`, kind 4, skill 12):

| Spell | Runes | Type | Timeline event | Champion byte |
|---|---|---|---|---|
| 25 | OH GOR DAIN | 0 | 80 (`0x50`) | `0x118` |
| 26 | OH GOR ROS | 1 | 81 | `0x119` |
| 27 | OH GOR KU | 2 | 82 | `0x11a` |
| 28 | ZO IR NETA | 3 | 83 | `0x11b` |

Cast (`menus_get_champion_spell_cast_result`, C`0x126e0e`, case kind 4 at C`0x12705c`):

1. `is_magic_map_in_hand` (C`0x111080`) must be true for the object in the action hand (champion slot word `0xd6`). Otherwise the spell fails with message 11 and the function returns 3.
2. `D6 = (power + 1) × 4`, power = power symbol - `0x5f`.
3. Type 0: byte `0x118` += 1, duration 2·D6². Type 1: byte `0x119` += 1, D6 -= 2, duration 2·D6². Type 2: byte `0x11a` += 1, D6 -= (D6 >> 2) - 1, duration 2·D6².
4. Type 3 (C`0x1270de`): delete the first event 83 of this champion. Byte `0x11b` = party direction + 1. Word `0x11c` = x << 11 | y << 6 | map. Action disabled ticks = D6² >> 1, event duration = 4 × that value.
5. C`0x1272d4`: the event time is game time + duration, on the party map. `timeline_add_event`. The event priority byte holds the champion index.

Checks of the cast (APPB disassembly):

- The switch on the spell type is `rt_switch_range` 0–3 after C`0x127094`: type 0 C`0x1270a6`, 1 C`0x1270bc`, 2 C`0x1270ca`, 3 C`0x1270de`.
- D6 is a 16-bit word. Types 0–2: duration = 2·D6² (`muls`, `add`). The action disabled ticks of types 0–2 come from the spell record, as for the other spells.
- Type 3 looks for the first event 83 only when byte `0x11b` is not 0. The search runs over the slots 0 to `g_timeline.high_water` - 1 (C`0x103bd2`) and compares the type byte and the priority byte (champion index). Disabled ticks = (D6² as an unsigned word) >> 1.
- The failure (C`0x12706a`) prints message 11 and returns 3 before the experience and the action disable. C`0x127a1a` prints the champion name, then the text of the message. The CSB prefix text 59 is empty, so the port order (prefix, name, suffix) gives the same line.

Timeline (`timeline_process`, C`0x12411e`): events 80, 81, 82 decrement champion bytes `0x118`, `0x119`, `0x11a` (C`0x124486`, `0x12449c`, `0x1244b2`). Event 83 clears byte `0x11b` (C`0x1244c8`).

Magic map object: a scroll whose decoded text equals "MAGICMAP" (pointer C`0x102564` → C`0x102640`). `is_magic_map_in_hand` (C`0x111080`) decodes the whole text (mode `0x8002`: scroll, any visibility) and compares it with `str_compare`.

The CSB dungeon (a search of every scroll, also in chests and group possessions, with the port loading `disk-b/MINI.DAT`):

| Text | Map | x | y | Thing |
|---|---|---|---|---|
| MAGICMAP | 4 | 20 | 11 | `0x1c05` |
| CREANAME | 4 | 12 | 13 | `0x1c06` |
| MAP SPELL / OH GOR DAIN | 5 | 14 | 1 | `0x5c08` |
| MAP SPELL / OH GOR ROS | 5 | 7 | 10 | `0x5c09` |
| MAP SPELL / ZO IR NETA | 6 | 21 | 19 | `0x5c07` |
| MAP SPELL / OH GOR KU | 8 | 1 | 24 | `0x1c03` |

The party starts on map 4. The prison (`Dungeon.DAT`, dungeon ID 12) has no scrolls.

Scroll panel (`draw_panel_scroll`, C`0x111764`):

- `scroll_text_insert_creature_name` (C`0x1116a0`) replaces each "CREANAME" (C`0x102636`) in the scroll text with the name of the creature group in front of the party, or with "NO CREATURE". The name is the text of `group_get_thing` (C`0x10d7f0`) on the square one step ahead in the party direction: "NO CREATURE" (C`0x102654`) for no group, otherwise the creature name pointer of the group creature type (table C`0x102164`, port texts 108 + type). The first match sets the drawn panel content to 9.
- When the first line (after the replacement) is "MAGICMAP": drawn panel content 10, graphic `0x18` in zone `0x65` with transparent color 8. Rune characters `0x72`–`0x75` (KU, ROS, DAIN, NETA) print in zones `0x267`–`0x26a` with `text_print_to_viewport_zone_color`: color 8 when the byte (`0x11a`, `0x119`, `0x118`, `0x11b`) is not 0, otherwise color 0, background 15.
- A 7 × 7 grid. `get_zone_size(0x266)` (C`0x119616`) gives the top left point of the first square. The first row is 3 squares ahead of the party, the first column 3 squares to the left (`update_map_coordinates_after_relative_movement(dir, 3, -3)`). Columns step to the right (direction + 1), rows step back (direction + 2). With byte `0x11b` set, the grid uses the stored map (`set_current_map`), position, and direction (byte - 1), and `set_current_map(party map)` follows.
- The party square is drawn last, when the grid map is the party map. `magic_map_draw_square` (C`0x111140`, args x, y, direction, pixel x, pixel y) draws each square from tiles of graphic 12 (`magic_map_draw_cell`, C`0x1110e0`: tile & 15 and tile >> 4 select the 7 × 7 source, transparent color 10).
- Tile size and gap (C`0x1032e6`–`0x1032ec`): width 7, height 7, gap x 1, gap y 1 in the APPB data. The code only reads them. Graphic 12 is 112 × 42 (16 × 6 tiles).

`magic_map_draw_square` (C`0x111140`), verified in the disassembly (the decompiler stops at the `rt_switch_word` table C`0x111218`):

1. `set_square_aspect` of the square in the map direction. The aspect words are element, square byte, first thing, then 4 words (wall sides, or door state, door thing index, floor ornament ordinal with the footprints bit `0x8000`).
2. Border fill (`fill_viewport_box`, 1 pixel around the tile): color 8 when x and y are the party x and y (also on a stored map), otherwise color 4 when DAIN (`0x118`) is set, the element is not a wall, and the footprints bit is set.
3. Blink flag: square type 6 (fake wall) with bit 0 (imaginary).
4. The element tile (table below).
5. Things show only with DAIN or KU: the thing list from the first thing. A group with KU becomes the group of the square. With DAIN: the first projectile, the first explosion, and each Zokathra spell (junk of object type 197).

| Element or thing | Tile |
|---|---|
| Wall | 0; 1 when ROS, the square blinks, and game time bit 1 is set. For each side k with an ornament: `0x16`+k alcove, `0x1a`+k Vi altar (C`0x10220c`), `0x1e`+k fountain. |
| Corridor | 2, then `0x4e` + the global floor ornament index (C`0x1021e4`) of the floor ornament. |
| Teleporter | 6 + `random_2bits`. |
| Door (side 16, front 17) | `0xb` or `0xa`; +2 without a door button (door byte 3 bit 6); +4 when open or destroyed. |
| Stairs (18, 19) | `0x12` + ((exit direction - direction) & 3); +2 for stairs up. |
| Pit | 3; 4 when invisible; 5 when invisible, ROS, and game time bit 0. |
| Zokathra spell (DAIN) | `0x22` + ((cell - direction) & 3); +4 when the element is a wall and the square does not blink. |
| Group with creature attributes bit `0x4000` clear (KU) | `0x4c` + game time bit 0. |
| Projectile (DAIN), by the aspect of its slot | -11: `0x32`; -12: `0x36`; -13: `0x3a`; -14: `0x3e`; -4: `0x2a`, +4 when bit 0 of the projectile event direction (event byte 9 bits 4–5) differs from bit 0 of the map direction; aspect ≥ 0 with object type 197: `0x22`; other negative aspects: none. Plus (cell - direction) & 3. |
| Explosion (DAIN), by type | 0, 2: `0x42`; 6, 7: `0x44` + `random_2bits`; 50: `0x48` + `random_2bits`; other types below 40: `0x43`; 40, 100, 101: none. |

The port draws the same tiles in `view_draw_magic_map` (`src/ui/view.c`).

The footprints bit (`set_square_aspect` C`0x10d630`–`0x10d654`): for a corridor, an open fake wall, a closed pit, an open imaginary pit, a door, and a teleporter, `champion_get_scent_ordinal` - 1 is in [`first_scent_index`, `last_scent_index`). Walls, closed fake walls, stairs, and other open pits have no footprints.

`set_current_map` (C`0x10d666`) changes the map, its size, and its door types. The ornament lists, the alcove, Vi altar, and fountain indices, and the inscription index come from `load_current_map_graphics` of the party map. Thus a stored map of another level shows its squares with the ornament data of the party map. The port does the same.

Redraw: in `game_loop` (C`0x10809c`), with the inventory open and the drawn panel content 9 or 10, CSB sets the panel attribute of the inventory champion and calls `draw_champion_state` each tick. `draw_champion_state` (C`0x115a04`) draws `draw_panel_object(leader hand object, 1)` while the eye is pressed and the drawn panel content is 9 or 10. Thus the map and the creature name follow the game.

`draw_panel_scroll` writes the drawn panel content `g_panel_content_drawn` (C`0x1024cc`: 5, 9, or 10), not `g_panel_content` (C`0x1024ca`).

Port: `magic.c` `cast_magic_map` (spell kind 4, failure message 11), `spells.c` (29 spells), `event.c` (events 80–83), `game.h` `Champion` (bytes `0x118`–`0x11d`, also in the save block), `panel.c` (`insert_creature_name`, `show_magic_map`, `panel_redraw_scroll`), `view.c` `view_draw_magic_map`, `gameloop.c` (the redraw each tick). Spells 25–28 exist only in CSB, and the scroll panel checks `g_adventure`.

### Sensors and timeline

- `sensor_add_event` (DM K`0x11e526`, CSB C`0x121ba8`): CSB writes priority 0 for each sensor event (C`0x121bc4`). DM sets the priority from the effect: set 1, clear 3, toggle 2, hold 0 (K`0x11e546`–`0x11e564`). The event comparison (C`0x122960`) still uses the priority. Thus sensor events of the same type on the same tick run in slot order in CSB. Port: `src/engine/sensor.c` `sensor_add_event`.
- Floor sensor type 9 (version checker): CSB triggers when the data is ≤ 31 (C`0x122766`). DM uses ≤ 36 (K`0x11f052`). Port: `src/engine/sensor.c` `VERSION_CHECKER_LIMIT_CSB` 32.
- Wall sensor type 18 (end game): see [End of the game](#end-of-the-game).
- No new sensor types, actuator types, or creature behavior. The other sensor, projectile, explosion, and creature functions differ only in code generation.
- `timeline_process` handles the same events as DM, plus 22 (copy protection), 80–83 (magic map).

### Creatures

- `group_init_active_groups` (C`0x10e990`): a new game has 60 active group slots (C`0x10e998`). DM has 110 (K`0x10cb80`). CSB allocates exactly the stored count. DM allocates at least 110. CSB `load_game` (C`0x129efc`) restores the saved count without a minimum. The count limits the creature generators (`group_get_generated`, C`0x10de76`). Port: `ACTIVE_GROUP_MAX_COUNT_CSB` (60) in `src/engine/include/group.h`; `src/engine/savegame.c` applies `ACTIVE_GROUP_MAX_COUNT` (110) as the minimum for DM only.
- `steal_from_champion` (C`0x10e7e2`–`0x10e7f2`): a new rule takes the action hand when the panel content is 10 and `random_bit` is set. No code writes 10 to the panel content variable C`0x1024ca`, so the rule never runs. No port change. APPB check: the code writes C`0x1024ca` only at C`0x112120` (7), C`0x112180` (6), C`0x112188` (5), C`0x112190` (1), and C`0x116324` (8). The magic map writes 10 to the drawn panel content C`0x1024cc`, a different variable. The rule stays unreachable.

### Champions

- `champion_click_rebirth_panel` (C`0x11668e`), REINCARNATE: before the 12 random points, CSB sets each statistic 1–6 (current and maximum) to max(minimum, value - value/8) (C`0x1167e0`–`0x116854`). Then it divides health, stamina, and mana (current and maximum) by 4 (C`0x116856`–`0x11688e`). Port: `src/engine/champion.c` `reincarnate`. The CSB dungeon has no mirrors, so this code does not run in a normal CSB game (UNVERIFIED).
- All other champion functions differ only in code generation: object modifiers, skill level, skill experience, luck, time effects, candidate, food and potions, object icons.

### Save and load

File name strings in the binary: `DF0:CSBGame.dat` and `DF0:CSBGame.bak` (C`0x10013a`, `0x10014a`), on the volume `DungeonSave:` (C`0x10012c`). DM uses `DF0:DMGAME~.DAT` and `.BAK`.

The cipher (`savefile_xor_block`, C`0x128720`), the checksum (C`0x128752`), the checked blocks (C`0x1287c2`, `0x128786`), and the summed blocks (C`0x128810`, `0x128856`) are the same as DM. The blocks are the same: globals (128 bytes, same layout), active groups, champions and party (`0x580` bytes), timeline events, timeline index, 4 portraits, dungeon.

The header differs (`save_read_header` C`0x1289c4`, `load_game` C`0x129b3e`); the offsets are in [save.md](../formats/save.md#csb-header).

`save_game` (C`0x128cc0`) writes the fields at the CSB offsets: dialog choice `0x132`, extra header `0x17c` (`0x84` bytes), game ID `0x134`, platform `0x12d` = 5, header byte `0x12c`, save format `0x178`, dungeon ID `0x17a`, block keys `0x138`, block checksums `0x158`. Then it calls `save_write_header` (C`0x128a42`). A prison save has dungeon ID 12; a save of the adventure has 13 (see [new-adventure.md](new-adventure.md#summary)).

Other load and save differences:

- `load_game` opens `CSBGAME.DAT`, then `CSBGAME.BAK`. Then it tries `DungeonMaster:Dungeon.FTL` (C`0x129d74`), which is not on the disk. A new game loads `Dungeon.DAT` of disk A: the prison (2622 bytes, compressed, signature `0x8104`).
- The save dialog has CANCEL, SAVE AND PLAY, SAVE AND QUIT, and FORMAT FLOPPY (`save_game` C`0x128cc0`, `reference/decomp/csb-a/APPB.c` from line 36964). Without the floppy option (`A5-0x4b6` = 0), the dialog has 3 choices: 1 CANCEL, 2 SAVE AND PLAY, 3 SAVE AND QUIT. With the option, it has 4: 1 SAVE AND PLAY, 2 SAVE AND QUIT, 3 FORMAT FLOPPY, 4 CANCEL.
  - CANCEL closes the dialog.
  - SAVE AND PLAY and SAVE AND QUIT write the file. The header field dialog choice (`0x132`) is 1 for SAVE AND PLAY and 0 for SAVE AND QUIT.
  - After a successful SAVE AND PLAY, `A5-0xe0` becomes 1 and the game continues. The dialog does not stay open (the DM port keeps the DM dialog open after SAVE).
  - After a successful SAVE AND QUIT, `A5-0xe0` becomes 0 and `endgame(0)` (C`0x128374`) runs: the credits, then `quit_game`.
  - When the save fails, a message dialog with one button shows (C`0x1294f8`), then the game continues.
  - No code path quits without a save, and the save dialog has no plain QUIT.
  - Port (`src/ui/startend.c` `save_game_csb`): the 3-choice dialog. The port writes dialog choice 1 for both saves. SAVE AND QUIT shows the credits and then the CSB screen (turn-based: the entrance picture).
- `load_dungeon` (C`0x1295a0`) reads the same format. Its extra calls are copy protection.

Port changes: `src/engine/savefile.c` (header offsets, file names), `src/engine/savegame.c` (active group count, champion bytes `0x118`–`0x11d`), `src/ui/startend.c` (save dialog).

Port start of a game: NEW ADVENTURE starts the prison, and a save in the prison creates the adventure ([manual](../manual.md#chaos-strikes-back)).

### Entrance and start

CSB has an entrance screen with doors (`process_entrance`, C`0x12a44e`). It differs from DM:

- Graphics 4 and 5 (CSB pictures), work bitmaps from graphics 2, 3, and 11.
- Mouse input C`0x103c24`, keyboard input C`0x1043bc` (one key).
- The choice is in `g_entrance_choice` (C`0x10017a`): two buttons (zones `0x1b3`, `0x1b4`). Value `0xcb` repeats the screen after the credits (C`0x12a6d8`).
- `initialize_game` (C`0x12b7ee`) runs the title (`show_title`), the entrance, then `load_game` until it succeeds.

Port: `src/ui/startend.c`, `src/ui/gameloop.c`. UNVERIFIED: the labels of the two buttons (they are in the pictures).

### End of the game

DM: wall sensor type 18 waits, clears the saved-game flag, and calls `endgame(1)`: the victory screen with champion portraits and skills, then the credits.

CSB (each address checked in the APPB, APPA, and ANIM disassembly):

1. Wall sensor type 18 (C`0x123970`) sets the game-won flag `-0x4f9c(A5)` (C`0x100182`) to 1 and calls `quit_game` (C`0x1285e8`) through slot `0xb42`. There is no delay, and the saved-game flag stays.
2. `quit_game_to_ending` (C`0x1285ee`) waits until the pointer is hidden, closes `Graphics.DAT`, fills zones 401, 13, 11, 9 with color 0, and writes the module tag `ENDA` (`0x454e4441`) into the module arguments (C`0x12863a`). Then `system_shutdown` and `rt_longjmp` leave the engine.
3. The launcher (`APPA.c` line 355) runs `ANIM.FTL` for the tags `ANIM`, `TITL`, and `ENDA`. `ANIM.FTL` (C`0x100e96` of ANIM) builds the file name `<tag>.DAT`, plays `ENDA.DAT`, and then loops forever on the last frame. The chunk format: [data.md "ENDA.DAT and the endgame"](data.md#endadat-and-the-endgame).
4. When the won flag is 0, `quit_game_to_ending` stops in an endless loop (C`0x128600`: `beq` to itself). This is the frozen screen after the credits of SAVE AND QUIT and after the credits of a death without a saved game.

CSB `endgame` (C`0x128374`) is only the death screen and the quit screen:

- Party dead and not won: sound 7 (scream) at the party, 240 frames.
- THE END: graphic 6 in zone 5, palette with 16 × `0x004` and color 15 `0xfff` (C`0x12842e`–`0x128464`), 300 frames (only the first time).
- With a saved game (`-0xe0(A5)`): RESTART THIS GAME. `fill_screen_box` of the box at C`0x100dcc` (x 103, y 145, width 115, height 15) with color 1 and of the box at C`0x100dd4` (105, 147, 111, 11) with color 0, `text_print_to_screen`(110, 154, color 4, color 0, text 77). Colors 1 = `0x86f` and 4 = `0xfff`. The endgame mouse table (C`0x103c60`) has RESTART only, with the outer box. No QUIT button, no champion list.
- 900 frames, or RESTART: `load_game`, then `start_game`. When the load fails, the credits.
- The credits: graphic 5, palette C`0x100f18`, wait for input. Then, with a saved game and a dead party, THE END again (without the 300 frames); otherwise `quit_game`.

Port (`src/ui/startend.c`): `startend_run_endgame_sensor` plays `ENDA.DAT` with `src/ui/anim.c` for CSB, waits for a key or a click, and shows the CSB screen. `show_the_end` draws the CSB button from the boxes, which the image copies to `0x104248` (`CSB_RESTART_BOXES`). After the credits, the port shows the CSB screen (deliberate deviation, as for DM); it does not go back to THE END.

### Copy protection

CSB contains copy protection. The port leaves it out and acts as if every check passes. The functions have the subsystem `protect` in the CSV.

- Disk access through CIA and `DSKPT` inside `sound_request_play` (`protect_disk_access`, C`0x1097e6`).
- Track bit pattern checks: `protect_compare_track_bits` (C`0x1244f8`), timeline event 22 (`protect_check_10c2b0`, every 500 ticks from `process_new_party_map` C`0x107f7a`).
- Code checksums: C`0x120b4e` (over C`0x128ca0`–`0x129580`, from `load_dungeon`), C`0x1245b4` (from `move_get_move_result` on maps above 2), C`0x128676`.
- Self-modifying hooks: `protect_install_hook` (C`0x11e248`) patches `champion_apply_time_effects` and runs graphic 21 as code (from `decrease_torches_light_power` C`0x111018`, and C`0x11c896`, `0x125cac`, `0x125da0`).
- Penalties when a check fails: `timeline_add_event` adds no event (C`0x122d46`), `timeline_is_first_event_expired` returns 0 (C`0x122e54`), movement destinations change (C`0x1216e6`), the timeline dispatcher uses a stale event type (C`0x124148`).

UNVERIFIED: the complete list of penalties.

### Platform and other code

These differences need no port change, because the port does not reproduce the Amiga layer: video driver and display setup, heap, interrupts, `mouse_begin_change`, OS stubs, trackdisk, and the language support of DM.

## Open questions

1. The DM disk has an empty `CSBGAME.DAT`. Does DM 3.60 read it? Not traced.
