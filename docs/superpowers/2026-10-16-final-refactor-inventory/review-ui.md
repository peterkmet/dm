# Review of `port/src/ui/` for the final refactor

Scope: every `.c` and `.h` in `port/src/ui/` (about 9,500 lines without the vendored `dr_mp3.h`), read in full, plus the callers in `engine/`, `platform/`, `tests/` and `tools/`. Read-only; nothing was built or run. Line numbers are of the tree at commit `0771406`.

Size: S under an hour, M a few hours, L a day or more.

Verified facts that shape the findings:

- The include path is flat: `port/Makefile:6` adds `-Isrc/data -Isrc/engine -Isrc/ui -Isrc/platform`; a `#include "video.h"` does not show its layer.
- No `SDL_` symbol and no platform header in `ui/` (grep clean); `test_layers` holds.
- `tests/standard_allowlist.txt` has no `ui/` line except the two soundtrack tables (`decimal ui/musicmap.c`, `decimal ui/music.c`); the engine-write check has no open ui line.
- `music_mix` runs on the game thread (`platform/platform.c:240-253` pumps the SDL stream); no lock is needed.
- `g_frame_hook` is `run_frame_hook` (`platform/main.c:293-299`): read input, pointer, present, sleep. It never runs game logic, so the nested waits inside `gameloop_tick` are safe. Turn-based mode sets the hook to NULL (`main.c:451`).
- `junk_charges` returns 2 bits (`map.c:333-336`), so `g_champions[junk_charges(thing)]` (`ui.c:297`, `ui_champion.c:575`) stays in range.

## 1. Module boundaries and cohesion

1. **`ui.h` is three modules in one header.** `ui.h:3` states `ui_ panel_ menus_`; sections at `ui.h:71`, `:109`, `:138` map to `ui.c`, `ui_champion.c`, `ui_menus.c`. The split does not follow the prefixes: `ui_draw_champion_*`, `ui_draw_inventory_*`, `ui_draw_panel_*` (`ui.h:110-134`) are in `ui_champion.c` with `ui_`, next to `panel_*` for the same panel; `ui_draw_icon_to_viewport` (`ui_champion.c:646`) and `ui_draw_icon_in_slot_box` (`ui.c:260`) are twins in two files. A reader cannot go from a name to its file. Why: the header is 154 lines and the first thing every ui reader opens. Proposal: three headers, one prefix each: `ui.h` (`ui.c`: zones, blits, icons, palette, hit tests), `party.h` (`party.c`, was `ui_champion.c`, prefix `party_`: status boxes, icons, inventory, panel; `champion_` is taken by the engine), `menus.h` (`menus.c`, was `ui_menus.c`, prefix `menus_`). M.

2. **Flat include namespace and `ui_` file names.** `port/Makefile:6` puts the four layers on one include path; files are named `ui_champion.c`, `ui_menus.c` inside `ui/` to tell them from the engine's `champion.c`, and `text.h`, `draw.h`, `video.h`, `select.h`, `file.h` carry generic guards (`TEXT_H`, `SELECT_H`, `FILE_H`; no clash today, verified). Why: the directory structure is the main readability tool the owner asked for, and the include line is where a reader learns which layer a module is in. Proposal: `-Isrc` only; `#include "ui/video.h"`, `"engine/game.h"`, `"data/ids.h"`; guards `DM_UI_VIDEO_H`; `test_layers` reads the layer from the path; then `ui_champion.c` can become `party.c` (finding 1). M (mechanical, every file).

3. **The entrance is split between `gameloop.h` and `command.h`.** `entrance_draw/press_button/open_doors` are in `gameloop.h:15-20` (`gameloop.c:68-171`, about 100 lines, second prefix `entrance_`), while the entrance result `g_new_game` and `ENTRANCE_RESUME/NEW_GAME/WAITING` are in `command.h:79-86`, set at `command.c:766-771`, consumed by `platform/main.c` (6 uses). Proposal: `entrance.c/h` with the three functions, the enum and `g_entrance_choice` (renamed from `g_new_game`, which is not a bool and not "new game"); `command.c` keeps writing it. S.

4. **The frame-wait primitive lives at the top of the layer and inverts the dependencies.** `g_frame_hook`, `wait_frame`, `wait_frames`, `g_waiting_frame`, `g_quit_requested` are in `startend.h:11-26` (`startend.c:68-96`), the module of the endgame sequences. `dialog.c:7`, `logo.c:8`, `anim.c:13`, `command.c:7`, `gameloop.c:16` include `startend.h` for them, so the lowest modules depend on the highest. Proposal: `frame.h` (prefix `frame_`: `frame_wait`, `frame_wait_count`, `g_frame_hook`, `g_frame_wait_depth` for `g_waiting_frame`, `g_quit_requested`) at the bottom of ui; `startend_` keeps `startend_` only. Document the hook contract at the declaration: "the hook shows a frame and reads input; it runs no game logic" (that is what keeps the nested waits in `gameloop_tick` → `startend_run_endgame` (`gameloop.c:425`) and `command_process_queue` → `startend_save_game` (`command.c:774`) safe). S.

5. **`KEY_*` words are split over two headers.** `command.h:55-65` (`KEY_RETURN … KEY_F1`) and `advanced.h:14-25` (`KEY_TAB … KEY_3`); `platform/main.c:5,23` includes both to map SDL keys; `advanced.c:142` mixes `KEY_ESCAPE` (command.h) with `KEY_M` (its own). Proposal: one key table in `command.h` (or `keys.h`), `advanced.h` keeps none. S.

6. **Game rules still in `ui/` (R8 of the clean-code pass is not finished).** Evidence:
   - `command.c:409-428` `grab_object`: the creature-protects-the-pile check (`group_get_creature_ordinal_in_cell`), then `move_get_move_result` + `inventory_put_object_in_leader_hand` composed in ui.
   - `command.c:432-449` `drop_object`: `inventory_get_object_removed_from_leader_hand`, `THING_WITH_CELL` packing at the call site, `move_get_move_result`, `sensor_add_vi_altar_rebirth_event`.
   - `command.c:542-544` the imaginary-wall predicate from square bits.
   - `command.c:794-802` `is_movement_blocked`: three engine globals and direction arithmetic.
   - `startend.c:431-443` `remove_fluxcages` (thing-list surgery with `map_unlink_thing_from_list`, `thing_set_free`), `:495-501` `delete_other_groups`, `:514-519` the lord setup (`group_set_health/cells/directions`), `:525,532` creature type changes.
   - `ui_champion.c:255-258` the heavy-load threshold computed from `HEAVY_LOAD_NUMERATOR/DENOMINATOR` (engine owns the rule).
   - `automap.c:24-39, 56-81` "what the party can see" (`blocks_sight`, `automap_get_visible_depths`, `automap_side_is_visible`): a world query, also used by `platform/state_json.c` for the LLM, with the result stored by `engine/explored.c`.
   - `view.c:1421-1435` `get_location_after_level_change` re-implements `map.c:86-102` without its party-map guard.
   Why: the owner's rule is "ui translates a click into one engine call and draws from flags"; each of these is a rule a differential test cannot reach through the engine. Proposal: engine functions `inventory_grab_object(cell, x, y)`, `inventory_drop_object(cell, x, y, into_alcove)`, `map_is_imaginary_wall(x, y)`, `move_is_blocked(command)`, `game_prepare_fuse_lord(lord, x, y)` + `map_remove_fluxcages(x, y)` + `group_delete_all_except(x, y)`, `champion_get_load_level(c)`, `explored_mark_visible(map, x, y, dir)` (the drawing stays in `automap.c`), and a `map_get_location_after_level_change_on(map, …)` so the view does not copy it. M-L (golden snapshots exist for the pattern: `tests/test_golden_rules.py`).

7. **Modules in `ui/` that are not user interface.**
   - `music.c` decodes MP3 and mixes PCM; `musicmap.c` is 316 lines of table data; the vendored `dr_mp3.h` (5,380 lines, 37% of the directory) is included with `DR_MP3_IMPLEMENTATION` at `music.c:14` under a `-Weverything` pragma (`music.c:10-18`) and excluded from the standard by name (`tests/test_standard.py:12`).
   - `video.c:30-53` `video_layout`, `video_map_mouse` (window scale, letterbox, floats) and `video.h:11-26` `video_convert_argb`, `video_argb_*` are output mapping, called only from `platform/` (callers table: `platform.c`, `control.c`).
   - `anim.c:17-80, 141-175` parse the chunk format and the MI music format (a data format), then play it.
   Proposal: `port/vendor/dr_mp3/dr_mp3.h` with `-Ivendor` and a `README` (version, licence), the VENDORED set becomes the directory; move `video_layout/map_mouse/convert_argb/argb_*` to `platform/` (`scale.c` or a new `output.c`); keep `music.c` and `anim.c` in ui (they present sound and pictures) and say so in the ui module comment; optionally move the chunk/MI parsing to `data/anim_format.c`. S for the vendor and video moves, M for anim.

8. **Constants owned by the wrong module (R3 leftovers).**
   - `view.h:9` `DOOR_NORTH_SOUTH = 8` "that map.h does not name"; `map.h:23` names its twin `STAIRS_NORTH_SOUTH`. Square bits belong in `map.h`; `automap.c:137` uses it too.
   - `startend.c:66` `SOUND_FUSE = 14`; `sound.h:20` `SOUND_TELEPORT = 14` and `sound.h:34` `SOUND_BUZZ = 14`: three names for one sound, one of them in ui.
   - `startend.c:43` `DECODE_MESSAGE_INVISIBLE ((int16_t)0x8001)` is `TEXT_MODE_MESSAGE | TEXT_MODE_ANY_VISIBILITY` of `dungeon.h:38-40`; `ui_champion.c:777` already composes the scroll variant that way.
   - `view.c:21` `ORNAMENT_VI_ALTAR 2`: a game-data id; the fountain has `tables_get_fountain_ornament()` (`view.c:2137`).
   - `view.c:2242` `PRESSURE_PAD = 1, ROUND_PRESSURE_PAD = 4, TINY_PRESSURE_PAD = 7`: floor-ornament graphic ids inside a function; `data/ids.h` owns graphic ids.
   - `ui_champion.c:32,35` `SCROLL_FONT_SHIFT 0x60`, `RUNE_FIRST 0x60` and `ui.h:103` "font character - 0x60" repeat `SPELL_SYMBOL_FIRST 0x60` (`data/spells.h:13`).
   - Slot-box numbering is spread over four files: `ui.c:17-18`, `ui.h:58`, `ui_champion.c:44-45`, `command.c:33`.
   - Amiga 12-bit colour packing is defined twice: `video.c:6-8` and `anim.c:28-30`.
   Proposal: move each to its owner; `video_amiga_color(r, g, b)` in `video.h`; one slot-box block in `ui.h`. S.

9. **ui reads engine internals raw.** `view.c:2185-2186` `g_dungeon_map_data[m][map_width - 1] + map_height` walks past the squares to the creature-type and ornament lists (record layout outside the accessor module; the check only catches `g_current_map[`). `view.c:674-677` indexes `g_active_groups` with `g_max_active_group_count`; `view.c:894` and `:2338` index `g_timeline_events[rec_projectile_event_index(thing)]`. Proposal: `dungeon_map_creature_types(m)`, `dungeon_map_ornaments(m)`, `group_get_active(thing)` (a similar inline exists at `group.h:146`), `projectile_get_flight_direction(thing)`. S.

10. **`g_game_screen_inactive` is not dialog state.** Declared in `dialog.h:12`, written by `startend.c:203,402,549` and `gameloop.c:244,294`, read by `advanced.c:38`; a negative name. Proposal: `g_game_screen_shown` in `gameloop.h` (or the new `entrance.h`). S.

11. **Game-loop tick state is declared in `command.h`.** `g_stop_waiting_for_input`, `g_game_time_ticking`, `g_wait_for_input_max_frames`, `g_game_frozen` (`command.h:82-85`) are read and written by `gameloop.c:340-347, 394-395`; `g_game_frozen` and `g_game_time_ticking` are two flags for one mode, set together at `command.c:709-725` but `command_unfreeze_game` keeps `g_game_frozen` while the party sleeps (`command.c:239-242`). Proposal: move the four to `gameloop.h` and document the sleep case in one line. S.

## 2. Naming

12. **Globals without `g_`.** `fb`, `palette`, `palette_split_top`, `palette_split_bottom` (`video.h:28-33`) are the most used globals of the layer (13 ui files, platform, 13 + 5 test bindings). Rule 7 says `g_`. Proposal: one `Screen` struct `g_screen = {px[SCREEN_H][SCREEN_W], palette[2 * PALETTE_COLORS], split_top, split_bottom}` with `video_get_color` and the palette split as its methods; the ctypes tests rebind by offset. M. The minimal fix (`g_fb`, `g_palette`, `g_palette_split_*`) is S.

13. **Names that say the wrong thing.** `ui_draw_champion_portrait` returns a `Bitmap` and draws nothing (`ui_champion.c:204-207`); `panel_draw_object_text` and `panel_draw_object_meter` format and return values (`ui_champion.c:566, 864`; both used by `platform/control.c` for the LLM); `g_new_game` holds 0/1/99 (`command.c:45`); `draw_center` computes a point (`automap.c:170`); `largest_attack` beside `get_largest_defense` and `get_largest_weight` (`ui_champion.c:812-838`); `dragged_icon_cell_plus_1` encodes an ordinal in its name (`command.c:64`); `Group` in `view.c:657` shadows the engine concept; `command_toggle_inventory(4)` and `(5)` are sentinels `INVENTORY_CLOSE`/`INVENTORY_REDRAW` (`command.c:29-30`), and `startend.c:508` passes `CHAMPION_MAX_COUNT` as the close sentinel. Proposal: `ui_get_champion_portrait`, `panel_format_object_text`, `panel_get_object_meter`, `g_entrance_choice`, `get_map_center`, `get_largest_attack`, `dragged_icon_cell` with -1, `ViewGroup`, and `command_close_inventory()` / `command_redraw_inventory()` instead of sentinels. S.

14. **Five parallel enums for the same 16 squares.** `WS_WALL_D3R2` (`view.c:32-67`), `VIEW_SQUARE_D3R2` (`:122-142`), `VIEW_D3R2` (`:145`), `VIEW_WALL_D3R2_LEFT` (`:148-164`), `BUTTON_D3R` (`:166`), `STAIRS_FRONT_D3L` (`:83-96`). They are indexes into different game-data tables (wall-set layout ids, view-square table, floor-ornament views, wall-ornament views, door buttons), so they cannot merge, but nothing says so. Proposal: one comment block above them that names the table each enum indexes, and the `SquareView` fields that join them (`view.c:1481-1485`). S.

15. **`select.h` constants borrow other owners' prefixes.** `SCREEN_ADVENTURE/MODE/CSB/DONE` (`select.h:11`) next to `SCREEN_W/H` (`video.h:6-7`) and `ZONE_SCREEN`; `CSB_NEW_ADVENTURE/RESUME/QUIT` (`select.h:12`) while rule 1 reserves `CSB_` for the APPB image (`CSB_MINI_FILE`, `CSB_PRISON_DUNGEON_ID`). Proposal: `SELECT_SCREEN_*`, `SELECT_CSB_*`. S.

16. **Comments that only name an original symbol or number** (R7.3 asked for game terms): "MENU.C state" (`ui.h:64`), "draw_viewport(0): the viewport bitmap to zone 7" (`ui.h:85`), "blit_bitmap on chunky pixels" (`draw.c:6`), "blit_box_filled_with_masked_bitmap" (`draw.c:85`), "as flip_bitmap_horizontal does" (`draw.c:89`), "draw_wall_set_bitmap and its _opaque and _flipped variants" (`view.c:418`), "(g_drawn_box)" (`view.c:445`), "get_scaled_flipped_bitmap" (`view.c:1214`), "set_current_map_and_party_map and load_current_map_graphics" (`view.c:2178`), "load_floor_set and load_wall_set" (`view.c:2062`), "(build_mouse_pointer_bitmap)" (`gameloop.c:454`), "(text_print_to_bitmap_zone)" (`text.c:73`), "as get_zone does" (`layout.c:153, 166`), "(draw_dungeon_view)" (`gameloop.h:27`), "Command 140" (`startend.h:28`), "Sensor 18" (`startend.h:45`), "KAOS range 8" (`command.h:8`). Proposal: rewrite in game terms; `docs/kaos-symbols.csv` keeps the mapping. S.

## 3. Types and API shape

17. **About 35 truth values are still `int` or `int16_t`** (R6.3 incomplete; `docs/todo.md:20` lists one). Parameters: `int pad` (`ui_champion.c:67`), `int inv` (`:269, :294`), `int16_t pressing_eye` (`:895, :912`, stored into a `bool` at `:916`), `int border` (`ui.c:237, 253`), `int mask_flip` (`draw.h:17`), `int resume` (`gameloop.h:18`), `int confirm` (`gameloop.c:286`), `int16_t imaginary_wall` (`command.c:467, 502`), `int alcove_front, int alcove` (`view.c:620`), `int drawn` (`:856`), `int allowed` (`:307, :2317`), `int blink` (`:2365`), `int16_t won` (`music.h:28`), `int16_t party_dead` (`startend.h:42`). Returns: `ui_zone_box` (`ui.c:52`), `ui_get_icon_zone_box` (`:237`), `layout_get_zone_box` (`layout.h:12`), `gameloop_run_frame` (`gameloop.h:31`, "Returns 1"), `draw_entrance_doors` (`gameloop.c:70`), `draw_wall_ornament` (`view.c:1263`). Locals: `palette_changed`, `input_stops` (`anim.c:65`), `running` (`:322`), `thrown` (`command.c:490`, takes a `bool`), `imaginary_wall` (`:543`), `shown`, `ran` (`gameloop.c:76, 337, 356`), `int16_t loaded = startend_load_saved_game()` (`startend.c:373`, a `bool` into `int16_t`), `use_flipped_walls` (`view.c:244`), `fluxcage`, `rebirth`, `mirrored`, `side` (`:1064, :1072, :1271, :1273`). Proposal: `bool` for all; one review pass, no new check. S-M.

18. **`int` vs `int16_t` for pixel coordinates.** `text.h:12-15` (`int x, y, w, h`), `dialog.c:135` `fill(int x, …)`, `select.h:30-34` (`int key, item, x, y`), `video.h:38` (`int y`) against `draw.h`, `ui.h`, `automap.h`, `view.h` (`int16_t`). Callers widen and narrow around `text_get_size` (`ui.c:116-121`, `startend.c:245-250`, `dialog.c:76-79`). Rule 3 says one value, one type on its path. Proposal: `int16_t` for every pixel coordinate and size in ui; `int` stays only for window pixels in `video_layout/map_mouse` (which move to platform, finding 7). S.

19. **Three return conventions for success.** 0/-1: `ui_init`, `text_init`, `layout_init`, `anim_open`, `anim_play`, `logo_play`, `draw_shrink`; `bool`: `gameloop_load_data`, `gameloop_start_new_game`, `view_render`, `select_restart`; `int` 0/1: `gameloop_run_frame`, `ui_zone_box`, `layout_get_zone_box`; codes with an out-parameter: `startend_save_current_game` (`startend.h:36`). `gameloop.c:60` mixes two in one expression: `gfx_open(path) >= 0 && !layout_init() && !text_init() && !ui_init()`. Proposal: a standard rule "two outcomes: `bool`; more: an enum"; convert the 0/-1 inits. S.

20. **Hidden parameters and results through file statics in `view.c`.** `zone_relative_x/y` (`view.c:245`; set at `:642-643, :753-754, :1309-1310, :1405-1406`; read at `:454-455`), `shift_x/y` (`:264`; set `:851-852, :858-859`; read `:461-462`), `drawn_box` (`:250`; set `:460`; read `:594-609, :863, :929-932, :1314, :1377`). `draw_bitmap_in_zone` has five visible parameters and five invisible ones; `draw_projectile` clears `drawn_box.w` to detect a draw (`:929-932`). This is the original's global-register style. Proposal: a `Placement {int16_t dx, dy, shift_x, shift_y;}` argument and a `Box *drawn` out-parameter (NULL when not wanted); `begin/end_projectile` take and return the placement. M.

21. **Wide parameter lists.** `draw_object` 10 (`view.c:619-620`), `draw_masked_fill` 10 (`draw.h:17-18`), `draw_creatures_in_cell` 9 (`view.c:765-766`), `draw_projectile` 8, `get_projectile_pose` 8, `gameloop_get_pointer` 8 (`gameloop.h:38`), `draw_map_square_things` 7. Proposal: a `SquareDraw {dir, x, y, view_square, depth, lane, coord, pass}` context passed by const pointer through the thing drawers; a `Pointer {px, w, h, hot_x, hot_y}` out-struct for the pointer. M.

22. **`Bitmap` views by value and three hand-made screen bitmaps.** `ui_get_screen()` (`ui.c:43-46`) returns `{SCREEN_W, SCREEN_H, &fb[0][0]}`; `text.c:57` and `view.c:2575` build the same literal because they sit below `ui.c`; about 20 functions open with `Bitmap screen = ui_get_screen();`. By-value views are fine (16 bytes) and should stay. Proposal: one `video_get_screen()` in `video.h`, delete the two literals and `ui_get_screen`. S.

23. **`Box` by value in `automap`** (`automap.h:26`, `automap.c:98-203` `Box area`) and by `const Box *` everywhere else (`draw.h`). S: pick the pointer form.

24. **`static inline` accessors narrow implicitly.** `video.h:15-26` return `uint8_t` from a `uint32_t` shift with no mask or cast; the truncation is the intent and rule 3 asks for a cast with a comment where a cast changes the result. S.

25. **Packing at a call site.** `view.c:828` builds `track_view` with `<< MAP_COORD_BITS` and `<< DIRECTION_BITS` of `map.h`; the packed-field check looks for `_SHIFT` names and 8/16/24 only. Proposal: `ViewPosition {map, x, y, dir}` and a field compare in `start_tracks`. S.

## 4. Global mutable state

26. **Counts.** Externs per header: `view.h` 10 (two of them arrays), `command.h` 8, `startend.h` 5, `music.h` 5 (one const table), `video.h` 4, `ui.h` 3 (+1 const table), `dialog.h` 2, `advanced.h` 2; 39 in all. File statics: `view.c` 20 plus two big arrays, `anim.c` 13, `command.c` 12, `select.c` 8, `music.c` 5, `gameloop.c` 4.
   - `view.h:21-27` are the outputs of the last `view_draw` (`g_square_ahead_element`, `g_facing_*`, `g_dungeon_view_clickable_boxes`, `g_pile_*`), read by `command.c` and `platform/state_json.c`. Proposal: `ViewResult` struct returned by `view_get_result()`; `g_do_not_draw_fluxcages` stays an input flag. M.
   - `music.h:16-18` `g_music_track`, `g_music_last_track`, `g_music_requested_track` have no reader outside `music.c` except the tests (callers table). Proposal: static, with `music_get_track()` for the tests. S.
   - `view.c:235-272` 20 statics: a `static struct View view;` names the module state as one object and lets `view_draw_magic_map` (`view.c:2502-2534`) save and restore `view_map/map_width/map_height` by copying one sub-struct instead of three fields twice. M.
   - Duplicated between ui and engine: none found that stores the same value twice; `shown_hand_object/shown_hand_icon` (`gameloop.c:46-47`) and `slot_box_icon[]` (`ui.c:30`) cache "what is drawn", which is legitimate ui state. `slot_box_icon` has no reader but `ui_get_icon_in_slot_box` (`ui.c:276`), which nobody calls (finding 40): the cache can go. S.

## 5. Comments

27. **Outdated or misplaced.** `ui.h:69` "call after kaos_load" (`kaos_load` is data-private since R2; the loader is `tables_load`); `ui.c:147-149` two comment blocks on `ui_set_screen_palette`, the first describes `ui_set_palette` (`ui.c:158`), which then has none; `ui.h:65-67` the portrait getter sits under "MENU.C state"; `view.c:481` a stray comment attached to nothing; `dialog.c:148` "the zone word of an entry with a negative x1" after `return;` explains nothing to a reader of this code; `gameloop.h:29-31` "Returns 1" for a truth value. S.

28. **Headers without a module comment.** `draw.h`, `text.h`, `layout.h`, `video.h`, `view.h` have only the prefix line (`:3` in each). The others have one paragraph. Proposal: one paragraph per header: what the module owns, what it must not do (for `view.h`: "draws from game state, changes none; its outputs are the click boxes and the facing flags"). S.

29. **Where the one-line comment lives is inconsistent.** `select`, `automap`, `music`, `anim`, `gameloop`, `startend`, `dialog` document in the header and leave definitions bare (`dialog_draw` `dialog.c:112`, `music_stop` `music.c:67`, `view_set_motion` `view.c:875`, `startend_run_endgame` `startend.c:393` has a deviation note instead of a description); `ui`, `text`, `layout`, `draw`, `command`, `view` document above the definition. Proposal: a rule: the game-terms comment is above the definition; the header repeats only what a caller must know (return codes, ownership, "exported for the tests"). S for the rule, M to apply.

30. **TODO markers in code.** "ponytail:" at `command.c:789`, `gameloop.c:119, 148`, `ui_champion.c:940`, `automap.c:46`, `view.c:1889` (and three in `engine/`). CLAUDE.md puts open work in `docs/todo.md` and deviations in the formats doc. Proposal: move each to `docs/todo.md` or the deviations list; where the port differs on purpose, leave "deviation: …" in one line. S.

31. **Missing one-line comments on non-trivial statics.** `set_mode` (`select.c:71`), `get_largest_defense`, `get_largest_weight` (`ui_champion.c:820, 828`; `largest_attack`'s comment at `:811` is a generic sentence), `append` (`:533`), `reset_wall_set_ids`, `get_wall_set_slot`, `set_slot` (`view.c:2045-2060`), `get_object_aspect` (`:555`). S.

## 6. Remaining decompiler flavor

32. **Functions tuned to the 60-line limit.** `draw_creatures_in_cell` 59 (`view.c:765`), `draw_square_things` 58 (`:1149`), `draw_wall_ornament` 57 (`:1263`), `ui_draw_champion_state` 57 (`ui_champion.c:312`), `panel_draw_object_text` 57 (`:566`), `command_process_queue` 56 (`command.c:806`), `anim_next_frame`, `layout_get_zone`, `place_in_parents`, `draw_square` 55. Most splits are natural (`draw_square` → one helper per shape, `view.c:1887-1968`). Three are mechanical:
   - `draw_wall_ornament` does five things: decode the text, set the facing flags, pick the scaled bitmap, size the plaque, draw the portrait. Split into `get_wall_ornament_bitmap`, `size_plaque_zone`, `set_facing_flags`.
   - `draw_creatures_in_cell` keeps the original's flag dance (`done`, `second`, `index < 0`, `front`, `rest`); it needs a case table in its comment and a `choose_creature_position()` that returns `{pos, second, done}`.
   - `draw_square_things` leaks `mcell` out of the loop into `draw_explosions` (`:1159, :1205`): the last cell visited decides the rebirth check, a side effect that deserves a name (`last_cell`).
   M.

33. **Deep nesting.** `get_projectile_pose` (`view.c:882-919`, four levels of if/else), `place_in_parents` (`layout.c:168-222`), `draw_masked_fill` (`draw.c:111-128` and `:129-138`), `draw_wall_text` (`view.c:1231-1244`, a `for` inside an `if` inside a `for` with three loop variables in one header). Proposal: early returns and one helper each (`fill_unit_rows`, `blit_temp`). S-M.

34. **The `SquareView` table repeats each left square as its right twin.** `view.c:1505-1855` (350 lines, 16 entries). Pairs differ only in the sign of `right`, `mirror`, and the L/R ids (`:1506-1530` vs `:1531-1555`, `:1725-1752` vs `:1753-1780`). `docs/todo.md:25` already notes the unread 0 fields and the literal D0C stairs zone. Proposal: keep the table (it is the clearest form) and (a) make "none" explicit with -1 in every row, (b) if the zone and layout ids of right squares are at a constant stride from the left ones, derive the right rows at init from the left rows with a `mirror_square_view()`; if not, accept the duplication and add a column comment. M (the stride check is the first step).

35. **The `Aspect` union** (`view.c:217-226`): `floor_ornament` aliases `front`, `state` aliases `far_side`, `door` aliases `right`; a reader must know which field is live for which element. Proposal: separate fields (8 bytes more). S.

36. **Sign-encoded tags.** `draw_wall_bitmap` treats `bitmap_id < 0` as a layout slot (`view.c:428-437`), and `load_wall_set` writes `slot_graphics[-1 - LAYOUT_…]` (`:2092-2099`); `get_projectile_aspect` returns a negative number for "a projectile aspect" (`:561-586`). Proposal: a `WallBitmap {int16_t graphic, slot;}` and a `ProjectileAspect {bool projectile; int16_t index;}`. M.

37. **Copied code.** `draw_viewport_icon` (`command.c:645-651`) = `ui_draw_icon_to_viewport` (`ui_champion.c:646-655`); the XOR loop of `ui_xor_screen_zone` (`ui.c:78-87`) = `command_highlight_disable` (`command.c:189-198`); point-in-box by hand five times (`ui.c:309`, `command.c:483`, `select.c:181`, `advanced.c:104`, `gameloop.c:488`); three screen-bitmap literals (finding 22); Amiga colour packing twice (finding 8). DM/CSB: `save_game_csb` and `startend_save_game` (`startend.c:142-199`) share the "saving game / unable to save" dialog pair. Proposal: `box_contains(const Box *, x, y)` in `draw.h`, `ui_xor_screen_box(const Box *)`, delete `draw_viewport_icon`, extract `save_with_dialog()`. S.

38. **Sentinels and ownership by convention.** `draw_object(…, int16_t *pile)` with NULL meaning "in flight" (`view.c:617-618`); `get_shrunk_bitmap` returns either the cached bitmap or `out` (`:534-545`), so ten call sites `free(own.px)` whether or not anything was allocated. Proposal: `ScaledBitmap { const Bitmap *bm; uint8_t *owned; }` with `scaled_bitmap_free()`. S-M.

## 7. Error handling and resource ownership

39. **malloc/free pairs are local and balanced** (`draw.c:44-49, 64-82, 107-139`; `view.c:1000-1015, 1393-1416`; `anim.c:61-94, 128-139`); failures are silent (a missing frame of a door or an icon), which is acceptable for a game. The view mallocs per object, creature, field and door on every frame (`get_shrunk_bitmap`, `draw_masked_fill`, `draw_door`). Not a bug; a senior would use static scratch buffers sized by the largest graphic, or one per-frame arena. M, optional.

40. **Lifetime coupling with the sound module.** `sound_play_data` keeps a pointer into anim's buffers and the logo image; `anim_close` (`anim.c:84`) and `logo_play` (`logo.c:45`) must call `sound_stop_all()` first ("the voices point into the file"). The owner of the memory must know a consumer's internals. Proposal: `sound_play_data` copies the sample (they are small), or the sound module takes ownership with a release callback. S.

41. **Threading contract is documented in the wrong place.** `music_mix` is safe only because `platform.c:240` pumps on the game thread; `music.h:31` says nothing. One sentence in `music.h`. S.

42. **Edge-case out-of-bounds read.** `draw_panel_scroll` (`ui_champion.c:783-793`) reads `end[-2]` when the text after the first newline is empty, that is `rest[-2]`; for a scroll text that starts with `\n`, `rest = text + 1` and `end[-2]` is `text[-1]`, one byte before the array. Proposal: guard `strlen(rest) >= 2` (the comment says the count quirk is intended; keep the quirk, remove the read). S.

43. **Unchecked results.** `select_item_box` ignores `text_get_size`'s false (`select.c:169`; an empty item would use uninitialized `w, h`; items are constants); `ui_extract_icon` with an icon below the first sheet reads sheet -1 (`ui.c:224-226`); `docs/todo.md:28` already lists the ignored `inventory_fill_leader_hand_at_fountain` result. S.

44. **No `SDL_Log` and no SDL in ui** (verified). ui reports failures by return value only; that is why the inits return 0/-1 without a reason (finding 19). Keep.

45. **Run-time write into the loaded tables.** `set_endgame_click_box` (`startend.c:236-239`) patches the KAOS mouse table through `tables_set_mouse_input_box`, as the original did; it is the only ui write into the image. Keep; name it in `docs/tables.md` if it is not there.

## 8. Dead code and unused API

46. **Twelve exported functions with no caller outside their own file** (grep over `port/src`, `tests/`, `tools/`): `ui_blit_graphic_box`, `ui_draw_champion_bar_graphs`, `ui_draw_icon_in_slot_box`, `ui_draw_panel_food_water_poisoned`, `ui_draw_status_box_portrait`, `ui_get_icon_in_slot_box`, `ui_get_icon_zone_box`, `ui_print_zone_padded`, `menus_draw_action_icon`, `menus_draw_spell_area_controls` (`ui_menus.c:187-190`, a wrapper nobody calls), `advanced_draw_automap`, `command_set_game_input`. Proposal: `static`, delete the wrapper and the `slot_box_icon` cache (finding 26). S.

47. **Exported only for the tests, unmarked.** `view_draw`, `anim_open/next_frame/close`, `select_item_box`, `g_music_track/last/requested`, `g_waiting_frame`, `ui_get_slot_box_zone` (20 test uses, no port caller outside ui) have no caller in `port/` outside their module. Proposal: keep exported, mark "exported for the tests" in the header (a rule in the standard), or give the tests one `ui_test.h`. S.

48. **Duplicate engine helper in ui.** `view.c:1421-1435` vs `map.c:86-102` (finding 6).

## 9. Other points a senior reviewer would raise

49. **Header hygiene.** `ui.h:6-8` includes `champion.h` (needs only `Champion` for one prototype at `:151`) and `tables.h` for nothing of its own; `command.c`, `dialog.c`, `startend.c`, `ui.c` call `tables_*` without including `tables.h` (they ride on `ui.h`). `advanced.h:6` includes `game.h` for nothing. `command.h:6` includes `move.h` to re-export the movement commands. `text.h:7-9` includes `ids.h` for the one-use alias `TEXT_FONT_ITEM` (`text.c:18`). Proposal: each `.c` includes what it uses; headers include only what their declarations need; forward-declare `Champion`. S.

50. **Include order varies by file.** `command.c:3-23` own header then an unsorted mix; `automap.c:1-3` `<stdio.h>` after two project headers; `gameloop.c:16-17` `startend.h` before `dungeon.h`. `.clang-format` has `SortIncludes: Never`. Proposal: a rule (own header, system headers, project headers alphabetical by layer) and `IncludeCategories` in `.clang-format` once finding 2 gives every include a layer prefix. S.

51. **Table data in source.** `musicmap.c` (316 lines) and `music.c:31-59` hold the FM-Towns and Super NES track tables from other platforms' executables, allowlisted as "not in the game files"; the source is a git-ignored `reference/extracted/music_map.json` (`musicmap.c:4`). Rule 6 says game tables are read at run time. Proposal for the PRD to decide: ship `music/soundtracks.json` next to the MP3 files and load it in `music_select` with `file_read`, or keep the tables and write the exception into the standard. M.

52. **Implementation-defined shifts of negative 16-bit values.** `view.c:541-542, 945, 1102-1104, 1220-1221` (`(int16_t)(…) >> SCALE_SHIFT`) are documented "as in the original"; a right shift of a negative signed value is implementation-defined (arithmetic on clang and gcc). One sentence in the standard ("the port assumes arithmetic right shifts") closes it. S.

53. **`const` correctness** is good (`const Bitmap *src`, `const Box *`, `const Champion *`, `const uint8_t *spells`). Two exceptions: `automap_draw(Bitmap *, Box area, …)` (finding 23) and `draw_scroll_line(…, char *text)` modifies its argument in place (`ui_champion.c:694-709`); say so or copy. S.

54. **Loop-counter types vary by file** (`for (uint16_t i …)` `ui_menus.c:93`, `for (int16_t i …)` `command.c:504`, `for (int k …)` `ui_champion.c:92`) over the same 16-bit counts; rule 3 covers C-array loops only. One line in the standard. S.

## What is good and should stay

- The layer rule and its check: no SDL, no platform header, no engine write in `ui/` (all verified).
- `layout.c`: the zone parent walk (`place_in_parents`, `layout_get_zone`) is clear and well commented; `draw.c`: small, const-correct, every function explains itself in game terms.
- `select.c`: a textbook small state module (one table, statics, a thin API).
- `automap.c`, `advanced.c`, `dialog.c`, `ui_menus.c`, `text.c`, `video.c`: short functions, good comments, no surprises.
- The `square_views` table and the `draw_square` dispatch (`view.c:1972-2026`): keep the table form; the per-shape helpers read well.
- `command_process_queue` as the single dispatch and the `InputTables` save/restore struct (`command.c:56-101`).
- The kept-quirk comments at casts and halvings ("as in the original": `view.c:313, 540, 944`, `dialog.c:92`, `ui_champion.c:264, 789`).
- The frame-hook design itself (blocking sequences with a frame callback; NULL hook for turn-based mode) is the right shape for this port; it only needs its contract written down (finding 4).
- Game-thread mixing of music and sound without locks (`platform.c:240`).
- Names in game terms throughout (`g_facing_fountain`, `menus_draw_action_damage`, `startend_run_fuse_sequence`, `panel_redraw_scroll`), the prefix lines, `ARRAY_COUNT`, `max16/min16`, `turn_left/right/back`.
- `music.c`'s soundtrack selection logic and `anim.c`'s chunk player are compact and readable; only their location and the vendored header are in question.
