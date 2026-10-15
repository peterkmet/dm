# KAOS image addresses outside data/tables.c — inventory for PRD R2

Repo state: main @ b3d1852. All paths relative to `port/src/` unless noted. "csv" = `docs/kaos-symbols.csv` (header `address,name,signature,subsystem`). "copy" = row in `data/csbimage.c` `csb_copies[]` (size in bytes).

## 1. Table A — address constants outside data/tables.c

Legend for the last column: name in `data/tables.c` (T), `data/tables.h` (TH), `data/spells.h` (SP), `data/csbimage.h` (CI), or "—".

### engine/

| file:line | name | value | original symbol (csv / comment) | element type x count (from call sites) | in data/? |
|---|---|---|---|---|---|
| engine/game.c:14 | DIRECTION_TO_STEP_EAST | 0x101038 | g_direction_to_step_east | int16 x 4 | — (dup view.c:23) |
| engine/game.c:15 | DIRECTION_TO_STEP_NORTH | 0x101040 | g_direction_to_step_north | int16 x 4 | — (dup view.c:24) |
| engine/group.c:16 | KAOS_ORDERED_CELLS_TO_ATTACK | 0x100b66 | g_ordered_cells_to_attack uint8_t[8][4] | uint8 [8][4] | — |
| engine/group.c:17 | KAOS_WOUND_PROBABILITY_INDEX_TO_MASK | 0x100b86 | g_wound_probability_index_to_mask uint8_t[5] | uint8 x 4 (csv says 5; next table starts at +4; copy is 4) | — |
| engine/group.c:18 | KAOS_SLOTS_TO_STEAL_FROM | 0x100b8a | (not in csv; copy comment g_slots_to_steal_from) | uint8 x 8 | — |
| engine/move.c:15 | KAOS_MOVEMENT_ARROW_TO_STEP_FORWARD | 0x103c56 | g_movement_arrow_to_step_forward_count int16_t[4] | int16 x 4 | — |
| engine/move.c:16 | KAOS_MOVEMENT_ARROW_TO_STEP_RIGHT | 0x103c5e | g_movement_arrow_to_step_right_count int16_t[4] | int16 x 4 | — |
| engine/recruit.c:7 | G_PORTRAIT_WIDTH | 0x102958 | g_portrait_width | int16 x 1 | — (dup view.h:11 PORTRAIT_SIZE) |
| engine/recruit.c:8 | G_PORTRAIT_HEIGHT | 0x10295a | g_portrait_height | int16 x 1 | — (= PORTRAIT_SIZE + 2) |
| engine/inventory.c:13 | TORCH_CHARGE_TO_ICON_OFFSET | 0x100ba0 | g_torch_charge_to_icon_offset uint8_t[16] | uint8 x 16 | — |
| engine/champion.c:21 | SLOT_DROP_ORDER | 0x100d0e | g_slot_drop_order int16_t[30] | uint16 x 30 (read u16) | — |
| engine/champion.h:14 | CHAMPION_COLORS | 0x100ce0 | g_champion_colors uint8_t[4] | uint8 x 4 | — (used by engine/champion.c and ui/ui_champion.c) |
| engine/timeline_process.c:18 | KAOS_LIGHT_POWER_TO_LIGHT_AMOUNT | 0x100cb4 | g_light_power_to_light_amount int16_t[16] | int16 x 16 | T LIGHT_POWER_TO_LIGHT_AMOUNT (cached, `tables_light_power_to_light_amount`) |
| engine/map.c:13 | KAOS_DOOR_INFO | 0x1016e2 | g_door_info uint16_t[] (hi byte attributes) | uint16 x 4 | TH DOOR_INFO (tables.h:9, cached as DoorInfo bytes) |
| engine/map.c:14 | KAOS_CREATURE_INFO | 0x10192e | g_creature_info CreatureInfo[] 26 bytes | 26-byte record x 27; only word +2 (attributes) read | T CREATURE_INFO (cached) |
| engine/map.c:15-16 | CREATURE_INFO_SIZE 26, CREATURE_INFO_ATTRIBUTES 2 | (offsets, not addresses) | | | duplicates T CREATURE_INFO_BYTES / CREATURE_AT_ATTRIBUTES |
| engine/sensor.c:15 | KAOS_SQUARE_TYPE_TO_EVENT_TYPE | 0x100d4a | g_square_type_to_event_type uint8_t[8] | uint8 x 8 | — |
| engine/sound.c:11 | G_SOUNDS | 0x100d52 | g_sounds uint8_t[35][12] | 12-byte record x 35: s16 graphic @0, u8 rate @3, u8 priority @4 | — |
| engine/sound.c:12 | G_SOUND_VOLUME_TABLE | 0x100696 | g_sound_volume_table uint8_t[25][25] | uint8 [25][25] | — |
| engine/dungeon.c:19 | NEW_GAME_EXTRA_THINGS | 0x101058 | g_new_game_extra_thing_count uint8_t[16] | uint8 x 16 | — |
| engine/dungeon.c:20 | MESSAGE_ESCAPES | 0x1016ea | g_message_escape_strings char[32][8] | char[8] x 32 (string block) | — |
| engine/dungeon.c:21 | REPLACEMENT_CHARS | 0x1017ea | g_escape_replacement_chars char[32][2] | char[2] x 32 | — |
| engine/dungeon.c:22 | INSCRIPTION_ESCAPES | 0x10182a | g_inscription_escape_strings char[32][8] | char[8] x 32 | — |
| engine/dungeon.c:24 | MAX_DUNGEON_SIZE | 0x100000 | NOT an address: a 1 MiB size limit (csv match at 0x100000 is a coincidence) | — | — (a 0x10xxxx grep check must skip it or it becomes `(1u << 20)`) |

### ui/ and platform/

| file:line | name | value | original symbol | element type x count | in data/? |
|---|---|---|---|---|---|
| ui/ui.h:15 | SKILL_LEVEL_STRINGS | 0x101e74 | g_skill_level_string_indices int16_t[15] (level 2-16) | int16 text index x 15 | — (csbtext.c:56 writes it) |
| ui/ui.h:16 | BASE_SKILL_STRINGS | 0x102472 | g_base_skill_string_indices int16_t[4] | int16 x 4 | — (csbtext.c:55 writes it) |
| ui/view.h:8 | DUNGEON_PALETTE | 0x100aa6 | g_dungeon_palettes uint16_t[][16] | uint16 [6][16] (192 bytes) | — |
| ui/view.h:9 | VIEWPORT_ORIGIN | 0x10293a | (no csv name; csb-engine-diff.md: viewport origin) | int16 x 2 (x, y) | — |
| ui/view.h:10 | VIEWPORT_SIZE | 0x102948 | g_viewport_width, g_viewport_height | int16 x 2 | — |
| ui/view.h:11 | PORTRAIT_SIZE | 0x102958 | g_portrait_width, g_portrait_height | int16 x 2 | — |
| ui/layout.h:7 | LAYOUT_BITMAPS | 0x1025f2 | g_layout_bitmaps LayoutBitmap[] 14 bytes | 14-byte record x 60 (copy 840); words x@2 y@4 w@6 h@8 | — |
| ui/command.c:27 | KEYS_INTERFACE | 0x103948 | (no csv name) "interface keys" | KeyboardInput {s16 command, u16 key} x 7 (28 bytes, 0-terminated) | — |
| ui/command.c:28 | KEYS_MOVEMENT | 0x103964 | (no csv name) | KeyboardInput x 7 | — |
| ui/command.c:29 | KEYS_ENTRANCE | 0x10393c | (no csv name) | KeyboardInput x 2 (DM), 1 (CSB) | — |
| ui/command.c:30 | KEYS_SLEEPING | 0x103980 | g_keyboard_input_party_sleeping | KeyboardInput x 3 | — |
| ui/command.c:31 | KEYS_FROZEN | 0x10398c | g_keyboard_input_frozen_game | KeyboardInput x 3 | — |
| ui/gameloop.c:38 | ENTRANCE_PALETTE | 0x100a66 | g_entrance_palette uint16_t[16] | uint16 x 16 | — |
| ui/gameloop.c:39 | SPRITE_PALETTE | 0x100190 | g_initial_colormap + 32 (second half) | uint16 x 16 | — |
| ui/gameloop.c:40 | ARROW_POINTER | 0x1002b0 | g_arrow_pointer_bitmap uint8_t[288]; width word @-4, height @-2 | 4 bit planes block | — (copy starts at 0x1002ac) |
| ui/gameloop.c:41 | HAND_POINTER | 0x1003d4 | g_hand_pointer_bitmap uint8_t[288] | 4 bit planes block | — (copy at 0x1003d0) |
| ui/gameloop.c:42 | POINTER_ZONES | 0x1004f8 | g_mouse_pointer_zones int16_t[16][6] | {x,y,w,h,shape,champion} x 16 | — |
| ui/gameloop.c:43 | POINTER_HOTSPOTS | 0x1005c8 | g_mouse_pointer_hotspots int16_t[4][4] | {x,y,w,h} x 4, only x,y read | — |
| ui/gameloop.c:44 | POINTER_SHADOW_OBJECT | 0x1005fe | g_pointer_shadow_colors_object uint8_t[16] | uint8 color map x 16 | — |
| ui/gameloop.c:45 | POINTER_SHADOW_OFFSET | 0x102952 | g_pointer_shadow_dx, g_pointer_shadow_dy | int16 x 2 | — |
| ui/dialog.c:14 | VERSION_TEXT | 0x10402c | g_dialog_version_text char[6] | C string ("V3.6"; CSB "V3.1") | — |
| ui/dialog.c:15 | MESSAGE_COLORS | 0x104026 | g_dialog_message_colors int16_t[2] | int16 x 2 (used as uint8) | — |
| ui/dialog.c:16 | MESSAGE_LINE_PAD | 0x102966 | (no csv name; csv has g_text_char_gap at 0x102964) | int16 x 1 | — |
| ui/startend.c:32 | CREDITS_PALETTE | 0x100a46 | g_credits_palette uint16_t[16] | uint16 x 16 | — |
| ui/startend.c:33 | BUTTON_BORDER | 0x102988 | (no csv name) | int16 x 2 | — (not in csb copy: DM-only path) |
| ui/startend.c:34 | BUTTON_INNER_BORDER | 0x10298c | (no csv name) | int16 x 2 | — (not in csb copy) |
| ui/ui_menus.c:19 | ACTION_ICON_PALETTE | 0x103ffa | g_palette_changes_action_area_object_icon uint8_t[16] | uint8 color map x 16 | — |
| ui/ui_menus.c:20 | DAMAGE_SIZE_LARGE | 0x102978 | (no csv name) | int16 x 2 (w, h) | — |
| ui/ui_menus.c:21 | DAMAGE_SIZE_SMALL | 0x10297c | (no csv name) | int16 x 2 | — |
| ui/ui_champion.c:25 | INVISIBILITY_PALETTE | 0x10247a | g_invisibility_palette_changes uint8_t[16] | uint8 color map x 16 | — |
| ui/ui_champion.c:26 | STATISTIC_STRINGS | 0x101e9a | g_statistic_string_indices int16_t[7] | int16 x 7 | — (csbtext.c:58 writes it) |
| ui/ui_champion.c:27 | CHAMPION_ICON_SIZE | 0x10295c | g_champion_icon_width, _height | int16 x 2 | — |
| ui/ui_champion.c:28 | BAR_SHADOW | 0x102980 | (no csv name) | int16 x 1 | — |
| ui/ui_champion.c:29 | DIRECTION_STRINGS | 0x101e92 | g_direction_string_indices int16_t[4] | int16 x 4 | — (csbtext.c:57 writes it) |
| ui/ui_champion.c:30 | LIST_SEPARATOR | 0x101ebe | (no csv name) ", " | C string (3 bytes) | — |
| ui/ui_champion.c:31 | BONES_SEPARATOR | 0x101ec6 | (no csv name) " " | C string (2) | — |
| ui/ui_champion.c:32 | ATTRIBUTES_SUFFIX | 0x101ec8 | (no csv name) ")" | C string (2) | — |
| ui/ui_champion.c:33 | ATTRIBUTES_PREFIX | 0x101eca | (no csv name) "(" | C string (2) | — |
| ui/ui.c:19 | BOTTOM_PALETTE | 0x100170 | g_initial_colormap[0..15] | uint16 x 16 | — |
| ui/ui.c:20 | ICON_FIRST | 0x100b92 | g_icon_graphic_first_index uint16_t[7] | int16 x 7 | — |
| ui/ui.c:21 | SLOT_BOXES | 0x100bb0 | g_slot_boxes int16_t[][2] | {s16 zone, s16 icon} x 46; only zone read | — |
| ui/ui.c:22 | SLOT_BOX_BORDER | 0x102956 | g_slot_box_border int16_t | int16 x 1 | — |
| ui/ui.c:23 | MOUSE_INPUT_INTERFACE | 0x1031f8 | g_mouse_input_interface MouseInput[] | MouseInput 12 bytes {command,x1,x2,y1,y2,buttons}, 0-terminated | — |
| ui/ui.c:24 | MOUSE_INPUT_MOVEMENT | 0x1032e8 | g_mouse_input_movement | same | — |
| ui/ui.c:25 | MOUSE_INPUT_INVENTORY | 0x103354 | g_mouse_input_champion_inventory | same | — |
| ui/ui.c:26 | MOUSE_INPUT_SLEEPING | 0x10351c | g_mouse_input_party_sleeping | same | — |
| ui/ui.c:27 | MOUSE_INPUT_FROZEN | 0x103540 | g_mouse_input_frozen_game | same | — |
| ui/ui.c:28 | MOUSE_INPUT_ACTION_NAMES | 0x103564 | g_mouse_input_action_area_names | same | — |
| ui/ui.c:29 | MOUSE_INPUT_ACTION_ICONS | 0x1035a0 | g_mouse_input_action_area_icons | same | — |
| ui/ui.c:30 | MOUSE_INPUT_SPELL_AREA | 0x1035dc | g_mouse_input_spell_area | same | — |
| ui/ui.c:31 | MOUSE_INPUT_STATUS_BOX | 0x103654 | g_mouse_input_champion_status_box | same | — |
| ui/ui.c:32 | MOUSE_INPUT_CHEST | 0x1036f0 | g_mouse_input_panel_chest | same | — |
| ui/ui.c:33 | MOUSE_INPUT_RESURRECT | 0x10375c | g_mouse_input_panel_resurrect_reincarnate | same | — |
| ui/ui.c:34 | MOUSE_INPUT_NAME_ENTRY | 0x10378c | g_mouse_input_champion_name_entry | same | — |
| ui/ui.c:35 | MOUSE_INPUT_ENTRANCE | 0x10318c | (no csv name) | same | — |
| ui/ui.c:36 | MOUSE_INPUT_ENDGAME | 0x1031d4 | (no csv name) | same; WRITTEN by startend.c:237 | — |
| ui/ui.c:37-41 | mouse_tables[] | array of the 14 addresses above, indexed by UI_INPUT_* | | | — |
| ui/ui.c:42 | DIALOG_MOUSE_INPUTS | 0x103bd4 | g_dialog_mouse_inputs void*[8] | uint32 pointer x 8 (into the dialog tables 0x103a84) | — (csbimage.c:12 KAOS_DIALOG_MOUSE_INPUTS) |
| ui/view.c:23 | STEP_EAST | 0x101038 | g_direction_to_step_east | int16 x 4 | — (dup game.c:14) |
| ui/view.c:24 | STEP_NORTH | 0x101040 | g_direction_to_step_north | int16 x 4 | — (dup game.c:15) |
| ui/view.c:25 | GLYPH_SIZE | 0x10296e | (no csv name) wall text glyph w, h | int16 x 2 | — |
| ui/view.c:26 | WALL_SET_IDS | 0x102a22 | g_wall_set_layout_ids int16_t[46] | int16 x 46 | — |
| ui/view.c:27 | WALL_ORN_DEPTH | 0x102ba6 | (no csv name) | uint8 x 14 | — |
| ui/view.c:28 | FLOOR_ORN_VIEW | 0x102bb4 | (no csv name) | uint8 x 11 | — |
| ui/view.c:29 | ALCOVES | 0x102bc0 | g_alcove_ornament_indices uint8_t[3] | uint8 x 3 | — |
| ui/view.c:30 | FOUNTAINS | 0x102bc3 | g_fountain_ornament_indices uint8_t[1] | uint8 x 1 | — |
| ui/view.c:32 | WALL_ORN_COORDS | 0x102bc4 | g_wall_ornament_coord_sets uint8_t[60] | uint8 x 60 | — |
| ui/view.c:33 | FLOOR_ORN_COORDS | 0x102c00 | g_floor_ornament_coord_sets uint8_t[10] | uint8 x 9 (copy 9) | — |
| ui/view.c:34 | DOOR_ORN_COORDS | 0x102c0a | g_door_ornament_coord_sets uint8_t[12] | uint8 x 12 | — |
| ui/view.c:35 | DOOR_BUTTON_COORDS | 0x102c16 | g_door_button_coord_sets uint8_t[2] | uint8 x 1 (only [0] read) | — |
| ui/view.c:36 | PAL_D3 | 0x102c18 | g_palette_changes_d3 uint8_t[16] | color map 16 | — |
| ui/view.c:37 | PAL_D2 | 0x102c28 | g_palette_changes_d2 | color map 16 | — |
| ui/view.c:38 | PAL_DOOR_D3 | 0x102c38 | g_palette_changes_door_ornament_d3 | color map 16 | — |
| ui/view.c:39 | PAL_DOOR_D2 | 0x102c48 | g_palette_changes_door_ornament_d2 | color map 16 | — |
| ui/view.c:40 | TEXT_HEIGHTS | 0x102c57 | (no csv name) plaque height by lines (1-3) + 3 x depth | uint8, read at +1..+15 (copy 0x102c58, 16) | — |
| ui/view.c:41 | LIGHT_THRESHOLDS | 0x100cd4 | g_palette_light_thresholds int16_t[6] | int16 x 6 | T PALETTE_LIGHT_THRESHOLDS (cached `tables_palette_light_threshold`) |
| ui/view.c:42 | GROUP_DIRS | 0x10192a | g_group_directions_packed uint8_t[4] | uint8 x 4 | — |
| ui/view.c:43-49 | VS_LANE, VS_DEPTH, VS_OBJECT, VS_ALCOVE, VS_CREATURE, VS_EXPLOSION, VS_FIELD | 0x102a7e, 0x102a96, 0x102aae, 0x102ac6, 0x102ade, 0x102af6, 0x102b0e | g_view_square_lane / _depth / _object_coord_index / _alcove_object_coord_index / _creature_coord_index / _explosion_coord_index / _field_aspect_index, int8_t[24] | int8 x 23 each (copy 23; stride 24) | — |
| ui/view.c:50 | FIELD_ASPECTS | 0x102b26 | g_field_aspects FieldAspect[12] | 8-byte record x 12 | — |
| ui/view.c:51 | OBJECT_ASPECTS | 0x102c68 | g_object_aspects uint8_t[85][4] | 4-byte record x 85 | — |
| ui/view.c:52 | PROJECTILE_ASPECTS | 0x102db8 | g_projectile_aspects uint8_t[15][4] | 4-byte record, index 1-14 (index 0 = last object aspect) | — |
| ui/view.c:53 | PAL_SMOKE | 0x102df4 | g_palette_changes_smoke uint8_t[16] | color map 16 | — |
| ui/view.c:54 | PAL_OBJECT_D3 | 0x102e04 | (no csv name) | color map 16 | — |
| ui/view.c:55 | PAL_OBJECT_D2 | 0x102e14 | (no csv name) | color map 16 | — |
| ui/view.c:56 | OBJECT_SCALES | 0x102e24 | (no csv name) | uint8 x 5 | — |
| ui/view.c:57 | PROJECTILE_SCALES | 0x102e2a | (no csv name) | uint8 x 7 | — |
| ui/view.c:58 | EXPLOSION_SCALES | 0x102e32 | (no csv name) | uint8 x 5 | — |
| ui/view.c:59 | PILE_SHIFTS | 0x102e38 | (no csv name) | uint8 pair x 16 | — |
| ui/view.c:60 | CREATURE_ASPECTS | 0x102e58 | g_creature_aspects uint8_t[27][6] | 6-byte record x 27 | — |
| ui/view.c:61 | REPLACEMENT_COLORS | 0x102efa | g_creature_replacement_color_sets uint8_t[13][14] | 14-byte record x 13: uint16 x 6 lights, u8 D2, u8 D3 | — |
| ui/view.c:63 | PAL_CREATURE_D3 | 0x102fb0 | g_palette_changes_creature_d3 | color map 16 | — |
| ui/view.c:64 | PAL_CREATURE_D2 | 0x102fc0 | g_palette_changes_creature_d2 | color map 16 | — |
| ui/view.c:65 | SHIFT_SETS | 0x102fd0 | g_creature_shift_sets int8_t[3][8] | int8 [3][8] | — |
| ui/view.c:68 | REBIRTH_SCALES | 0x102fe8 | (no csv name) step 2 [7], then step 1 at +8 [7] | uint8 x 7 x 2 | — |
| ui/logo.c:14 | SWSH_BASE | 0x100000 | NOT KAOS: base of the SWSH.FTL image (own `FtlImage img`, ftl_load at logo.c:32) | — | — |
| ui/logo.c:15 | PALETTE_SCRIPT | 0x10009a | SWSH image (csv hit is KAOS, unrelated) | {s16 index, u16 color} pairs | — |
| ui/logo.c:16 | PALETTE_SCRIPT_END | 0x100106 | SWSH image | end marker | — |
| ui/logo.c:17 | LOGO_GRAPHIC | 0x100106 | SWSH image | packed 320x200 graphic block | — |
| ui/logo.c:18 | SAMPLE_PERIOD | 0x100a54 | SWSH image | uint16 | — |
| ui/logo.c:19 | SAMPLE_LENGTH | 0x100a56 | SWSH image | uint32 | — |
| ui/logo.c:20 | SAMPLE_DATA | 0x100a5a | SWSH image | int8 sample block | — |
| ui/anim.c:35, 40, 285 | (comments only) | 0x101424, 0x102e4c | CSB disk B ANIM.FTL addresses in comments; MUSIC_PERIOD = 328 and MUSIC_FADE_FRAMES = 3 are values, not addresses | — | a text grep for 0x10xxxx flags these comments |

platform/ has no address define; it uses VIEWPORT_ORIGIN and DUNGEON_PALETTE from ui/view.h and `ui_mouse_table_address`.

### Inside data/ but not in tables.c (R2 says every address goes to tables.c with KAOS_)

| file:line | name | value | note |
|---|---|---|---|
| data/tables.h:9 | DOOR_INFO | 0x1016e2 | public header; used by tables.c:105 and ui/view.c:1549 |
| data/spells.h:6 | G_SYMBOL_BASE_MANA_COST | 0x103c66 | uint8_t[4][6]; used by tables.c:121 only |
| data/spells.h:7 | G_SYMBOL_MANA_COST_MULTIPLIER | 0x103c7e | uint8_t[6]; tables.c:123 |
| data/spells.h:8 | G_SPELLS | 0x103c84 | SpellRecord[25] 8 bytes; tables.c:114 |
| data/csbimage.h:8 | CSB_SPELLS | 0x104160 | synthetic CSB-only address; tables.c:114, csbimage.c:147 |
| data/csbimage.h:10 | CSB_RESTART_BOXES | 0x104248 | synthetic CSB-only; ui/startend.c:310, csbimage.c:148 |
| data/ftl.h:7 | KAOS_BASE | 0x100000 | image base |
| data/csbimage.c:10,12,14 | KAOS_ACTION_SETS, KAOS_DIALOG_MOUSE_INPUTS, KAOS_DIALOG_TABLES | 0x103d4c, 0x103bd4, 0x103a84 | dup of tables.c:14 ACTION_SETS and ui.c:42 DIALOG_MOUSE_INPUTS |

PRD naming rule "no address name with TICKS or SIZE" hits: tables.c:15 ACTION_DISABLED_TICKS; view.h:10 VIEWPORT_SIZE; view.h:11 PORTRAIT_SIZE; ui_champion.c:27 CHAMPION_ICON_SIZE; view.c:25 GLYPH_SIZE; ui_menus.c:20-21 DAMAGE_SIZE_LARGE/SMALL; dungeon.c:24 MAX_DUNGEON_SIZE (not an address).

## 2. Table B — kaos_* call sites outside data/ (grouped by file)

`ui/ui.h:84` and `engine/dungeon.c:18` mention kaos_load in comments only. `kaos_load` calls: `ui/gameloop.c:78`, `platform/main.c:41` (loader; stays, it is the data API, not a table read).

### engine/champion.c
- :181 `kaos_u8(CHAMPION_COLORS + champion_index)` — 0x100ce0 + i
- :753 `kaos_u16(SLOT_DROP_ORDER + i * 2)` — 0x100d0e + 2i

### engine/dungeon.c
- :194 `kaos_u8(NEW_GAME_EXTRA_THINGS + t)` — 0x101058 + t
- :474 `(const char *)kaos_ptr(REPLACEMENT_CHARS + code * REPLACEMENT_BYTES)` — 0x1017ea + 2 code (raw pointer, string)
- :475 `(const char *)kaos_ptr((mode == MODE_INSCRIPTION ? INSCRIPTION_ESCAPES : MESSAGE_ESCAPES) + code * ESCAPE_STRING_BYTES)` — 0x10182a / 0x1016ea + 8 code (raw pointer, string)

### engine/game.c
- :102 `kaos_s16(DIRECTION_TO_STEP_EAST + direction * 2)`, `kaos_s16(DIRECTION_TO_STEP_EAST + right * 2)` — 0x101038
- :103 same with DIRECTION_TO_STEP_NORTH — 0x101040

### engine/group.c
- :628 `kaos_u8(KAOS_SLOTS_TO_STEAL_FROM + (slot_index & 7))` — 0x100b8a
- :1053 `kaos_u8(row + i)`, row = `KAOS_ORDERED_CELLS_TO_ATTACK + (((group_cell >> 1) & 1) + direction * 2) * 4` (:1051) — 0x100b66
- :1082 `kaos_u8(KAOS_WOUND_PROBABILITY_INDEX_TO_MASK + index)` — 0x100b86

### engine/inventory.c
- :97 `kaos_u8(TORCH_CHARGE_TO_ICON_OFFSET + weapon_charges(thing))` — 0x100ba0

### engine/map.c
- :44 `kaos_u16(KAOS_DOOR_INFO + map_door_set(map, door) * 2)` — 0x1016e2
- :261 `kaos_u16(KAOS_CREATURE_INFO + type * CREATURE_INFO_SIZE + CREATURE_INFO_ATTRIBUTES)` — 0x10192e + 26t + 2

### engine/move.c
- :353 `kaos_s16(KAOS_MOVEMENT_ARROW_TO_STEP_FORWARD + arrow * 2)` — 0x103c56
- :354 `kaos_s16(KAOS_MOVEMENT_ARROW_TO_STEP_RIGHT + arrow * 2)` — 0x103c5e

### engine/recruit.c
- :107 `kaos_s16(G_PORTRAIT_WIDTH)` — 0x102958
- :108 `kaos_s16(G_PORTRAIT_HEIGHT)` — 0x10295a

### engine/sensor.c
- :109 `kaos_u8(KAOS_SQUARE_TYPE_TO_EVENT_TYPE + target_type)` — 0x100d4a

### engine/sound.c
- :51 `kaos_s16(G_SOUNDS + SOUND_ENTRY_SIZE * sound)` — 0x100d52 + 12s
- :52 `kaos_u8(G_SOUNDS + SOUND_ENTRY_SIZE * sound + SOUND_ENTRY_RATE)` — +3
- :54 `kaos_u8(G_SOUNDS + SOUND_ENTRY_SIZE * sound + SOUND_ENTRY_PRIORITY)` — +4
- :102 `kaos_u8(row + SOUND_HEARING_RANGE + side)`, row = `G_SOUND_VOLUME_TABLE + (ahead + 12) * 25` (:101) — 0x100696
- :103 `kaos_u8(row + SOUND_HEARING_RANGE - side)` — 0x100696

### engine/timeline_process.c
- :334 `kaos_s16(KAOS_LIGHT_POWER_TO_LIGHT_AMOUNT + power * 2) - kaos_s16(KAOS_LIGHT_POWER_TO_LIGHT_AMOUNT + rest * 2)` — 0x100cb4

### platform/control.c
- :140-141 `kaos_s16(VIEWPORT_ORIGIN)`, `kaos_s16(VIEWPORT_ORIGIN + 2)` — 0x10293a
- :169 loop `a = ui_mouse_table_address(table); kaos_s16(a + 2 * INPUT_COMMAND)` — mouse table (any)
- :170 `kaos_s16(a + 2 * INPUT_COMMAND)`
- :171 `kaos_s16(a + 2 * INPUT_X1)`, `kaos_s16(a + 2 * INPUT_X2)`
- :172 `kaos_s16(a + 2 * INPUT_Y1)`
- :174 `kaos_s16(a + 2 * INPUT_Y2)`

### platform/main.c
- :41 `kaos_load(path)` (loader)
- :53 `kaos_u16(DUNGEON_PALETTE + 2 * i)` — 0x100aa6 (palette 0)

### ui/command.c
- :144 `kaos_s16(entry)` loop over key table (uint32 `table` from primary_keys/secondary_keys = KEYS_* at :84, :113, :114, :269, :668, :690)
- :145 `kaos_u16(entry + KEY_ENTRY_KEY)`, `kaos_s16(entry)` — KEYS_* 0x10393c..0x10398c
- :785 `kaos_s16(VIEWPORT_ORIGIN)`, `kaos_s16(VIEWPORT_ORIGIN + 2)` — 0x10293a

### ui/dialog.c
- :65-66 `kaos_s16(source + LAYOUT_BITMAP_X/Y/W/H)`, source = `LAYOUT_BITMAPS + (patch_layout - 1) * 14` (:64) — 0x1025f2
- :90 `kaos_s16(MESSAGE_LINE_PAD)` — 0x102966
- :101 `(uint8_t)kaos_s16(MESSAGE_COLORS + 2 * i)` — 0x104026
- :124 `(const char *)kaos_ptr(VERSION_TEXT)` — 0x10402c (raw pointer, string)
- :146 `kaos_s16(entry + 2 * INPUT_X2)`, entry = `ui_mouse_table_address(table) + (choice - 1) * 12` (:144) — dialog mouse table

### ui/gameloop.c
- :78 `kaos_load(path)` (loader)
- :120 `kaos_u16(ENTRANCE_PALETTE + 2 * i)` — 0x100a66
- :472-473 `kaos_u16(address - POINTER_WIDTH_BEFORE)`, `kaos_u16(address - POINTER_HEIGHT_BEFORE)` — 0x1002ac/0x1002ae or 0x1003d0/0x1003d2
- :479 `kaos_u8(address + p * plane + y * row_bytes + x / CHAR_BIT)` — ARROW_POINTER 0x1002b0 / HAND_POINTER 0x1003d4 planes (address passed at :535)
- :491 `kaos_s16(POINTER_SHADOW_OFFSET)`, `kaos_s16(POINTER_SHADOW_OFFSET + 2)` — 0x102952
- :498 `kaos_u8(POINTER_SHADOW_OBJECT + icon_px[...])` — 0x1005fe
- :515-516, 518-519 `kaos_s16(z + POINTER_ZONE_X/Y/W/H/CHAMPION/SHAPE)`, z = `POINTER_ZONES + 12 i` (:514) — 0x1004f8
- :536-537 `kaos_s16(POINTER_HOTSPOTS + 8 * shape + HOTSPOT_X/Y)` — 0x1005c8
- :544 `kaos_u16(SPRITE_PALETTE + 2 * i)` — 0x100190

### ui/startend.c
- :235 `uint8_t *box = kaos_ptr(ui_mouse_table_address(UI_INPUT_ENDGAME) + entry_index * MOUSE_ENTRY_BYTES + MOUSE_ENTRY_BOX)` then :237 `WR16(box + 2 * i, words[i])` — WRITE into 0x1031d4 table (the only image write outside data/)
- :247 `kaos_s16(BUTTON_INNER_BORDER)`, `kaos_s16(BUTTON_INNER_BORDER + 2)` — 0x10298c
- :249 `kaos_s16(BUTTON_BORDER)`, `kaos_s16(BUTTON_BORDER + 2)` — 0x102988
- :266 `kaos_s16(SKILL_LEVEL_STRINGS + 2 * (level - 2))` — 0x101e74
- :267 `kaos_s16(BASE_SKILL_STRINGS + 2 * skill)` — 0x102472
- :277 `kaos_s16(PORTRAIT_SIZE)`, `kaos_s16(PORTRAIT_SIZE + 2)` — 0x102958
- :303 `kaos_s16(address)`, `+2`, `+4`, `+6` in csb_box(address), called :310 with CSB_RESTART_BOXES and CSB_RESTART_BOXES + 8 — 0x104248
- :368 `kaos_u16(CREDITS_PALETTE + 2 * i)` — 0x100a46

### ui/ui.c
- :63 `kaos_u16(VIEWPORT_SIZE)`, `kaos_u16(VIEWPORT_SIZE + 2)` — 0x102948
- :147, :156, :166 `kaos_u16(BOTTOM_PALETTE + 2 * i)` — 0x100170
- :225-226 `kaos_s16(ICON_FIRST + 2 * sheet)` — 0x100b92
- :239 `kaos_s16(SLOT_BOX_BORDER)` — 0x102956
- :248 `kaos_s16(SLOT_BOXES + 4 * slot_box)` — 0x100bb0
- :300 `kaos_u32(DIALOG_MOUSE_INPUTS + 4 * (table - UI_INPUT_DIALOG_FIRST))` — 0x103bd4 (returns an address)
- :310-311, :314 `kaos_s16/u16(a + 2 * INPUT_COMMAND/X1/X2/BUTTONS/Y1/Y2)` — mouse tables (ui_mouse_table_address :298)
- :319-320 `kaos_s16(VIEWPORT_ORIGIN)`, `+2` — 0x10293a

### ui/ui_champion.c
- :84 `kaos_u8(CHAMPION_COLORS + champion_index)` — 0x100ce0
- :175 `kaos_s16(CHAMPION_ICON_SIZE)`, `+2` — 0x10295c
- :185 `kaos_u8(INVISIBILITY_PALETTE + (px[i] & 15))` — 0x10247a
- :202-203 `kaos_s16(PORTRAIT_SIZE)`, `+2` — 0x102958
- :347-348 `kaos_s16(BAR_SHADOW)` (x and y) — 0x102980
- :401 `kaos_s16(SKILL_LEVEL_STRINGS + 2 * (level - 2))` — 0x101e74
- :419 `kaos_s16(BASE_SKILL_STRINGS + 2 * k)` — 0x102472
- :426 `kaos_s16(STATISTIC_STRINGS + 2 * k)` — 0x101e9a
- :483 `(const char *)kaos_ptr(ATTRIBUTES_PREFIX)` — 0x101eca
- :488 `(const char *)kaos_ptr(LIST_SEPARATOR)` — 0x101ebe
- :491 `(const char *)kaos_ptr(ATTRIBUTES_SUFFIX)` — 0x101ec8
- :502 `(const char *)kaos_ptr(BONES_SEPARATOR)` — 0x101ec6
- :526 `kaos_s16(DIRECTION_STRINGS + 2 * icon)` — 0x101e92

### ui/ui_menus.c
- :122 `kaos_u8(ACTION_ICON_PALETTE + (px[i] & 15))` — 0x103ffa
- :169 `kaos_s16(size)`, `kaos_s16(size + 2)`, size = DAMAGE_SIZE_SMALL/LARGE (:168) — 0x10297c / 0x102978

### ui/view.c
- :71 macro `S8(a) = (int16_t)(int8_t)kaos_u8(a)`; S8 sites: :517-518 (SHIFT_SETS), :602-603 (SHIFT_SETS), :613 (VS_CREATURE), :798 (VS_EXPLOSION, VS_FIELD), :860 (VS_LANE, VS_DEPTH, VS_ALCOVE/VS_OBJECT), :890 (VS_OBJECT), :902 (VS_FIELD)
- :361-364 `kaos_s16(record + LAYOUT_BITMAP_X/Y/W/H)`, record = `LAYOUT_BITMAPS + 14 * slot` (:358) — 0x1025f2
- :449 `kaos_ptr(PAL_OBJECT_D2)` / `kaos_ptr(PAL_OBJECT_D3)` — 0x102e14 / 0x102e04 (raw pointer, color map)
- :503 `kaos_ptr(OBJECT_ASPECTS + 4 * aspect_index)` — 0x102c68 (raw pointer, record)
- :513 `kaos_u8(OBJECT_SCALES + row)` — 0x102e24
- :517-518 `kaos_u8(PILE_SHIFTS + 2 * *pile [+1])` — 0x102e38
- :543 `kaos_ptr(CREATURE_ASPECTS + 6 * aspect_index)` — 0x102e58 (raw pointer, stored in s->aspect)
- :552 `kaos_u8(GROUP_DIRS + rec_group_direction(thing))` — 0x10192a
- :717 `kaos_ptr(PROJECTILE_ASPECTS + 4 * -aspect_index)` — 0x102db8 (raw pointer)
- :722 `kaos_u8(PROJECTILE_SCALES + row)` — 0x102e2a
- :773 `kaos_ptr(PAL_SMOKE)` — 0x102df4 (raw pointer, color map)
- :786 `kaos_u8(PAL_SMOKE + units->px[i])` — 0x102df4
- :813 `kaos_u8(REBIRTH_SCALES + REBIRTH_STEP_1_SCALES + coord)` — 0x102ff0
- :814 `kaos_u8(PROJECTILE_ASPECTS + 4 * PROJECTILE_ASPECT_LIGHTNING + PROJECTILE_ASPECT_GRAPHIC)` — 0x102dc8
- :830 `kaos_u8(REBIRTH_SCALES + coord)` — 0x102fe8
- :834 `kaos_u8(EXPLOSION_SCALES + depth)` — 0x102e32
- :841 `memcpy(aspect, kaos_ptr(FIELD_ASPECTS + 8 * field_index), 8)` — 0x102b26 (raw block)
- :902 `kaos_ptr(FIELD_ASPECTS + 8 * S8(VS_FIELD + view_square))` — 0x102b26 (raw pointer)
- :911 `kaos_ptr(palette_changes)` inside `scaled(..., uint32_t palette_changes, ...)` (:906); callers pass PAL_D3 / PAL_D2 (:966-968, :1025), PAL_DOOR_D2 / PAL_DOOR_D3 (:1012)
- :918 `kaos_s16(GLYPH_SIZE)`, `+2` — 0x10296e
- :936 `kaos_s16(PORTRAIT_SIZE)`, `+2` — 0x102958
- :978 `kaos_u8(TEXT_HEIGHTS + lines + 3 * kaos_u8(WALL_ORN_DEPTH + view))` — 0x102c57, 0x102ba6
- :996 `kaos_u8(FLOOR_ORN_VIEW + view)` — 0x102bb4
- :1028 `kaos_u8(DOOR_BUTTON_COORDS)` — 0x102c16
- :1491 `kaos_u16(set + 2 * l)`, set = `REPLACEMENT_COLORS + 14 * n` (:1490) — 0x102efa
- :1492-1493 `kaos_u8(set + 12)`, `kaos_u8(set + 13)` — 0x102efa
- :1498 `kaos_s16(WALL_SET_IDS + 2 * i)` — 0x102a22
- :1549 `kaos_u16(DOOR_INFO + 2 * set)` — 0x1016e2
- :1565 `kaos_u8(WALL_ORN_COORDS + o)` — 0x102bc4
- :1567 `kaos_u8(ALCOVES + k)` — 0x102bc0
- :1571 `kaos_u8(FOUNTAINS)` — 0x102bc3
- :1575 `kaos_u8(FLOOR_ORN_COORDS + o)` — 0x102c00
- :1580 `kaos_u8(DOOR_ORN_COORDS + o)` — 0x102c0a
- :1592 `kaos_u16(DUNGEON_PALETTE + 32 * l + 2 * i)` — 0x100aa6
- :1593 `memcpy(creature_d3, kaos_ptr(PAL_CREATURE_D3), 16)` — 0x102fb0 (raw block)
- :1594 `memcpy(creature_d2, kaos_ptr(PAL_CREATURE_D2), 16)` — 0x102fc0 (raw block)
- :1598 `kaos_u8(CREATURE_ASPECTS + 6 * creature_types[i] + CREATURE_ASPECT_REPLACEMENT)` — 0x102e58 + 5
- :1629 `kaos_s16(LIGHT_THRESHOLDS + 2 * g_dungeon_view_palette_index)` — 0x100cd4
- :1635-1636 `kaos_s16(STEP_EAST/STEP_NORTH + 2 * dir)` and `+ 2 * ((dir + 1) & 3)` — 0x101038 / 0x101040
- :1876 `kaos_s16(VIEWPORT_SIZE)`, `+2` — 0x102948

### ui/logo.c (SWSH image, no kaos_ call)
- :35-47 `BE16(at + SAMPLE_PERIOD)`, `BE32(at + SAMPLE_LENGTH)`, `at + LOGO_GRAPHIC`, `at + SAMPLE_DATA`, palette script loop — reads its own `img` (SWSH.FTL), not KAOS.

### Indirect address users (no kaos_ call at the site, but hold an image address)
- `ui/ui.h:118` `uint32_t ui_mouse_table_address(int16_t table)` (def ui/ui.c:298); callers ui/dialog.c:144, ui/startend.c:235, ui/ui.c:309, platform/control.c:169.
- `ui/command.c` key tables as uint32 in `InputTables`/primary_keys/secondary_keys: :84, :113-114, :269, :668, :690.
- `ui/gameloop.c:469` `planar_pointer(uint32_t address, ...)`, called :535.
- `ui/view.c:906` `scaled(..., uint32_t palette_changes, ...)`.

## 3. Proposed data/tables.h API

Principle: each function reads one table; ui/engine/platform no longer see an address. Names `tables_<game noun>`. Addresses become `#define KAOS_<NAME>` in data/tables.c (none with TICKS or SIZE). Indices are checked the way the existing GET macro does where a caller can pass a bad index; scalar accessors read at call time (see §4).

### Reuse existing (cached) functions

| new use | existing function | KAOS_ name (in tables.c) | call sites |
|---|---|---|---|
| creature attributes | `tables_creature_info(type)->attributes` | KAOS_CREATURE_INFO (rename T:7 CREATURE_INFO) | engine/map.c:261 (removes map.c:14-16) |
| door word | `tables_door_info(set)`: word = `attributes << 8 \| defense` | KAOS_DOOR_INFO (move TH:9 into tables.c) | engine/map.c:44, ui/view.c:1549. Alternative with no recomposition: add `uint16_t tables_door_attributes_word(int door_set)` |
| light amount | `*tables_light_power_to_light_amount(power)` | KAOS_LIGHT_POWER_TO_LIGHT_AMOUNT | engine/timeline_process.c:334 |
| palette light threshold | `*tables_palette_light_threshold(i)` | KAOS_PALETTE_LIGHT_THRESHOLDS | ui/view.c:1629 (loop: the index must stay < 6; the GET macro returns NULL at 6, the raw read would read 0x100ce0) |

### New functions — engine

```c
int16_t tables_step_east(int direction);            /* KAOS_DIRECTION_TO_STEP_EAST 0x101038, [4]: game.c:102, view.c:1635 */
int16_t tables_step_north(int direction);           /* KAOS_DIRECTION_TO_STEP_NORTH 0x101040, [4]: game.c:103, view.c:1636 */
int16_t tables_arrow_steps_forward(int arrow);      /* KAOS_MOVEMENT_ARROW_TO_STEP_FORWARD 0x103c56, [4]: move.c:353 */
int16_t tables_arrow_steps_right(int arrow);        /* KAOS_MOVEMENT_ARROW_TO_STEP_RIGHT 0x103c5e, [4]: move.c:354 */
uint8_t tables_ordered_cell_to_attack(int row, int i); /* KAOS_ORDERED_CELLS_TO_ATTACK 0x100b66, [8][4]: group.c:1051-1053 */
uint8_t tables_wound_probability_mask(int index);   /* KAOS_WOUND_PROBABILITY_INDEX_TO_MASK 0x100b86, [4]: group.c:1082 */
uint8_t tables_slot_to_steal_from(int index);       /* KAOS_SLOTS_TO_STEAL_FROM 0x100b8a, [8]: group.c:628 */
uint8_t tables_torch_icon_offset(int charges);      /* KAOS_TORCH_CHARGE_TO_ICON_OFFSET 0x100ba0, [16]: inventory.c:97 */
uint16_t tables_slot_drop_order(int i);             /* KAOS_SLOT_DROP_ORDER 0x100d0e, [30]: champion.c:753 */
uint8_t tables_champion_color(int champion_index);  /* KAOS_CHAMPION_COLORS 0x100ce0, [4]: champion.c:181, ui_champion.c:84 (removes champion.h:14) */
uint8_t tables_square_type_event(int square_type);  /* KAOS_SQUARE_TYPE_TO_EVENT_TYPE 0x100d4a, [8]: sensor.c:109 */
int16_t tables_sound_graphic(int sound);            /* KAOS_SOUNDS 0x100d52, [35] x 12 bytes, +0: sound.c:51 */
uint8_t tables_sound_rate(int sound);               /* +3: sound.c:52 */
uint8_t tables_sound_priority(int sound);           /* +4: sound.c:54 */
uint8_t tables_sound_volume(int ahead, int side);   /* KAOS_SOUND_VOLUMES 0x100696, [25][25], args -12..12: sound.c:101-103 */
uint8_t tables_new_game_extra_things(int thing_type); /* KAOS_NEW_GAME_EXTRA_THING_COUNT 0x101058, [16]: dungeon.c:194 */
const char *tables_message_escape(int code);        /* KAOS_MESSAGE_ESCAPE_STRINGS 0x1016ea, [32][8]: dungeon.c:475 */
const char *tables_inscription_escape(int code);    /* KAOS_INSCRIPTION_ESCAPE_STRINGS 0x10182a, [32][8]: dungeon.c:475 */
const char *tables_escape_replacement(int code);    /* KAOS_ESCAPE_REPLACEMENT_CHARS 0x1017ea, [32][2]: dungeon.c:474 */
int16_t tables_portrait_width(void);                /* KAOS_PORTRAIT_WIDTH 0x102958: recruit.c:107, ui_champion.c:202, startend.c:277, view.c:936 */
int16_t tables_portrait_height(void);               /* KAOS_PORTRAIT_HEIGHT 0x10295a: recruit.c:108, ui_champion.c:203, startend.c:277, view.c:936 */
```
The three string functions return raw pointers into the image (8- and 2-byte fixed strings), as today.

### New functions — ui / platform

Palettes (uint16 colors, 16 per palette):
```c
enum { TABLES_PALETTE_INTERFACE, TABLES_PALETTE_POINTER, TABLES_PALETTE_CREDITS, TABLES_PALETTE_ENTRANCE,
       TABLES_PALETTE_DUNGEON /* + light level 0-5 */ };
void tables_palette(int palette, uint16_t colors[16]);
/* KAOS_INITIAL_COLORMAP 0x100170 (INTERFACE) and +32 = 0x100190 (POINTER), KAOS_CREDITS_PALETTE 0x100a46,
   KAOS_ENTRANCE_PALETTE 0x100a66, KAOS_DUNGEON_PALETTES 0x100aa6 [6][16].
   Sites: ui.c:147, 156, 166 (INTERFACE); gameloop.c:544 (POINTER); startend.c:368 (CREDITS); gameloop.c:120 (ENTRANCE);
   view.c:1592 (DUNGEON + l); platform/main.c:53 (DUNGEON). */
```
(ui.c:156 and :166 fill parts of a larger array; with `out` they copy from a local `uint16_t[16]`, or use `uint16_t tables_palette_color(int palette, int color)` to keep the loops one-line. Pick one.)

Palette changes (16-byte color maps; raw pointer because draw_shrink takes `const uint8_t *pal`):
```c
enum { TABLES_CHANGES_D3, TABLES_CHANGES_D2, TABLES_CHANGES_DOOR_ORNAMENT_D3, TABLES_CHANGES_DOOR_ORNAMENT_D2,
       TABLES_CHANGES_SMOKE, TABLES_CHANGES_OBJECT_D3, TABLES_CHANGES_OBJECT_D2, TABLES_CHANGES_CREATURE_D3,
       TABLES_CHANGES_CREATURE_D2, TABLES_CHANGES_INVISIBILITY, TABLES_CHANGES_ACTION_ICON, TABLES_CHANGES_POINTER_SHADOW };
const uint8_t *tables_palette_changes(int changes);
/* KAOS_PALETTE_CHANGES_D3 0x102c18, _D2 0x102c28, _DOOR_ORNAMENT_D3 0x102c38, _DOOR_ORNAMENT_D2 0x102c48,
   _SMOKE 0x102df4, _OBJECT_D3 0x102e04, _OBJECT_D2 0x102e14, _CREATURE_D3 0x102fb0, _CREATURE_D2 0x102fc0,
   KAOS_INVISIBILITY_PALETTE_CHANGES 0x10247a, KAOS_PALETTE_CHANGES_ACTION_ICON 0x103ffa, KAOS_POINTER_SHADOW_COLORS 0x1005fe.
   Sites: view.c:449, 773, 786, 911 (scaled() takes `const uint8_t *` instead of uint32_t; callers :966-968, :1012, :1025),
   view.c:1593-1594 (memcpy), ui_champion.c:185, ui_menus.c:122, gameloop.c:498. */
```

Creature colors and aspects:
```c
uint16_t tables_replacement_color(int set, int light);  /* KAOS_CREATURE_REPLACEMENT_COLOR_SETS 0x102efa [13] x 14: view.c:1491 */
uint8_t tables_replacement_change(int set, int depth);  /* +12 (D2) / +13 (D3): view.c:1492-1493 */
const uint8_t *tables_creature_aspect(int creature_type); /* KAOS_CREATURE_ASPECTS 0x102e58 [27] x 6: view.c:543, 1598 */
const uint8_t *tables_object_aspect(int index);         /* KAOS_OBJECT_ASPECTS 0x102c68 [85] x 4: view.c:503 */
const uint8_t *tables_projectile_aspect(int index);     /* KAOS_PROJECTILE_ASPECTS 0x102db8 [1..14] x 4: view.c:717, 814 */
const uint8_t *tables_field_aspect(int index);          /* KAOS_FIELD_ASPECTS 0x102b26 [12] x 8: view.c:841, 902 */
int16_t tables_creature_shift(int distance, int index); /* KAOS_CREATURE_SHIFT_SETS 0x102fd0 int8 [3][8]: view.c:517-518, 602-603 */
uint8_t tables_pile_shift(int pair, int axis);          /* KAOS_PILE_SHIFTS 0x102e38 [16][2]: view.c:517-518 */
uint8_t tables_object_scale(int row);                   /* KAOS_OBJECT_SCALES 0x102e24 [5]: view.c:513 */
uint8_t tables_projectile_scale(int row);               /* KAOS_PROJECTILE_SCALES 0x102e2a [7]: view.c:722 */
uint8_t tables_explosion_scale(int depth);              /* KAOS_EXPLOSION_SCALES 0x102e32 [5]: view.c:834 */
uint8_t tables_rebirth_scale(int step, int coord);      /* KAOS_REBIRTH_SCALES 0x102fe8 (step 2), +8 (step 1), [7]: view.c:813, 830 */
uint8_t tables_group_directions(int index);             /* KAOS_GROUP_DIRECTIONS_PACKED 0x10192a [4]: view.c:552 */
```
(Aspects: returning `const uint8_t *` keeps view.c's byte-offset enums; an alternative is a typed struct per aspect, which moves the view.c offset enums into tables.h.)

View squares and the dungeon view:
```c
typedef struct { int8_t lane, depth, object_coord, alcove_coord, creature_coord, explosion_coord, field_aspect; } ViewSquare;
ViewSquare tables_view_square(int view_square);         /* KAOS_VIEW_SQUARE_LANE 0x102a7e ... _FIELD_ASPECT 0x102b0e, [23] each:
                                                           view.c:613, 798, 860, 890, 902 (removes the S8 macro :71) */
int16_t tables_wall_set_layout_id(int i);               /* KAOS_WALL_SET_LAYOUT_IDS 0x102a22 [46]: view.c:1498 */
uint8_t tables_wall_ornament_depth(int view);           /* KAOS_WALL_ORNAMENT_DEPTHS 0x102ba6 [14]: view.c:978 */
uint8_t tables_plaque_height(int lines, int depth);     /* KAOS_PLAQUE_HEIGHTS 0x102c57 (+lines + 3 depth): view.c:978 */
uint8_t tables_floor_ornament_view_offset(int view);    /* KAOS_FLOOR_ORNAMENT_VIEW_OFFSETS 0x102bb4 [11]: view.c:996 */
uint8_t tables_alcove_ornament(int k);                  /* KAOS_ALCOVE_ORNAMENT_INDICES 0x102bc0 [3]: view.c:1567 */
uint8_t tables_fountain_ornament(void);                 /* KAOS_FOUNTAIN_ORNAMENT_INDICES 0x102bc3 [1]: view.c:1571 */
uint8_t tables_wall_ornament_coord_set(int ornament);   /* KAOS_WALL_ORNAMENT_COORD_SETS 0x102bc4 [60]: view.c:1565 */
uint8_t tables_floor_ornament_coord_set(int ornament);  /* KAOS_FLOOR_ORNAMENT_COORD_SETS 0x102c00 [9]: view.c:1575 */
uint8_t tables_door_ornament_coord_set(int ornament);   /* KAOS_DOOR_ORNAMENT_COORD_SETS 0x102c0a [12]: view.c:1580 */
uint8_t tables_door_button_coord_set(void);             /* KAOS_DOOR_BUTTON_COORD_SETS 0x102c16: view.c:1028 */
int16_t tables_glyph_width(void); int16_t tables_glyph_height(void); /* KAOS_WALL_TEXT_GLYPH_DIMENSIONS 0x10296e: view.c:918 */
int16_t tables_viewport_x(void); int16_t tables_viewport_y(void);    /* KAOS_VIEWPORT_ORIGIN 0x10293a:
                                                           command.c:785, ui.c:319-320, platform/control.c:140-141 */
int16_t tables_viewport_width(void); int16_t tables_viewport_height(void); /* KAOS_VIEWPORT_WIDTH 0x102948, _HEIGHT 0x10294a:
                                                           ui.c:63 (read u16 today, values small: same), view.c:1876 */
```

Layout bitmaps (data/ cannot use ui/draw.h Box):
```c
typedef struct { int16_t x, y, w, h; } TablesRect;
TablesRect tables_layout_bitmap(int slot);              /* KAOS_LAYOUT_BITMAPS 0x1025f2 [60] x 14, words +2..+8:
                                                           dialog.c:64-66 (slot = layout - 1), view.c:358-364 (removes layout.h:7) */
```

Interface, champion panel, pointer:
```c
int16_t tables_icon_sheet_first(int sheet);             /* KAOS_ICON_GRAPHIC_FIRST_INDEX 0x100b92 [7]: ui.c:225-226 */
int16_t tables_slot_box_zone(int slot_box);             /* KAOS_SLOT_BOXES 0x100bb0 [46] x 4: ui.c:248 */
int16_t tables_slot_box_border(void);                   /* KAOS_SLOT_BOX_BORDER 0x102956: ui.c:239 */
int16_t tables_champion_icon_width(void); int16_t tables_champion_icon_height(void); /* KAOS_CHAMPION_ICON_WIDTH 0x10295c, _HEIGHT 0x10295e: ui_champion.c:175 */
int16_t tables_bar_shadow(void);                        /* KAOS_BAR_SHADOW_OFFSET 0x102980: ui_champion.c:347-348 */
int16_t tables_damage_splat_width(int large); int16_t tables_damage_splat_height(int large);
                                                        /* KAOS_DAMAGE_SPLAT_SMALL 0x10297c, KAOS_DAMAGE_SPLAT_LARGE 0x102978: ui_menus.c:168-169 */
int16_t tables_skill_level_text(int level);             /* KAOS_SKILL_LEVEL_STRING_INDICES 0x101e74 [15], level 2-16: startend.c:266, ui_champion.c:401 */
int16_t tables_base_skill_text(int skill);              /* KAOS_BASE_SKILL_STRING_INDICES 0x102472 [4]: startend.c:267, ui_champion.c:419 */
int16_t tables_statistic_text(int statistic);           /* KAOS_STATISTIC_STRING_INDICES 0x101e9a [7]: ui_champion.c:426 */
int16_t tables_direction_text(int direction);           /* KAOS_DIRECTION_STRING_INDICES 0x101e92 [4]: ui_champion.c:526 */
enum { TABLES_TEXT_LIST_SEPARATOR, TABLES_TEXT_BONES_SEPARATOR, TABLES_TEXT_ATTRIBUTES_PREFIX, TABLES_TEXT_ATTRIBUTES_SUFFIX, TABLES_TEXT_VERSION };
const char *tables_text(int text);                      /* raw pointer to a C string in the image: KAOS_LIST_SEPARATOR 0x101ebe,
   KAOS_BONES_SEPARATOR 0x101ec6, KAOS_ATTRIBUTES_PREFIX 0x101eca, KAOS_ATTRIBUTES_SUFFIX 0x101ec8, KAOS_DIALOG_VERSION_TEXT 0x10402c:
   ui_champion.c:483, 488, 491, 502; dialog.c:124 */
int16_t tables_message_line_pad(void);                  /* KAOS_DIALOG_MESSAGE_LINE_PAD 0x102966: dialog.c:90 */
uint8_t tables_message_color(int i);                    /* KAOS_DIALOG_MESSAGE_COLORS 0x104026 int16 [2] (low byte): dialog.c:101 */
int16_t tables_button_border(int axis);                 /* KAOS_ENDGAME_BUTTON_BORDER 0x102988 [x,y]: startend.c:249 (DM only) */
int16_t tables_button_inner_border(int axis);           /* KAOS_ENDGAME_BUTTON_INNER_BORDER 0x10298c: startend.c:247 (DM only) */
TablesRect tables_csb_restart_box(int inner);           /* CSB_RESTART_BOXES 0x104248 (+8 inner), x,y,w,h: startend.c:303, 310 */
/* pointer */
typedef struct { int16_t x, y, w, h, shape, champion; } PointerZone;
PointerZone tables_pointer_zone(int i);                 /* KAOS_MOUSE_POINTER_ZONES 0x1004f8 [16] x 12: gameloop.c:514-519 */
int16_t tables_pointer_hotspot_x(int shape); int16_t tables_pointer_hotspot_y(int shape); /* KAOS_MOUSE_POINTER_HOTSPOTS 0x1005c8 [4] x 8: gameloop.c:536-537 */
int16_t tables_pointer_shadow_dx(void); int16_t tables_pointer_shadow_dy(void); /* KAOS_POINTER_SHADOW_DX 0x102952, _DY 0x102954: gameloop.c:491 */
const uint8_t *tables_pointer_planes(int hand, int *w, int *h); /* KAOS_ARROW_POINTER_BITMAP 0x1002b0, KAOS_HAND_POINTER_BITMAP 0x1003d4;
   w, h from the words at -4, -2; raw pointer to the 4 bit planes: gameloop.c:469-479, 535 */
```

Input tables (address-carrying code goes away):
```c
/* Mouse input tables: the UI_INPUT_* table ids (ui/ui.h) move to tables.h, or tables.c keeps the 14 addresses in
   the same order as UI_INPUT_* and takes the id as int. Dialog tables (id >= UI_INPUT_DIALOG_FIRST) go through
   KAOS_DIALOG_MOUSE_INPUTS 0x103bd4 (u32 pointers). */
typedef struct { int16_t command, x1, x2, y1, y2; uint16_t buttons; } MouseInput;
int tables_mouse_input(int table, int entry, MouseInput *out); /* 0 at the terminator (command 0), else 1.
   KAOS_MOUSE_INPUT_INTERFACE 0x1031f8 ... KAOS_MOUSE_INPUT_ENDGAME 0x1031d4 (14 tables, ui.c:23-36):
   ui.c:298-320, dialog.c:144-146, platform/control.c:169-174. Removes ui_mouse_table_address (ui.h:118). */
void tables_set_mouse_input_box(int table, int entry, int16_t x1, int16_t x2, int16_t y1, int16_t y2);
   /* WRITE: startend.c:235-237 (endgame table). Writes the image big-endian, as today. */
enum { TABLES_KEYS_NONE = -1, TABLES_KEYS_ENTRANCE, TABLES_KEYS_INTERFACE, TABLES_KEYS_MOVEMENT, TABLES_KEYS_SLEEPING, TABLES_KEYS_FROZEN };
int16_t tables_key_command(int key_table, uint16_t key); /* KAOS_KEYS_ENTRANCE 0x10393c, _INTERFACE 0x103948, _MOVEMENT 0x103964,
   KAOS_KEYBOARD_INPUT_PARTY_SLEEPING 0x103980, KAOS_KEYBOARD_INPUT_FROZEN_GAME 0x10398c: command.c:140-147;
   primary_keys/secondary_keys change from uint32_t address to the int id (command.c:84, 113-114, 269, 668, 690; NO_TABLE/0 checks). */
```

Not KAOS — ui/logo.c (SWSH.FTL): R2's "no 0x10xxxx outside data/" also hits logo.c:14-20. Open decision: (a) a small data module (e.g. `data/swsh.c`: `int swsh_load(path, SwshLogo *out)` returning palette script, graphic pointer, sample, period) with `SWSH_` names, or (b) exempt logo.c from the check. ui/anim.c:35/40/285 has ANIM.FTL addresses in comments only: reword or exempt.

Rename also touches docs (c-decomp-port rule): `docs/csb-engine-diff.md` (lines ~105-182 name VIEWPORT_ORIGIN, MESSAGE_LINE_PAD, KEYS_*, WALL_ORN_DEPTH, OBJECT_SCALES, BUTTON_BORDER, LIST_SEPARATOR ...), `docs/csb-port-integration.md:79`, `docs/csb-data.md`, `docs/tables.md`.

## 4. Caching and adventure switch

- `tables_init` (data/tables.c:157-168) copies 18 tables into static arrays and sets `ready`; accessors return NULL before it (GET macro :170). Callers: `ui/gameloop.c:82` (gameloop_load_data, right after `kaos_load` at :78), lazily `ui/view.c:1590` (`if (!tables_creature_info(0)) tables_init();`), and tests (test_tables, test_ui, test_champion, test_move, test_creatures, test_automap).
- Adventure switch in the game: `platform/main.c` start_adventure (:396-401) / start_csb (:371-374) -> load_game_files (:337-342) -> `gameloop_load_data` -> `kaos_load` (which builds the CSB KAOS-layout image, data/ftl.c:224-235, csb_image_build + csb_text_build) and then `tables_init`. So in the program the cache is rebuilt on every adventure load; `tables_init` reads `g_adventure` for the spell table (tables.c:111).
- But `kaos_load` alone does not re-run `tables_init`. Tests that call only `kaos_load`: test_recruit.py:53, test_dungeon.py:43, test_view.py:160, test_sound.py:41, test_spells.py:46, test_port_data.py:102, test_game.py:88, test_csb_engine.py:71 (switches g_adventure and reloads). With a cache, these tests see NULL (not ready) or a stale DM cache after a CSB reload.
- The image is also written at run time: `ui/startend.c:237` (endgame mouse boxes), and csbtext.c:151-153 writes the string index tables at load. A cache of the mouse tables would miss the endgame write unless `tables_set_mouse_input_box` writes the cache too.
- Safe choice (behavior identical): all NEW accessors read the image at call time through kaos_u8/u16/s16/u32/ptr inside tables.c (same reads, same abort on an address outside the image). Keep the existing 18 caches as they are.
- Replacing raw reads with EXISTING cached accessors (map.c:261, map.c:44, view.c:1549, timeline_process.c:334, view.c:1629) changes behavior where tables_init has not run: NULL dereference. In the program it is safe (gameloop_load_data runs tables_init). Before using the cache there, check the tests that reach these paths without tables_init (test_dungeon, test_game, test_view, test_recruit, test_port_data, test_csb_engine) and add `tables_init()` to their fixtures, or give these accessors image-time reads. view.c:1629 also needs the index < 6 bound to stay as today (raw read past the end vs NULL).

## 5. csbimage.c / csbtext.c KAOS addresses that have a name elsewhere

data/csbimage.c `csb_copies[]` (KAOS column, line -> other name):
- :28 0x100170 -> BOTTOM_PALETTE ui/ui.c:19
- :29 0x100190 -> SPRITE_PALETTE ui/gameloop.c:39
- :30 0x1002ac -> ARROW_POINTER - 4 (ui/gameloop.c:40, 0x1002b0)
- :31 0x1003d0 -> HAND_POINTER - 4 (ui/gameloop.c:41, 0x1003d4)
- :32 0x1004f8 -> POINTER_ZONES ui/gameloop.c:42
- :33 0x1005c8 -> POINTER_HOTSPOTS ui/gameloop.c:43
- :34 0x1005fe -> POINTER_SHADOW_OBJECT ui/gameloop.c:44
- :35 0x100696 -> G_SOUND_VOLUME_TABLE engine/sound.c:12
- :36 0x100a46 -> CREDITS_PALETTE ui/startend.c:32
- :37 0x100a66 -> ENTRANCE_PALETTE ui/gameloop.c:38
- :38 0x100aa6 -> DUNGEON_PALETTE ui/view.h:8
- :39 0x100b66 -> KAOS_ORDERED_CELLS_TO_ATTACK engine/group.c:16
- :40 0x100b86 -> KAOS_WOUND_PROBABILITY_INDEX_TO_MASK engine/group.c:17
- :41 0x100b8a -> KAOS_SLOTS_TO_STEAL_FROM engine/group.c:18
- :42 0x100b92 -> ICON_FIRST ui/ui.c:20
- :43 0x100ba0 -> TORCH_CHARGE_TO_ICON_OFFSET engine/inventory.c:13
- :44 0x100bb0 -> SLOT_BOXES ui/ui.c:21
- :45 0x100c68 -> SLOT_MASKS data/tables.c:24
- :46 0x100cb4 -> LIGHT_POWER_TO_LIGHT_AMOUNT data/tables.c:22; KAOS_LIGHT_POWER_TO_LIGHT_AMOUNT engine/timeline_process.c:18
- :47 0x100cd4 -> PALETTE_LIGHT_THRESHOLDS data/tables.c:23; LIGHT_THRESHOLDS ui/view.c:41
- :48 0x100ce0 -> CHAMPION_COLORS engine/champion.h:14
- :49 0x100ce4 -> WOUND_DEFENSE_FACTOR data/tables.c:25
- :50 0x100d0e -> SLOT_DROP_ORDER engine/champion.c:21
- :51 0x100d4a -> KAOS_SQUARE_TYPE_TO_EVENT_TYPE engine/sensor.c:15
- :52 0x100d52 -> G_SOUNDS engine/sound.c:11
- :53 0x101038 -> DIRECTION_TO_STEP_EAST engine/game.c:14; STEP_EAST ui/view.c:23
- :54 0x101040 -> DIRECTION_TO_STEP_NORTH engine/game.c:15; STEP_NORTH ui/view.c:24
- :55 0x101058 -> NEW_GAME_EXTRA_THINGS engine/dungeon.c:19
- :56 0x101068 -> OBJECT_INFO data/tables.c:9
- :57 0x1014a0 -> WEAPON_INFO data/tables.c:10
- :58 0x1015b4 -> ARMOUR_INFO data/tables.c:11
- :59 0x10169c -> JUNK_WEIGHTS data/tables.c:12
- :60 0x1016d2 -> FOOD_VALUES data/tables.c:13
- :61 0x1016e2 -> DOOR_INFO data/tables.h:9; KAOS_DOOR_INFO engine/map.c:13
- :62 0x1016ea -> MESSAGE_ESCAPES engine/dungeon.c:20
- :63 0x1017ea -> REPLACEMENT_CHARS engine/dungeon.c:21
- :64 0x10182a -> INSCRIPTION_ESCAPES engine/dungeon.c:22
- :65 0x10192a -> GROUP_DIRS ui/view.c:42
- :66 0x10192e -> CREATURE_INFO data/tables.c:7; KAOS_CREATURE_INFO engine/map.c:14
- :67 0x101bec -> CREATURE_SOUNDS data/tables.c:8
- :68 0x101c10 -> FIXED_POSSESSIONS data/tables.c:26
- :69 0x101ebe -> LIST_SEPARATOR ui/ui_champion.c:30
- :70 0x101ec6 -> BONES_SEPARATOR ui/ui_champion.c:31
- :71 0x101ec8 -> ATTRIBUTES_SUFFIX ui/ui_champion.c:32
- :72 0x101eca -> ATTRIBUTES_PREFIX ui/ui_champion.c:33
- :73 0x10247a -> INVISIBILITY_PALETTE ui/ui_champion.c:25
- :74 0x1025f2 -> LAYOUT_BITMAPS ui/layout.h:7
- :75 0x10293a -> VIEWPORT_ORIGIN ui/view.h:9
- :76 0x102948 -> VIEWPORT_SIZE ui/view.h:10
- :77 0x102952 -> POINTER_SHADOW_OFFSET ui/gameloop.c:45
- :78 0x102956 -> SLOT_BOX_BORDER ui/ui.c:22
- :79 0x102958 -> PORTRAIT_SIZE ui/view.h:11; G_PORTRAIT_WIDTH engine/recruit.c:7 (G_PORTRAIT_HEIGHT 0x10295a recruit.c:8 inside it)
- :80 0x10295c -> CHAMPION_ICON_SIZE ui/ui_champion.c:27
- :81 0x102966 -> MESSAGE_LINE_PAD ui/dialog.c:16
- :82 0x10296e -> GLYPH_SIZE ui/view.c:25
- :83 0x102978 -> DAMAGE_SIZE_LARGE ui/ui_menus.c:20
- :84 0x10297c -> DAMAGE_SIZE_SMALL ui/ui_menus.c:21
- :85 0x102980 -> BAR_SHADOW ui/ui_champion.c:28
- :86 0x102a22 -> WALL_SET_IDS ui/view.c:26; :87 0x102a60 = WALL_SET_IDS + 62 (no name)
- :88-94 0x102a7e..0x102b0e -> VS_LANE, VS_DEPTH, VS_OBJECT, VS_ALCOVE, VS_CREATURE, VS_EXPLOSION, VS_FIELD ui/view.c:43-49
- :95 0x102b26 -> FIELD_ASPECTS ui/view.c:50
- :96 0x102ba6 -> WALL_ORN_DEPTH ui/view.c:27
- :97 0x102bb4 -> FLOOR_ORN_VIEW ui/view.c:28
- :98 0x102bc0 -> ALCOVES ui/view.c:29
- :99 0x102bc3 -> FOUNTAINS ui/view.c:30
- :100 0x102bc4 -> WALL_ORN_COORDS ui/view.c:32
- :101 0x102c00 -> FLOOR_ORN_COORDS ui/view.c:33
- :102 0x102c0a -> DOOR_ORN_COORDS ui/view.c:34
- :103 0x102c16 -> DOOR_BUTTON_COORDS ui/view.c:35
- :104 0x102c18 (64 bytes) -> PAL_D3 ui/view.c:36 (covers PAL_D2 :37, PAL_DOOR_D3 :38, PAL_DOOR_D2 :39)
- :105 0x102c58 -> TEXT_HEIGHTS + 1 (ui/view.c:40 is 0x102c57)
- :106 0x102c68 -> OBJECT_ASPECTS ui/view.c:51
- :107 0x102db8 -> PROJECTILE_ASPECTS ui/view.c:52
- :108 0x102df4 (48) -> PAL_SMOKE ui/view.c:53 (covers PAL_OBJECT_D3 :54, PAL_OBJECT_D2 :55)
- :109 0x102e24 -> OBJECT_SCALES ui/view.c:56
- :110 0x102e2a -> PROJECTILE_SCALES ui/view.c:57
- :111 0x102e32 -> EXPLOSION_SCALES ui/view.c:58
- :112 0x102e38 -> PILE_SHIFTS ui/view.c:59
- :113 0x102e58 -> CREATURE_ASPECTS ui/view.c:60
- :114 0x102efa -> REPLACEMENT_COLORS ui/view.c:61
- :115 0x102fb0 (56) -> PAL_CREATURE_D3 ui/view.c:63 (covers PAL_CREATURE_D2 :64, SHIFT_SETS :65)
- :116 0x102fe8 -> REBIRTH_SCALES ui/view.c:68; :117 0x102ff0 = REBIRTH_SCALES + REBIRTH_STEP_1_SCALES (view.c:69)
- :118 0x10318c -> MOUSE_INPUT_ENTRANCE ui/ui.c:35
- :119 0x1031d4 -> MOUSE_INPUT_ENDGAME ui/ui.c:36
- :120-131 0x1031f8..0x10378c -> MOUSE_INPUT_INTERFACE .. MOUSE_INPUT_NAME_ENTRY ui/ui.c:23-34
- :132 0x10393c -> KEYS_ENTRANCE ui/command.c:29
- :133 0x103948 -> KEYS_INTERFACE ui/command.c:27
- :134 0x103964 -> KEYS_MOVEMENT ui/command.c:28
- :135 0x103980 -> KEYS_SLEEPING ui/command.c:30
- :136 0x10398c -> KEYS_FROZEN ui/command.c:31
- :137 KAOS_DIALOG_TABLES 0x103a84 -> no other name (reached via the pointers at 0x103bd4)
- :138 0x103c56 -> KAOS_MOVEMENT_ARROW_TO_STEP_FORWARD engine/move.c:15
- :139 0x103c5e -> KAOS_MOVEMENT_ARROW_TO_STEP_RIGHT engine/move.c:16
- :140 0x103c66 -> G_SYMBOL_BASE_MANA_COST data/spells.h:6
- :141 0x103c7e -> G_SYMBOL_MANA_COST_MULTIPLIER data/spells.h:7
- :142 0x103eac (264) -> ACTION_DISABLED_TICKS data/tables.c:15 (covers ACTION_DAMAGE_FACTOR .. ACTION_SKILL_INDEX :16-20)
- :143 0x103fb4 -> ACTION_EXPERIENCE_GAIN data/tables.c:21
- :144 0x103ffa -> ACTION_ICON_PALETTE ui/ui_menus.c:19
- :145 0x104026 -> MESSAGE_COLORS ui/dialog.c:15
- :146 0x10402c -> VERSION_TEXT ui/dialog.c:14
- :147 CSB_SPELLS, :148 CSB_RESTART_BOXES -> data/csbimage.h:8, :10 (used in tables.c:114, ui/startend.c:310)
- defines csbimage.c:10 KAOS_ACTION_SETS 0x103d4c -> ACTION_SETS data/tables.c:14; csbimage.c:12 KAOS_DIALOG_MOUSE_INPUTS 0x103bd4 -> DIALOG_MOUSE_INPUTS ui/ui.c:42

Used KAOS addresses NOT in the copy table: BUTTON_BORDER 0x102988, BUTTON_INNER_BORDER 0x10298c (DM-only path, startend.c:247-249), G_SPELLS 0x103c84 (DM only), and the four string index tables below (written by csbtext.c).

data/csbtext.c (`table_texts[].kaos_indices`, the only KAOS addresses there; all others are APPB addresses):
- :55 0x102472 -> BASE_SKILL_STRINGS ui/ui.h:16
- :56 0x101e74 -> SKILL_LEVEL_STRINGS ui/ui.h:15
- :57 0x101e92 -> DIRECTION_STRINGS ui/ui_champion.c:29
- :58 0x101e9a -> STATISTIC_STRINGS ui/ui_champion.c:26
