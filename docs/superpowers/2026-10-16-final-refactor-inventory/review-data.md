# Review of port/src/data/ (senior C reviewer, read-only)

Scope: every file of `port/src/data/` (28 files, 3575 lines) and the callers in `engine/`, `ui/`, `platform/`, `tests/`. Read first: `docs/coding-standard.md`, the clean-code PRD (`docs/superpowers/2026-10-15-clean-code.md`, Current state, Requirements, Decisions). Nothing that `clang-format` or `tests/test_standard.py` already enforces is listed. Line numbers are of the tree at commit 0771406.

Sizes: S under an hour, M a few hours, L a day or more. Each finding says whether it is a real problem or fine.

## 1. Module boundaries and cohesion

1. **ftl.h mixes the generic FTL loader with the KAOS singleton.** `port/src/data/ftl.h:12-13` (`ftl_load`, `ftl_free`, also used by `swsh.c:20`) and `ftl.h:15-22` (`extern FtlImage kaos`, `kaos_load`, `kaos_ptr/u8/u16/s16/u32`) are two concerns with two prefixes in one header; `ftl.c:257-301` holds both. Real problem: a reader of `swsh.c` or `csbimage.c` gets the engine singleton with the loader, and the standard says one prefix per header is the *limit*, not the aim. Proposal: `ftl.h/.c` keep the loader; new `kaos.h/.c` own the engine image, its load (with the CSB build) and the readers. S-M.

2. **spells.h carries the spell record layout beside the spell rules.** `spells.h:7-10` (`SPELL_BYTES`, `SPELL_ENTRY_LEVEL/SKILL/ATTRIBUTES` = byte offsets 4, 5, 6) and `spells.h:28-39` (three `static inline spell_attributes_*` unpackers) are used only by `tables.c:148-155` (`read_magic`). Real problem: byte offsets in a public header against rule 7 ("no byte offsets in names"), and a second record-layout owner. Proposal: move them into `tables.c` beside `CREATURE_AT_*` (`SPELL_AT_LEVEL`...); `spells.h` keeps the rules only. S.

3. **Two owners of "which spell table and how many spells".** `spells.c:16-23` (`get_spells_address`, `spell_count`) and `tables.c:145-148` (`loaded_spell_count`, the same `csb ? CSB_SPELLS : KAOS_SPELLS` ternary). `spell_find` (`spells.c:42-47`) re-reads the raw image with `kaos_u32` although `tables.c:85` caches `spells[]` with `symbols` already decoded. Real problem: duplicated logic. Proposal: `spell_find` loops `tables_get_spell(i)` until NULL; delete `get_spells_address`; `spell_count` becomes `tables_get_spell_count` or goes (callers: tests only, `tests/test_csb_engine.py:148,157`). S.

4. **The mana cost and symbol formulas exist four times; the data-layer versions have no caller.** `spells.c:52-54` (`spell_get_symbol`) and `spells.c:58-65` (`spell_get_symbol_mana_cost`) have 0 callers in `port/src` (tests only: `tests/test_spells.py:68-71`). The same formulas are written again in `engine/magic.c:407-411` and `:416`, `ui/ui_menus.c:198`, `platform/state_json.c:395-400`. Real problem: a rule with four copies. Proposal: the three callers call `spell_get_symbol` / `spell_get_symbol_mana_cost`; then `tables_get_symbol_base_mana_cost` and `tables_get_symbol_mana_cost_multiplier` (`tables.h:166-167`) lose their last external callers and can become static. S.

5. **tables.h/tables.c is one 366-line header and a 790-line file for six table families.** Rule tables (`tables.h:9-177`), movement/sound/text (179-204), dungeon view (206-250), palettes (252-280), interface metrics (282-309), mouse pointer (311-321), mouse and key input (323-364). Is it a problem? Moderate: the sections are marked and one prefix keeps lookup easy; the prefix rule pushes toward one header. Proposal: keep one header and the `tables_` prefix; if the owner wants shorter files, split the `.c` by section (`tables.c` cached rule tables, `tables_view.c`, `tables_ui.c`) with no API change. M, optional.

6. **`UI_INPUT_*` is a data-layer enum named after its consumer.** `tables.h:323-341`, comment "Mouse input tables of COMMAND.C" (an original source file name). Real but small. Proposal: `TABLES_MOUSE_*` beside `TABLES_KEYS_*` (`tables.h:354-362`), comment in game terms. 17 call sites in `ui/command.c`, `ui/dialog.c`, `ui/startend.c`, `ui/ui.c`, `ui/ui_champion.c`, `platform/control.c`. S.

7. **ids.h is the right owner but is organized by consumer, not by kind.** `ids.h:4-5` says "Each module adds its own section", so `ZONE_` sits in 5 sections, `TEXT_` in 4, `GFX_` in 4, `ICON_` in 3, and one value has two names: `ZONE_DIALOG_ON_SCREEN` (`ids.h:78`) and `ZONE_THE_END` (`ids.h:90`) are both zone 5 (also `ZONE_FIELD_FIRST` 181 == `ZONE_WALL_D3L2` 346, documented). Use outside `data/`: `ZONE_` ui 277 / engine 0, `LAYOUT_` ui 26, `GFX_` ui 107 / engine 1, `ICON_` engine 75 / ui 21, `TEXT_` ui 52 / engine 10. Verdict: the owner is fine (these are Graphics.DAT item, layout-zone and string-table numbers, which is data); the grouping is the problem. Proposal: regroup by kind (GFX, ZONE, LAYOUT, ICON, TEXT), sort by value, one name per value; keep the file in `data/`. S-M, mechanical, no call-site change.

8. **`PATH_SIZE` lives in adventure.h.** `adventure.h:15`, used by `adf.c:98,150`, `engine/savegame.c:273,322,551`, `ui/anim.c`, `ui/music.c`, `ui/gameloop.c`, `platform/control.c`, `platform/main.c`. It is a file-path limit. Proposal: move to `file.h`. The dungeon ids (`adventure.h:11-12`) are adventure data and fine there. S.

9. **`os_compare_names` is a string compare in the "folder services" module.** `os.c:18-27`, `os.h:6,16`. Proposal: move to `file.h` as `file_compare_names` (it compares file names) or rename the module purpose line. S.

10. **adventure.c still has parallel arrays keyed by adventure.** `adventure.c:12-15` (`adventures[]`), `:32-38` (`game_files[][GAME_FILES]`), `:41-47` (`game_disks[]`). R10.6 asked for one struct per adventure. `adventure_csb_present` (`:81-84`) loops all `GAME_FILES` entries without the NULL-name stop that `adventure_bad_file` (`:92`) uses; it works only because CSB has exactly 4 files. Proposal: `Adventure` gets `const GameFile files[GAME_FILES]`; both loops stop at a NULL name. S.

11. **Duplicate counts across data files.** "8 dialog tables": `tables.h:340` (`UI_INPUT_DIALOG_FIRST + 8`), `csbimage.c:15` (`DIALOG_TABLE_COUNT 8`), and `ui/dialog.c:173` recomputes the index as `screen * MAX_CHOICES + choices - 1`. `csbimage.c:20` `KAOS_ACTION_SET_BYTES = 8` duplicates `tables.c:14` `ACTION_SET_BYTES 8`. The APPB/KAOS pointer size is the literal `4` in `csbimage.c:180,182` and `csbtext.c:192,202,207` while `tables.c:32` names it `DIALOG_TABLE_POINTER_BYTES`. The test catches duplicates only across headers. Proposal: one `TABLES_DIALOG_TABLE_COUNT` in `tables.h`, one `ACTION_SET_BYTES` (a small private `tables_layout.h` or a comment-free move into `tables.h`), one `LONG_BYTES 4` in `bytes.h`. S.

12. **The "load the engine module and Graphics.DAT of a folder" sequence is written twice outside data.** `platform/main.c:40-50` and `ui/gameloop.c:50-60` both build `dir/<engine>` + `tables_load` + `dir/Graphics.DAT` + `gfx_open`. Proposal: `adventure_load_files(dir)` in `data/` owns the sequence and the file names. S. (Cross-layer; listed because the data API should offer it.)

13. **graphics.c holds the string lists beside the decoder.** `graphics.c:209-275` (strings, action names, object names, the CSB override through `csbtext.c`). Verdict: fine; it is "Graphics.DAT items as text", and a split would need a new prefix. Keep.

## 2. Naming

14. **`kaos` is a global without `g_`, and engine reads it directly.** `ftl.h:15` `extern FtlImage kaos;` breaks rule 7; `engine/dungeon.c:371` reads `kaos.data` to test "image loaded", which `test_standard.py:239-246` does not catch (it looks for `kaos_` calls). Real problem: the one data global that escapes the accessor rule. Proposal: `static FtlImage image;` in the kaos module plus `bool kaos_is_loaded(void)`; `dungeon.c:371` and `tables.c:201` call it. S.

15. **A base-adjusted address of the original survives.** `kaos_addresses.h:93` `KAOS_PLAQUE_HEIGHTS 0x102c57 /* one byte before the table: a line count starts at 1 */`, so `tables.c:500` indexes with a 1-based `lines` and `csbimage.c:103` must copy from `KAOS_PLAQUE_HEIGHTS + 1`. This is the pointer trick of the 68000 code. Proposal: name the real table start (`0x102c58`) and write `lines - 1` in `tables_get_plaque_height`. S.

16. **Address names with the original symbol or an odd shape.** `KAOS_INITIAL_COLORMAP` (`kaos_addresses.h:110`; it is the interface palette, cf. `TABLES_PALETTE_INTERFACE`), `KAOS_INVISIBILITY_PALETTE_CHANGES` (`:124`) against the `KAOS_PALETTE_CHANGES_*` order, `KAOS_REBIRTH_SCALES` (`:78`, step 2) next to `KAOS_REBIRTH_STEP_1_SCALES` (`tables.c:466` has to pick by step), `KAOS_ACTIONS_DISABLED` (`:23`) against `KAOS_ACTION_*`, `KAOS_ICON_GRAPHIC_FIRST_INDEX` (`:129`) vs the accessor `tables_get_icon_sheet_first`, `KAOS_FOUNTAIN_ORNAMENT_INDICES` (`:96`, one byte). Proposal: rename to the accessor's noun (`KAOS_INTERFACE_PALETTE`, `KAOS_PALETTE_CHANGES_INVISIBILITY`, `KAOS_REBIRTH_STEP_2_SCALES`, `KAOS_ACTION_DISABLED_TICKS`, `KAOS_ICON_SHEET_FIRST_ICONS`, `KAOS_FOUNTAIN_ORNAMENT`); update `csbimage.c` rows and `docs/`. S.

17. **Noun-only public functions against rule 7, not caught by the test.** `gfx_string`, `gfx_object_name`, `gfx_action_name` (`graphics.h:33-35`), `gfx_count` (`:23`), `spell_count` (`spells.h:41`), `adventure_of`, `adventure_default_dir`, `adventure_engine_name`, `adventure_save_name`, `adventure_backup_name`, `adventure_bad_file` (`adventure.h:28-40`). Either the standard allows noun accessors in `data/` (then it must say so; today it allows nouns only for `map.h`), or rename (`gfx_get_string`, `adventure_get_dir`...). `gfx_string` has about 50 call sites. S-M.

18. **Poor or clashing file-static names.** `graphics.c:22` `static uint16_t count;` (R7.4: short names only for loops and adjacent lines); `graphics.c:217` `strings, action_names, object_names` and `csbtext.c:119` `strings, action_names` are the same names for related but different objects in two files. Proposal: one `static struct { ... } graphics;` and `static struct { ... } csb_text;` per file (also gives one reset for tests). S.

19. **Two identical rectangle types in two layers, three rectangle shapes in data.** `tables.h:247-249` `TablesRect {int16_t x, y, w, h}` is field-for-field `ui/draw.h:7-9` `Box`. `tables.h:312-314` `PointerZone {x, y, w, h, shape, champion}` and `tables.h:344-347` `MouseInput {command, x1, x2, y1, y2, buttons}` keep the image layout (fine as records). Proposal: `Box` moves to `data/graphics.h` beside `Bitmap`; `TablesRect` goes (3 ui users: `dialog.c:47`, `startend.c:315`, `view.c:430`); `tables_get_pointer_planes(bool, int16_t *w, int16_t *h)` (`tables.h:321`) returns a struct instead of two out-params. S.

20. **`TEXT_` is a constant prefix for four unrelated things.** String ids (`ids.h:138-168, 244-247, 275-305`), font metrics `TEXT_CHAR_WIDTH`, `TEXT_LINE_HEIGHT`... (`graphics.h:12-15`), `TABLES_TEXT_*` short image texts (`tables.h:296-303`), and in engine `TEXT_MODE_*`, `TEXT_CODE_*`, `TEXT_SIZE` (dungeon text decoding). Real but low: a reader cannot tell a string id from a font metric by the prefix. Proposal: font metrics become `FONT_CHAR_WIDTH`, `FONT_LINE_HEIGHT`, `FONT_CHAR_GAP`, `FONT_BASELINE_GAP` and move to `ui/text.h` with their only users (ui and `engine/message.c`; engine includes data, so `graphics.h` stays legal, but the metrics describe the built-in font, not the decoder). S-M.

## 3. Types and API shape

21. **Two parameter types for the same game values.** The cached accessors take `int` (`tables.h:157-174`), the call-time accessors take `int16_t` (`tables.h:183-364`); callers pass `int16_t` creature types, slots and indices (`engine/champion.c:297`, `engine/group.c:460`...). Rule 3: one value, one type on its path. Proposal: `int16_t` everywhere in `tables.h`; the `GET` macro (`tables.c:212`) already handles a negative index. 6 test files set `argtypes` on `tables_get_*`; most leave the default `c_int`, which still passes an `int16_t` on arm64 and x86-64, but the explicit ones (`tests/test_tables.py:74`, `tests/test_spells.py:31`) change. M.

22. **Nine accessors return a pointer to a scalar, and every caller dereferences at once.** `tables.h:162-174`: `tables_get_junk_weight`, `_food_value`, `_symbol_base_mana_cost`, `_symbol_mana_cost_multiplier`, `_light_power_to_light_amount`, `_palette_light_threshold`, `_slot_mask`, `_wound_defense_factor`, `_fixed_possession`. All 20 call sites write `*tables_get_...` (`engine/inventory.c:140,296,343,371,521,570,588`, `champion.c:297,301,319,731`, `group.c:460`, `magic.c:208,240,407,410`, `action.c:624`, `timeline_process.c:358`, `ui/ui_champion.c:836`, `platform/state_json.c:395,398`). A NULL return crashes at the deref anyway, so the pointer buys nothing and costs a `*` per use. Proposal: return by value like the call-time accessors; out of range follows the same policy as `kaos_u8` (abort in the image reader) or returns 0 with a comment. Tests: `tests/test_tables.py:74-75,141`, `tests/test_champion.py:447-449` bind `POINTER`. S-M.

23. **Two accessor regimes ("after tables_init" vs "after kaos_load") leak into ui and tests.** `tables.h:153-156` and `:182`; `tables.c:76` `ready`; `ui/view.c:2159-2160` calls `tables_init()` lazily when `tables_get_creature_info(0)` is NULL (a ui module repairing a data module's state); 15 test files call `kaos_load` then `tables_init` (`tests/test_tables.py:77-79`, `test_sound.py:42`, `test_view.py:152`...). Proposal: `tables_load` is the only public entry (it already is `kaos_load` + `tables_init`, `tables.c:289-293`); `tables_init` becomes static; tests call `tables_load`; the `ready` flag and the view.c hack go. M (15 test files, one ui line).

24. **Three success/failure conventions in one layer.** `int 0/-1`: `ftl_load`, `kaos_load`, `tables_init`, `tables_load`, `gfx_expand`, `gfx_expand_over`, `gfx_load_bitmap`, `gfx_load_strings`, `adf_extract`, `csb_image_build`, `csb_text_build`. `int` count-or-minus-one: `gfx_open` (`graphics.c:27-54`). `int 1/0/-1`: `adventure_install_disk` (`adventure.h:41-43`). `bool`: `swsh_load`, `adf_has_file`, `file_exists`, `os_make_folder`. NULL: `file_read`, `gfx_get_item`, `gfx_get_bitmap`, `tables_get_*`. The standard (rule 3) makes truth values `bool` but says nothing about success codes. Proposal: a standard rule: success returns `bool`; a count returns `int` with the count only; a three-way result is an `enum` (`ADVENTURE_DISK_INSTALLED`, `_PRESENT`, `_FAILED`); a lookup returns NULL. Then convert the 11 `0/-1` functions; `tests/` `restype` lines follow. M.

25. **Implicit narrowing and sign conversions where rule 3 asks for a cast and a comment.** `ftl.c:294-297` `kaos_s16` returns `kaos_u16()` through an implementation-defined `uint16_t`→`int16_t` conversion (C11 6.3.1.3p3); `tables.c:685-688` `tables_get_message_color` returns `uint8_t` from `kaos_s16` (the low byte of a word, by design per `kaos_addresses.h:149`); `csbtext.c:160` `strings_size = size` (`size_t`→`uint16_t`); `graphics.c:53` `return count = n;` (assignment in a return). Proposal: `(int16_t)` with "sign" comment in `kaos_s16`; `kaos_u8(addr + 1)` for the color; a bounded `size` check in `join_texts`; `count = n; return n;`. S.

## 4. Global mutable state

26. **`kaos`**: see finding 14. The only data global that is not `static` and not `g_`.

27. **`g_adventure`** (`adventure.h:22`): read in all layers, written by `ui/select.c:85,123`. It is the run configuration; a global is the honest shape. Keep. The comment "game_start_new does not reset it" is history, not purpose. S (comment).

28. **File-scope arrays of tables.c, graphics.c, csbtext.c** (`tables.c:76-95`: 20 arrays plus `ready` and `loaded_spell_count`; `graphics.c:20-24`: 5; `csbtext.c:119-120`: 4) are all `static` already. Fine. Optional: one `static struct` per file (finding 18) so a test reset is `= {0}`. S, optional.

29. **`csb_copies` and `csb_copy_count`** (`csbimage.h:20-21`) are exported only for `tests/test_csb_engine.py:81-82`. A test seam; fine, but the header should say "exported for the tests". S.

30. **The os hooks** (`os.h:9-10`) are the sanctioned way for lower layers to reach the platform; `tests/conftest.py:72-80` sets and clears them. Keep.

## 5. Comments

31. **Comments reference globals that no longer exist.** `ids.h:139,140,142,143` name `g_base_skill_string_indices`, `g_skill_level_string_indices`, `g_direction_string_indices`, `g_statistic_string_indices`; `ids.h:439` and `ui/view.c:30` name `g_wall_set_layout_ids`. These became `tables_get_base_skill_text`... (`kaos_addresses.h:139-142, 90`). S.

32. **Outdated or original-name comments.** `tables.h:7` "Static rule tables of KAOS.FTL, decoded once" describes the first third of the header; `tables.h:129` `kind /* 1 potion, 2 projectile, 3 other */` misses 4 (magic map) and does not name `SPELL_KIND_*` (`spells.h:25`); `tables.h:137` `flags /* copied with the set; no reader is known */` admits dead data; `graphics.h:34` "graphic 0x2b6" for `GFX_OBJECT_NAMES` 694; `tables.h:323` "COMMAND.C"; `ftl.c:192` "like BJELoad_R" (an original symbol; rule 4 says the address belongs in `docs/kaos-symbols.csv`, the name may stay if it helps). S.

33. **Headers without a purpose line.** `ftl.h` (none), `graphics.h` (only "Amiga planar graphics" for two constants), `spells.h` (none). The others have one (`tables.h:7`, `ids.h:4`, `adventure.h:7`, `adf.h:6`, `swsh.h:8`, `csbimage.h:6`, `csbtext.h:6`, `os.h:6`, `bytes.h:6`, `crc32.h:7`, `kaos_addresses.h:4`). Proposal: the standard fixes the header head order: guard, prefix line, purpose line, includes. S.

34. **Where the one-line function comment goes is not decided.** `adventure.c`, `os.c`, `file.c`, `csbtext.c` comment in the header; `tables.c` comments the call-time accessors in the `.c` and the cached ones in the header (`tables.h:156`); `graphics.c` mixes (`gfx_count`, `gfx_free_bitmap`, `list_get` have none; `gfx_string` has it in the header). Proposal: standard rule: public functions in the header, static ones in the `.c`, never both; the test can check that each public prototype has a comment line above it. S (doc), M (test).

## 6. Remaining decompiler flavor

35. **Unnamed file-format values.** `ftl.c:116-117` version bytes `2, 1, 0`; `ftl.c:135` entry version `1` and the entry layout `e + 2`, `e + 4`, `e + 8` (tag, version, offset, size); `ftl.c:22` `enum { SECTION_INFO = 16, SECTION_DATA, SECTION_CODE, SECTION_COUNT = 3 }` mixes tag values and a count; `ftl.c:227-228` word alignment by `& ~1ull` twice; `graphics.c:42-48` Graphics.DAT header `4`, `8u`, `2 * i`. All under the literal limit of the test, but a reader cannot tell a version from a count. Proposal: `FTL_FORMAT_VERSION`, `FTL_ENTRY_TAG/VERSION/OFFSET/SIZE`, a separate `SECTION_KINDS 3`, `align_to_word()`, `GFX_HEADER_BYTES`, `GFX_ITEM_ENTRY_BYTES`. S.

36. **Record reads with raw offsets and a 17-field positional literal.** `tables.c:120-140` (`read_objects`: `a + 2`, `a + 3`, `a + 4`, `+ i * 2 + 1`) and `:165-182` (`read_actions`: `a + 4`, `a + 5`, `a + 6`) while `read_creatures` names every offset (`CREATURE_AT_*`, `:56-74`); `tables.c:102-112` is a positional compound literal with 17 members, where one swapped line changes a rule silently. Proposal: named offsets for every record and designated initializers (`.aspect_index = kaos_u8(a + CREATURE_AT_ASPECT_INDEX)`). S.

37. **`(int8_t)kaos_u8(...)` nine times.** `tables.c:115-116, 179, 441, 478-484`. Proposal: `kaos_s8(addr)` in the kaos module (one cast, one comment). S.

38. **A tri-state `int` visitor result.** `adf.c:38` `EntryVisit` returns 1 (found, stop), -1 (error), 0 (go on); `each_entry` (`:59-78`) mixes its own -1 with the visitor's; `adf_has_file` (`:90`) tests `== 1`. Readable but implicit. Proposal: `enum { ADF_CONTINUE, ADF_STOP, ADF_BROKEN }`. S, low priority.

39. **One dense expression.** `ftl.c:68` `v = v << NIBBLE_BITS | (b->pos < b->n ? b->s[b->pos / 2] >> (b->pos % 2 ? 0 : NIBBLE_BITS) & NIBBLE_MASK : 0);` relies on three precedence levels. Proposal: two statements (`nibble = ...; v = v << 4 | nibble;`). S.

## 7. Error handling and resource ownership

40. **A failed CSB build leaves `kaos` half loaded.** `csbimage.c:184-185` stores `*out = built;` and then calls `csb_text_build`; when that fails, `csb_image_build` returns -1 but `kaos.data` already points at the image, so `kaos_load` (`ftl.c:264-269`) reports failure while `dungeon_load` (`engine/dungeon.c:371`) and `tables_init` (`tables.c:201`) see a loaded image. The first failure path (`csbimage.c:164-167`) frees `mem`; the second hands it to `out`: two ownership rules in one function. Proposal: build into a local, call `csb_text_build` on the local, free on failure, assign `*out` only on success. S.

41. **Undefined behavior: a pointer formed 1 MiB before the buffer.** `swsh.c:26` `const uint8_t *at = out->image.data - SWSH_BASE;` (C11 6.5.6p8), the same bug R10.3 fixed in `csbimage.c` (now `appb_at`, `:154-157`). `ftl.c:273-280` (`get_image_pointer`) is a third copy of the same address arithmetic. Proposal: one `ftl_at(const FtlImage *, uint32_t addr)` in `ftl.h`; `swsh.c`, `csbimage.c`, `csbtext.c:127,136,196` and the kaos module use it. S.

42. **The image reader aborts.** `ftl.c:273-280` prints to `stderr` and `abort()`s on an address outside the image; in `libdm.dylib` this kills a pytest worker, and `%#x` on a `uint32_t` is not the `PRIx32` the standard asks for. Verdict: the abort is defensible (an address outside the image is a port bug, not a runtime condition) and matches engine `game.c:58`, `map.c:159`. Proposal: keep, say so in the standard ("data aborts on a port bug, returns false on a bad file"), use `PRIx32`. S.

43. **No symmetric close.** `gfx_open` (`graphics.c:27-36`) frees the previous state and `kaos_load` (`ftl.c:263`) frees the previous image, but there is no `gfx_close`, `kaos_free` or `gfx_free_strings`; `gfx_open` on a `malloc(offsets)` failure (`:44`) returns -1 with `graphics_data` kept until the next open. Acceptable for a game process; a leak checker on the test library will report it. Proposal: `gfx_close`, `kaos_free`; `gfx_open` frees on every failure path. S.

44. **An unexplained factor.** `graphics.c:233` `malloc(2 * (size_t)size + 1)`: the 2 is the worst case of the CSB object names (`:239-241` inserts one NUL per character with bit 7 set). One comment. S.

45. **Errors in data/ are silent by design.** Every loader returns -1/NULL; `platform/main.c:45` logs "cannot load %s", so a wrong checksum and a missing file read the same. Fine for a game; the standard should state "data/ never logs; platform logs". S (doc).

## 8. Dead code and unused API

46. **Functions with no caller in port/src.** `tables_get_door_info` (`tables.h:164`, the cached `DoorInfo` duplicates `tables_get_door_attributes` `:199`, which has 2 callers): delete the table, the struct and `read_objects` line 139. `gfx_count` (`graphics.h:23`; tests only). `gfx_expand` public (`:25`; internal use plus 2 tests). `spell_count`, `spell_get_symbol`, `spell_get_symbol_mana_cost` (tests only; finding 4 wires them in instead). `adf_is_safe_name` public (`adf.h:15`) only for `tests/test_adf.py:94` (a parser rule test; keep public, say "exported for the tests"). S each.

47. **Two APIs to the same adventure data.** `adventure_get` (`adventure.h:25`, one caller `platform/main.c:480` for `.title`) beside four field accessors (`adventure.h:30-36`; callers `ui/gameloop.c:54`, `platform/main.c:43,418,471,693`, `engine/savegame.c:315,464,526`). Proposal: keep `adventure_get` and write `adventure_get(g_adventure)->engine`, or keep the four and add `adventure_title`; not both. S.

48. **Dead fields.** `ActionSet.flags` (`tables.h:137`, "no reader is known"). `FtlImage.data_base` and `.code_base` (`ftl.h:9`) are read only by `csbimage.c:164` (`appb->data_base` check); `csbimage.c:168` `FtlImage built = {mem, size, KAOS_BASE, CSB_SPELLS, 0};` sets `data_base = CSB_SPELLS` and `code_base = 0` positionally, values nobody reads. Proposal: drop `flags`; designated initializer with only the fields that mean something; or keep `data_base` with a comment that it is the APPB check only. S.

## 9. Other things a senior reviewer flags

49. **Include guards use two schemes.** `DM_ADF_H`, `DM_FILE_H`, `DM_OS_H`, `DM_BYTES_H`, `DM_CRC32_H` (the files the clean-code pass added) vs `ADVENTURE_H`, `TABLES_H`, `FTL_H`, `IDS_H`, `GRAPHICS_H`, `SPELLS_H`, `SWSH_H`, `CSBIMAGE_H`, `CSBTEXT_H`, `KAOS_ADDRESSES_H`; ui has `TEXT_H`, `VIEW_H`, `UI_H`, `DRAW_H`. No collision today, but `GRAPHICS_H`, `TEXT_H`, `FILE_H` are names any library uses. Proposal: `DM_<LAYER>_<FILE>_H` (`DM_DATA_TABLES_H`) in the standard, checked by the test. S.

50. **Two styles of packed-field accessor in headers.** `spells.h:28-39` three `static inline` functions; `tables.h:71-72` two function-like macros (`GRAPHIC_HORIZONTAL_OFFSET_RANGE(info) ((info) >> 12 & 3)`) for the same job. Proposal: `static inline` everywhere (type-checked, no double evaluation). S.

51. **The byte-order helpers are expression macros with the comma operator.** `bytes.h:7-10`: `BE16(p)` evaluates `p` twice; `WR16`/`WR32` use the comma operator that rule 3 bans in code. Proposal: `static inline` functions with the same names (about 150 call sites unchanged). S.

52. **Include order is not decided.** `tables.c:1` own header first; `ftl.c:1-9` system headers, then others, own header fifth; `adf.c:1-9` system then own; `graphics.c:3-8` alphabetical with the own header in the middle. "Own header first" is the convention that proves a header is self-contained; `.clang-format` has `SortIncludes: Never`, so the standard must say it. S.

53. **const-correctness of the image pointer.** `ftl.h:18` `uint8_t *kaos_ptr(uint32_t)` gives a writable pointer to 20 readers; the one writer is `tables_set_mouse_input_box` (`tables.c:771-778`). Proposal: `const uint8_t *kaos_ptr` and `kaos_write_u16(addr, value)` in the kaos module (also removes the `WR16` from `tables.c`). S.

54. **Small portability and cast nits.** `tables.c:318` `(uint32_t)(row * ATTACK_CELL_COUNT)` changes nothing; `adf.c:84,154` `(int32_t)BE32(...) != ADF_SEC_FOLDER` changes nothing for `!= 2` (and is implementation-defined for the file value -3); `crc32.c:12` lazy table init keyed on `crc_table[1]` is not thread-safe (the program is single-threaded; a `static const` table or an explicit `crc32_init` reads plainer); `file.c:10` `ftell` (`long`) into `int64_t` is fine. S.

55. **Directory structure (the owner asked).** `data/` holds three kinds of modules: file formats (`adf`, `ftl`, `graphics`, `swsh`, `file`, `crc32`, `bytes`), the KAOS image and its tables (`ftl`/kaos, `tables`, `kaos_addresses`, `spells`, `csbimage`, `csbtext`, `ids`), and run configuration (`adventure`, `os`). `port/Makefile:5-8` builds by a `LAYERS` wildcard, so subfolders need a `LAYERS` and `INCLUDES` change and the layer check of `test_standard.py` (include order by folder) must learn them. Verdict: 14 modules in one folder is readable; a split into `data/format/` and `data/kaos/` buys little and costs include paths and the layer test. Recommend: keep the flat folder; fix the names instead (findings 1, 7, 8, 9). The one rename worth doing: `csbimage.c`/`csbtext.c` become `csb_image.c`/`csb_text.c` to match their prefixes `csb_image_`/`csb_text_`, and `kaos_addresses.h` keeps its name. S.

## Good: keep, do not churn

- `kaos_addresses.h`: one name per address, the type and count in the comment, grouped by table; the "only `data/` includes it" rule and the test behind it.
- The call-time accessors of `tables.c:295-790`: tiny, one game-terms comment each, no cache to invalidate, no raw address outside `data/`; `tables_set_mouse_input_box` isolates the one image write.
- `csbimage.c:25-150` `csb_copies`: a declarative table with a comment column, verified byte for byte by `tests/test_csb_engine.py`; `appb_at` fixed the UB.
- `csbtext.c`: tables of (text id, APPB address) rows; `get_appb_string` bounds-checks with `memchr`.
- `ftl.c:111-249`: validates every size and checksum before it allocates, one cleanup label, `Nibbles` reader that reads zeros past the end, `expand_data` cannot write past `size`.
- `graphics.c:82-156`: `ByteReader`/`PixelWriter` make the decoder safe past the end (replaced the `NEXT`/`PUT` macros); the lazy `decoded[]` cache.
- `adf.c`: the visitor pattern with bounded chains (`guard`), `block()` returning NULL outside the disk, `adf_is_safe_name` with a test per character.
- `os.h`: hooks are the one sanctioned way for lower layers to reach the platform; tests set their own.
- `file.h`: two functions, no platform code; `bytes.h` `ARRAY_COUNT` as `int`, `WORD_BITS`/`BYTE_BITS`.
- Every file-scope variable except `kaos` is `static`; file-static names are descriptive except `count`.
- Struct comments in `tables.h` give units and sentinels (`defense; /* 255 = invulnerable */`, `attack_sound_ordinal /* 0 = none */`).
- No raw numbers at call sites: every id, offset and size has a name; the allowlist for `data/` has three lines, all for the APPB address tables.

## Suggested order for the PRD

1. Mechanical, no behavior risk, unblock the rest: 49 (guards), 52 (include order), 31-33 (comments), 16 (address names), 7 (ids.h regroup).
2. Structural in data/: 1 + 14 + 53 (kaos module, `g_`/static, const pointer), 41 (one `ftl_at`), 40 (CSB build ownership), 2 + 3 + 4 (spells), 19 (Box), 46-48 (dead API).
3. API shape, touches tests: 21 (int16_t), 22 (return by value), 23 (`tables_load` only), 24 (bool returns).
4. Standard text: 17 (nouns), 24, 34, 42, 45, 49, 52, and "an address table row counts as the definition" for the `csbimage.c`/`csbtext.c` allowlist lines.
