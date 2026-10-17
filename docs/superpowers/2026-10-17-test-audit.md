# Test audit against the test list of CLAUDE.md

Date: 2026-10-17. Scope: every test function in `tests/` (a parametrized test is one entry). Nothing is removed before the owner approves this list (`docs/todo.md`, "Test audit").

| Folder | KEEP | REMOVE | MERGE | KEEP? |
|---|---|---|---|---|
| `tests/port/` | 127 | 64 | 10 | 19 |
| `tests/differential/` | 114 | 23 | 2 | 26 |
| `tests/golden/`, `tests/standard/`, `tests/tools/` | 94 | 4 | 1 | 3 |
| Total | 335 | 91 | 13 | 48 |

## Decisions of the owner (2026-10-17)

1. Smoke runs: keep one. `port/test_modes.py::test_turnbased_program_answers_get_state_and_act` stays; it starts `build/dm` and answers over the socket. The other smoke runs go.
2. Bug regressions checked on UI pixels: keep them, one test per bug.
3. CSB magic map tile tests: keep them.
4. `standard::test_sources_are_formatted` stays; the clang-format step leaves the `Makefile`.

## Rulings on the KEEP? entries

Decisions 1-4 settle part of the KEEP? entries; the rest follows the test list of CLAUDE.md.

KEEP (30):
- Decision 1: `port/test_modes.py::test_turnbased_program_answers_get_state_and_act`.
- Decision 2 (a real bug): `port/test_automap.py::test_side_squares_behind_walls_stay_unseen`, `test_darkness_shortens_the_visible_depth`; `port/test_control.py::test_a_cast_leaves_the_cleared_runes_inverted_as_with_the_mouse`, `test_wait_does_not_stop_for_a_group_that_stays_in_view`, `test_the_tests_do_not_log_into_a_run`; `port/test_gameloop.py::test_freeze_covers_open_inventory`, `test_hand_object_name_follows_the_icon`, `test_pointer_over_the_status_box_hands`; `port/test_panels.py::test_eat_food_redraws_the_food_bar`.
- Decision 3: `differential/test_csb_magic_map.py::test_rune_of_each_spell`, `test_ku_creature_tile`, `test_dain_explosion_tile`, `test_ros_pit_and_fake_wall_tiles`, `test_party_and_footprint_borders`, `test_door_tiles`, `test_stairs_tiles`, `test_fireball_projectile_tile`, `test_zokathra_tile`, `test_floor_ornament_tile`, `test_wall_ornament_tiles`.
- Decision 4: `standard/test_standard.py::test_sources_are_formatted`.
- A rule or a data format: `port/test_modes.py::test_smooth_projectile_flight` (no game random numbers), `port/test_port_data.py::test_kaos_accessors` (big-endian reads), `differential/test_creatures.py::test_group_activates_and_approaches_party`, `test_group_attacks_adjacent_party` (no reference check covers them), `tools/test_dmtest.py::test_each_load_has_its_own_globals` (guards the rule "each test file owns its state").
- Keep with fewer asserts (weakened assertions): `differential/test_csb_ending.py::test_chunk_walk_consumes_the_file` (drop the tag counts of ENDA.DAT), `test_death_shows_restart_only_and_restart_loads_the_save` (drop the button pixel checks), `test_csb_intro_frames_and_music` (drop the frame count 851).

REMOVE (18):
- UI or layout: `port/test_entrance.py::test_ftl_logo_lasts_as_its_sound`, `port/test_ui.py::test_palette_split`, `differential/test_text.py::test_text_draw_clips`, `differential/test_csb_engine.py::test_layout_zones_equal_but_endgame_buttons`, `differential/test_csb_magic_map.py::test_panel_draws_again_each_tick`, `test_eye_keeps_the_leader_map` (both added with the feature in 91922a0, no bug record).
- UI wiring: `differential/test_champion.py::test_death_sets_redraw_flags`, `test_death_marks_the_champion_for_release`.
- Covered by another test: `port/test_actions.py::test_sword_attack_hurts_group_and_gains_experience`, `differential/test_tables.py::test_object_types_match_dmcsb_item_list`, `tools/test_csb_tools.py::test_csb_prison_state_through_the_tools`.
- Constants or data tables: `port/test_music.py::test_csb_square_table_matches_the_research`, `differential/test_dungeon.py::test_door_vertical_bit`, `differential/test_recruit.py::test_portrait_origin`, `differential/test_tables.py::test_strides_and_bases_match_decompiled_code`.
- CLI arguments or process start: `port/test_options.py::test_errors`, `port/test_select.py::test_load_failure_leaves_no_control_socket`.
- One outcome, no bug record: `port/test_music.py::test_music_stop_after_audio_stop_all_stops_the_track_and_keeps_the_soundtrack`.

Totals after the rulings: KEEP 365, REMOVE 109, MERGE 13 (487 entries).

## Audit of tests/port

220 test functions in 20 files; each parametrized test is one entry.

- KEEP: 127
- REMOVE: 64
- MERGE: 10
- KEEP?: 19

Most REMOVE: test_select.py 13, test_control.py 10, test_modes.py 8, test_automap.py 7.

The golden snapshots (tests/golden) already cover several port tests: protocol, visibility, fountain, vi altar, fuse sequence, screens and views.

### test_actions.py

| test | verdict | reason |
|---|---|---|
| test_action_matches_original | KEEP | reference check against the original (emulator) |
| test_melee_damage_matches_original | KEEP | reference check against the original (emulator) |
| test_sword_attack_hurts_group_and_gains_experience | KEEP? | rule on a real target group; doubt: damage, experience and disable are mostly covered by test_action_matches_original and test_melee_damage_matches_original |
| test_back_row_champion_cannot_reach | KEEP | rule with more than one outcome (front row hits, back row gets -1) |
| test_throw_creates_projectile_and_event_11_refills_hand | KEEP | rule with more than one outcome (event 11 refills the hand from the quiver) |
| test_event_11_ends_action_defense | KEEP | timeline state machine (disabled until the last tick, then defense cleared) |
| test_triggered_action_closes_menu_and_asks_for_action_area | REMOVE | wiring: session index and redraw flag after a click |
| test_poison_event_deals_damage_each_step | KEEP | rule with more than one outcome (poison decay per event); differential covers only the resistance |
| test_event_12_hides_damage | REMOVE | wiring: one event clears one index, one outcome |
| test_vi_altar_rebirth | REMOVE | covered by golden test_golden_rules.py::test_bones_on_the_vi_altar |

### test_automap.py

| test | verdict | reason |
|---|---|---|
| test_marks_start_and_squares_ahead | REMOVE | covered by golden test_golden_rules.py::test_visibility (all corridor squares of maps 0-3, 4 directions, 3 lights) |
| test_walls_stop_visibility | REMOVE | covered by golden test_golden_rules.py::test_visibility |
| test_side_squares_behind_walls_stay_unseen | KEEP? | regression of a real bug (fbdf438 "automap marks side squares only when the party can see them"); doubt: golden test_visibility covers map 1 too |
| test_save_load_round_trip | KEEP | data format (AMAP block, rejected bad inputs) |
| test_draw_stays_in_area_with_party_arrow_at_center | REMOVE | UI and layout (automap drawing) |
| test_rulers_number_the_squares_and_a_frame_ends_the_map | REMOVE | UI and layout |
| test_floors_are_light_and_walls_dark | REMOVE | UI and layout |
| test_a_pressure_pad_is_a_gray_square_in_the_floor | REMOVE | UI and layout |
| test_png_after_walking | REMOVE | UI; an optional PNG render (scaffold), the only assert is "more than 3 colors" |
| test_darkness_shortens_the_visible_depth | KEEP? | regression of a real bug (9d5a985 "darkness limits the rows an LLM player can see"); doubt: golden test_visibility uses lights 0, 4, 5 |

### test_control.py

| test | verdict | reason |
|---|---|---|
| test_state_json_after_recruiting | REMOVE | covered by golden test_golden_protocol.py::test_protocol_matches_the_golden_snapshot (get_state, get_view, get_map after the recruit) |
| test_control_session_moves_and_recruits | REMOVE | covered by golden test_protocol_matches_the_golden_snapshot (same walk, click_wall_ahead, recruit, wait, "fly") |
| test_mcp_tools | REMOVE | wiring (MCP wrapper over the socket) |
| test_save_and_load_restore_the_position_and_the_time | REMOVE | covered by golden test_protocol_matches_the_golden_snapshot (save, wait, load, get_state) |
| test_dm_play_prints_json_and_the_brief_summary | REMOVE | CLI arguments and output wiring |
| test_view_stops_at_a_closed_door | KEEP | rule with more than one outcome in the view JSON (closed door limits depth, hides the map square) |
| test_view_shows_a_mirror_on_a_side_wall | KEEP | data format (side_face of the view JSON) |
| test_act_many_runs_in_order_and_stops_on_failure_and_damage | KEEP | state machine (stop reasons done, failed, damage; bad input) |
| test_act_many_lists_the_damage_of_each_attack | MERGE | into test_act_many_runs_in_order_and_stops_on_failure_and_damage: one more field of the same reply |
| test_a_rune_without_enough_mana_fails | KEEP | regression of a real bug (c81d887 "add_rune fails without enough mana") |
| test_a_cast_leaves_the_cleared_runes_inverted_as_with_the_mouse | KEEP? | regression of a real bug (ff0616d "the control socket draws what its commands changed"); doubt: UI pixel check |
| test_wait_runs_all_ticks_or_stops_on_damage | KEEP | state machine (done, damage, limit refused) |
| test_act_many_stops_only_on_a_new_creature_group | KEEP | rule with more than one outcome (old group continues, new group stops) |
| test_turn_reports_a_fireball_and_a_killed_creature_seen_during_the_turn | KEEP | data format of the "seen" report and its brief line |
| test_a_group_in_view_for_many_ticks_is_recorded_once | KEEP | regression of a real bug (860f0f2 buffer overflow in turn reports) |
| test_wait_does_not_stop_for_a_group_that_stays_in_view | KEEP? | same bug area as test_a_group_in_view_for_many_ticks_is_recorded_once; doubt: merge there if the stop reason can be asserted with the fireball |
| test_view_names_explosion_types | MERGE | into test_brief_names_a_lock: one table of square_text cases (name and number fallback) |
| test_goto_is_an_unknown_command | REMOVE | covered by golden test_protocol_matches_the_golden_snapshot (the goto reply); hasattr check is wiring |
| test_a_move_into_a_wall_bumps | KEEP | rule with more than one outcome (bumped only on a blocked move; act_many stops) |
| test_a_teleporter_back_to_the_start_square_moves_the_party | KEEP | regression of a real bug (f390ae3 "reports moved, not bumped") |
| test_act_many_stops_when_a_spinner_turns_the_party | KEEP | state machine (turned stop, no stop on a normal move) |
| test_mcp_and_dm_play_batches | REMOVE | wiring (MCP and CLI wrappers of act_many) |
| test_swap_champions_and_look | KEEP | protocol rules with more than one outcome (swap refused for a == b, look needs the inventory, advanced ATTACK line) |
| test_take_put_and_look_in_an_open_chest | KEEP | regression of a real bug (4259c3f "chest slots in turn-based actions") |
| test_a_pile_lists_all_its_objects_top_first_in_take_order | KEEP | data format and order rule of piles |
| test_brief_prints_creatures_on_side_squares | KEEP | regression of a real bug (4259c3f "side-square creatures in brief") |
| test_brief_prints_doors_and_pits_on_side_squares | MERGE | into test_brief_prints_creatures_on_side_squares: same view_lines fixture |
| test_map_grid_has_indexes | KEEP | data format (map grid rows and header) |
| test_the_tests_do_not_log_into_a_run | KEEP? | guard of a real bug (d86aa46 "the call log stays out of the tests"); doubt: it checks one constant |
| test_a_bump_reports_the_health_lost_of_the_step | KEEP | rule with more than one outcome (health_lost only with damage, includes the bump) |
| test_dm_play_does_not_import_mcp | REMOVE | wiring (import graph) |
| test_load_and_new_game_after_a_party_death | KEEP | state machine (game over refuses save and wait; load and new_game recover) |
| test_dm_play_logs_each_call | REMOVE | logging |
| test_dm_play_reports_an_unreachable_game | REMOVE | CLI error output and logging |
| test_a_free_record_never_enters_a_square_list | KEEP | regression of a real bug (5d78012) |
| test_unlinking_a_freed_last_thing_empties_the_square | MERGE | into test_a_free_record_never_enters_a_square_list: same bug 5d78012, one test per bug |
| test_torch_holder_is_not_a_lock | KEEP | rule with more than one outcome (sensor type gives torch_holder, not lock) |
| test_brief_names_a_torch_holder | MERGE | into test_brief_names_a_lock: one table of square_text cases |
| test_a_lock_names_its_picture | KEEP | data format (lock picture name) |
| test_a_wall_text_has_no_leading_newline | KEEP | regression of a real bug (0ec43c4) |
| test_the_view_has_no_alcove_without_a_wall_ahead | KEEP | regression of a real bug (b4451f8) |
| test_a_wall_decoration_names_its_picture | MERGE | into test_a_lock_names_its_picture: same check of a front_face name, parametrize the place |
| test_a_border_wall_outside_the_map_shows_its_fountain | KEEP | regression of a real bug (c81d887 "border walls outside the map show their faces") |
| test_the_view_lists_no_side_square_that_the_screen_hides | KEEP | rule with more than one outcome (hidden side square in view and map) |
| test_brief_skips_a_side_square_that_the_view_leaves_out | KEEP | regression of a real bug (6a837ae) |
| test_brief_names_a_lock | KEEP | text format with more than one outcome (named lock, number fallback, ornament) |
| test_view_and_map_show_the_pressure_pads_of_map_1 | KEEP | data format (floor_ornament, map legend), more than one outcome |
| test_a_plate_without_an_ornament_stays_hidden | KEEP | rule with more than one outcome (a plate without an ornament is not shown) |
| test_two_requests_in_one_write | KEEP | parser (line framing of the protocol) |
| test_client_gone_mid_line | KEEP | parser state (partial line of a dropped client) |

### test_csb_adventure.py

| test | verdict | reason |
|---|---|---|
| test_merge_equals_the_python_merge | KEEP | reference check (dmtools csb_mini merge, CEDT reader) |
| test_prison_save_continues_in_the_adventure | KEEP | state machine (prison save makes the adventure, backup, plain save outside) |
| test_merge_refuses_an_empty_party | KEEP | rule with more than one outcome (refusal reason) |
| test_merge_refuses_equal_first_names | KEEP | rule with more than one outcome (refusal reason) |
| test_resume_loads_csbgame | KEEP | rule with more than one outcome (found, missing) |
| test_save_and_play_saves_and_goes_on | KEEP | dialog state machine |
| test_prison_save_and_quit_makes_the_adventure_and_ends_the_game | KEEP | dialog state machine (prison branch) |
| test_save_and_quit_saves_and_ends_the_game | KEEP | dialog state machine (adventure branch) |
| test_cancel_and_refused_merge_go_back_to_the_game | KEEP | dialog state machine (cancel, refused) |
| test_advanced_save_keeps_the_automap_block | KEEP | data format (CSB save with the AMAP block) |
| test_control_new_game_save_and_load | REMOVE | wiring of the socket commands; the rules are covered by test_merge_refuses_an_empty_party and test_prison_save_continues_in_the_adventure |
| test_turnbased_csb_starts_the_prison | REMOVE | CLI arguments, smoke run |

### test_csb_save.py

| test | verdict | reason |
|---|---|---|
| test_mini_decodes_like_csb_mini | KEEP | reference check (dmtools csb_mini decoder) |
| test_mini_loads_the_csb_start | KEEP | data format (start position, map count, group count from MINI.DAT) |
| test_loaded_game_saves_and_loads_back | KEEP | data format round trip (CSB layout, magic map bytes) |
| test_new_prison_game_saves_and_loads_back | KEEP | data format round trip of the prison (group count 60) |
| test_original_reader_accepts_the_file | KEEP | reference check (CEDT reader) |
| test_damaged_champion_block_is_rejected | KEEP | data format (CSB checks the block 2 checksum) |

### test_entrance.py

| test | verdict | reason |
|---|---|---|
| test_doors_open_in_64_steps_of_2_frames | REMOVE | UI animation, no reference comparison |
| test_no_frames_no_doors | REMOVE | UI and scaffold (headless path) |
| test_pressed_button_shows_for_20_frames | REMOVE | UI and layout |
| test_ftl_logo_lasts_as_its_sound | KEEP? | reads the SWSH.FTL palette script (a data format); doubt: the asserts are on screen output (UI) |

### test_gameloop.py

| test | verdict | reason |
|---|---|---|
| test_new_game_recruit_take_and_store | REMOVE | covered by differential test_recruit.py::test_recruit_first_mirror_champion and golden test_grab_and_drop; the rest is click wiring |
| test_champion_dies_with_inventory_open | KEEP | regression of a real bug (2f9cc2c "fix champion death crash") |
| test_game_runs_headless_with_auto_enter | REMOVE | smoke run (scaffold); also duplicates test_port.py::test_smoke_run |
| test_freeze_covers_open_inventory | KEEP? | regression of a real bug (9df44c1); doubt: UI pixel check |
| test_party_light_sets_the_darkness_of_view_and_rules | KEEP | regression of a real bug (df13d49 "set the light palette index every tick for the rules") |
| test_hand_object_name_follows_the_icon | KEEP? | regression of a real bug (8c18111); doubt: UI pixel check |
| test_pointer_over_the_status_box_hands | KEEP? | regression of a real bug (5312866), rule with two outcomes from KAOS 0x108530; doubt: pointer UI |
| test_a_party_that_dies_in_one_tick_shows_the_spell_area_of_the_last_caster | KEEP | regression of a real bug (dc50091) |

### test_modes.py

| test | verdict | reason |
|---|---|---|
| test_vanilla_is_the_default_and_the_advanced_keys_do_nothing | KEEP | rule with more than one outcome (mode gates the keys) |
| test_letter_keys_move_and_turn_outside_vanilla | REMOVE | data table (letter to key map); the vanilla gate is in test_vanilla_is_the_default_and_the_advanced_keys_do_nothing |
| test_quick_save_and_quick_load | KEEP | state machine (no save, saved, loaded, damaged) |
| test_bad_mode_stops_the_program | REMOVE | CLI arguments; also covered by test_options.py::test_errors (--mode fast) |
| test_automap_stops_the_game_time_and_draws_the_map | KEEP | rule with more than one outcome (time stops while the automap is open) |
| test_a_drag_scrolls_the_automap | REMOVE | UI input and layout |
| test_automap_opens_only_in_the_dungeon_view | KEEP | state machine (automap gated by the inventory) |
| test_quick_keys_work_only_in_the_dungeon_view | KEEP | rule with more than one outcome (inventory, frame wait, dungeon view) |
| test_automap_shows_the_position_readout | REMOVE | UI and layout |
| test_escape_rule | KEEP | state machine per mode |
| test_tab_closes_the_inventory | KEEP | rule with more than one outcome per mode |
| test_save_and_load_keep_the_automap | KEEP | data format (AMAP block appended, vanilla reads both, damaged block ignored) |
| test_render_advanced_panels | REMOVE | UI and layout (icons, meters, runes) |
| test_turnbased_program_answers_get_state_and_act | KEEP? | the game waits for a turn; doubt: a process smoke run, the turn rule may be covered by the in-library control tests |
| test_turnbased_load_sent_at_once_runs_the_game | KEEP | regression of a real bug (b25f974) |
| test_smooth_projectile_flight | KEEP? | the drawn motion uses no game random numbers (fidelity rule); doubt: the rest is UI |
| test_empty_hand_at_a_fountain | REMOVE | covered by golden test_golden_rules.py::test_drink_at_a_fountain (same place, same asserts) |
| test_cheat_keys_work_only_on_the_advanced_automap | KEEP | rule with more than one outcome per mode |
| test_cheat_light_draws_the_bright_palette | KEEP | rule with more than one outcome (bright palette, rules keep the darkness) |
| test_cheat_immortal_keeps_1_health | KEEP | rule (damage clamped at 1 health) |
| test_cheat_food_keeps_food_and_water | KEEP | rule with more than one outcome (cheat on, off) |
| test_party_death_during_a_turn_reports_game_over | REMOVE | covered by test_control.py::test_load_and_new_game_after_a_party_death |
| test_turnbased_save_goes_to_the_saves_folder | REMOVE | CLI arguments (--saves) |

### test_music.py

| test | verdict | reason |
|---|---|---|
| test_csb_square_table_matches_the_research | KEEP? | check against the research extract (music_map.json); doubt: it is a data table |
| test_music_keys_work_only_on_the_advanced_automap | KEEP | rule with more than one outcome per mode |
| test_fm_towns_dm_plays_the_level_track | KEEP | rule with more than one outcome (level track) |
| test_track_plays_one_time | KEEP | state machine (an ended track does not start again) |
| test_music_stop_after_audio_stop_all_stops_the_track_and_keeps_the_soundtrack | KEEP? | probable regression of the audio refactor (a45ef4f); doubt: one outcome, no bug record |
| test_music_mix_clips | KEEP | rule with more than one outcome (clip, no wrap) |
| test_a_loaded_game_selects_original | REMOVE | one outcome (reset on load), no bug record |
| test_super_nes_trigger_square_starts_its_track | KEEP | state machine (trigger square, no restart without a step) |
| test_super_nes_does_nothing_in_csb | MERGE | into test_super_nes_trigger_square_starts_its_track: the CSB outcome of the same rule |
| test_fm_towns_csb_zone_track_waits_for_the_current_track | KEEP | state machine (requested track waits, level change stops) |
| test_end_of_the_game_track | KEEP | rule with more than one outcome (adventure, soundtrack, won) |

### test_options.py

| test | verdict | reason |
|---|---|---|
| test_no_options | REMOVE | CLI arguments |
| test_values | REMOVE | CLI arguments |
| test_errors | KEEP? | a parser with several error outcomes; doubt: the policy lists CLI arguments under "no tests" |
| test_help | REMOVE | CLI arguments |

### test_panels.py

| test | verdict | reason |
|---|---|---|
| test_object_text_matches_original | KEEP | reference check against the original (emulator) |
| test_chest_open_swap_close | KEEP | rule with more than one outcome (chest content order after a swap and a close) |
| test_render_scroll_and_eye | REMOVE | UI; the text part is covered by test_object_text_matches_original |
| test_eat_food_redraws_the_food_bar | KEEP? | regression of a real bug (f86a62e); doubt: UI redraw check |

### test_png.py

| test | verdict | reason |
|---|---|---|
| test_png_of_the_screen | KEEP | data format (PNG chunks, CRC, zlib rows) |
| test_png_to_a_missing_folder | MERGE | into test_png_of_the_screen: the failed-write outcome of the same writer |

### test_port_data.py

| test | verdict | reason |
|---|---|---|
| test_ftl_image_matches_python | KEEP | data format, checked against the dmtools FTL loader |
| test_ftl_bad_checksum | KEEP | data format (checksum rejection) |
| test_ftl_huge_region | KEEP | data format (bounds of a region) |
| test_graphics_dat_rejects_bad_files | KEEP | data format (bad Graphics.DAT) |
| test_kaos_accessors | KEEP? | big-endian reads of the image (data format); doubt: thin accessors, indirectly used by many differential tests |
| test_graphics_dat_items | KEEP | data format (item table of Graphics.DAT) |
| test_expand_graphic_matches_original | KEEP | reference check against the original (emulator) |
| test_show_smoke_run | REMOVE | smoke run, CLI arguments |

### test_port.py

| test | verdict | reason |
|---|---|---|
| test_palette | REMOVE | UI (color conversion the user sees at once), three constants |
| test_mouse_mapping | KEEP | rule with more than one outcome (scale, letterbox, clamp) |
| test_smoke_run | REMOVE | smoke run (scaffold) |

### test_request.py

| test | verdict | reason |
|---|---|---|
| test_get_string | KEEP | parser |
| test_get_string_cuts_to_size | KEEP | parser (buffer limit) |
| test_get_int | KEEP | parser |
| test_list_of_names_and_objects | KEEP | parser |
| test_bad_item | KEEP | parser (bad input) |
| test_item_too_long | MERGE | into test_bad_item: one more bad-item case (size parameter) |

### test_save.py

| test | verdict | reason |
|---|---|---|
| test_save_and_load_round_trip | KEEP | data format round trip (with the leader load quirk) |
| test_save_keeps_a_backup | KEEP | rule with more than one outcome (backup on the second save, load falls back to it) |
| test_portraits_survive_save_and_load | KEEP | regression of a real bug (6045e77 "each champion keeps its own portrait bitmap") |
| test_load_fails_gracefully | KEEP | data format (missing, damaged, good) |
| test_dm_load_raises_the_active_group_count_to_110 | KEEP | rule (DM raises the group count on load) |
| test_original_load_routines_accept_the_file | KEEP | reference check against the original (emulator) |
| test_save_command_runs_the_dialog | KEEP | dialog state machine (save, cancel, not frozen) |
| test_party_death_returns_to_the_entrance_headless | KEEP | rule with more than one outcome (headless death goes to the entrance) |
| test_party_death_restart_loads_the_saved_game | KEEP | state machine (DM endgame RESTART loads the save) |
| test_fuse_sequence_ends_the_game | REMOVE | covered by golden test_golden_rules.py::test_fuse_sequence |
| test_new_game_after_sleep_runs_at_normal_speed | KEEP | regression of a real bug (2f9cc2c "state reset") |
| test_endgame_quit_button_shows_the_credits_then_the_entrance | KEEP | state machine (QUIT, credits, entrance) |

### test_scale.py

| test | verdict | reason |
|---|---|---|
| test_scale | KEEP | rule with more than one outcome (scale2x/scale3x edge cases) |

### test_select.py

| test | verdict | reason |
|---|---|---|
| test_adventure_find | REMOVE | data table lookup |
| test_default_dirs | REMOVE | constants |
| test_save_names | REMOVE | constants; the names are also checked by test_save_writes_csb_name |
| test_csb_present | MERGE | into test_csb_present_needs_every_file: add the empty and the full folder as parameters |
| test_csb_present_needs_every_file | KEEP | rule with more than one outcome (each missing file) |
| test_csb_present_in_game_csb | REMOVE | scaffold (real data folder); the rule is covered by test_csb_present_needs_every_file |
| test_save_writes_csb_name | KEEP | rule with more than one outcome (adventure picks the save name, backup) |
| test_start_no_flags | KEEP | state machine (first screen from the flags) |
| test_start_both_flags | KEEP | state machine |
| test_start_adventure_only | KEEP | state machine |
| test_start_mode_only_means_dm | KEEP | state machine |
| test_return_selects_dm | KEEP | state machine |
| test_arrows_select_csb | KEEP | state machine |
| test_cursor_stops_at_the_list_ends | KEEP | state machine (cursor clamp) |
| test_mode_screen_values | KEEP | state machine (mode and features per item) |
| test_escape_back | KEEP | state machine |
| test_escape_on_the_csb_screen | KEEP | state machine |
| test_csb_disabled | KEEP | state machine (disabled item) |
| test_click_activates | KEEP | state machine (click hit and miss) |
| test_csb_screen_choices | KEEP | state machine (CSB screen choices) |
| test_csb_screen_again_with_a_message | REMOVE | UI (message line colors) |
| test_done_ignores_keys | KEEP | state machine (final state) |
| test_draw_items | REMOVE | UI and layout |
| test_draw_disabled_color | REMOVE | UI and layout |
| test_font_from_csb_graphics | REMOVE | UI and layout |
| test_invalid_adventure_exits | REMOVE | CLI arguments |
| test_load_failure_leaves_no_control_socket | KEEP? | cleanup rule on a failed start; doubt: process-level CLI test, no bug record |
| test_csb_missing_exits | REMOVE | CLI arguments |
| test_flags_skip_screens | REMOVE | CLI arguments; the screen rule is covered by test_start_both_flags and test_start_mode_only_means_dm |
| test_selection_screen_runs | REMOVE | smoke run |
| test_csb_screen_runs | REMOVE | smoke run |
| test_restart_goes_back_to_the_first_screen | KEEP | state machine |
| test_restart_with_every_screen_skipped | KEEP | state machine |

### test_ui.py

| test | verdict | reason |
|---|---|---|
| test_palette_split | KEEP? | rule with more than one outcome (palette per screen row); doubt: display layer, the user sees it |
| test_zone_box_matches_original | KEEP | reference check against the original (emulator) |
| test_mouse_commands_match_original | KEEP | reference check against the original (emulator) |
| test_render_empty_party | REMOVE | UI and layout |
| test_render_party | REMOVE | UI; covered by golden test_golden_screens.py::test_status_boxes |
| test_render_action_menu | REMOVE | UI; covered by golden test_golden_rules.py::test_action_list and test_golden_screens.py::test_action_area |
| test_render_inventory | REMOVE | UI; covered by golden test_golden_screens.py::test_inventory_panel |

### test_view.py

| test | verdict | reason |
|---|---|---|
| test_blit_transparent_and_flip | REMOVE | UI drawing primitive; covered by golden test_golden_view.py::test_view_matches_the_golden_snapshot |
| test_shrink_matches_original | KEEP | reference check against the original (emulator) |
| test_zones_match_original | KEEP | reference check against the original (emulator) |
| test_render_view | REMOVE | UI; covered by golden test_view_matches_the_golden_snapshot (every view of every map) |
| test_masked_fill_matches_original | KEEP | reference check against the original (emulator) |
| test_render_things | REMOVE | UI; the views without explosions are covered by golden test_view_matches_the_golden_snapshot |

## Audit of tests/differential

Tests: 165. KEEP: 114, REMOVE: 23, MERGE: 2, KEEP?: 26.

| file | KEEP | REMOVE | MERGE | KEEP? |
|---|---|---|---|---|
| test_champion.py | 11 | 0 | 0 | 2 |
| test_creatures.py | 13 | 0 | 0 | 2 |
| test_csb_ending.py | 13 | 4 | 0 | 3 |
| test_csb_engine.py | 7 | 4 | 0 | 1 |
| test_csb_magic_map.py | 8 | 7 | 0 | 13 |
| test_csb_mini.py | 5 | 0 | 1 | 0 |
| test_csb_rules.py | 4 | 0 | 1 | 0 |
| test_csb_text.py | 4 | 1 | 0 | 0 |
| test_dungeon.py | 8 | 1 | 0 | 1 |
| test_game.py | 6 | 1 | 0 | 0 |
| test_move.py | 11 | 1 | 0 | 0 |
| test_recruit.py | 3 | 0 | 0 | 1 |
| test_savefile.py | 6 | 0 | 0 | 0 |
| test_sound.py | 6 | 0 | 0 | 0 |
| test_spells.py | 2 | 0 | 0 | 0 |
| test_tables.py | 3 | 3 | 0 | 2 |
| test_text.py | 4 | 1 | 0 | 1 |

### test_champion.py

| test | verdict | reason |
|---|---|---|
| test_pack_layout | KEEP | parser or data format: champion record byte layout and round trip |
| test_object_weight_and_icon_match_original | KEEP | reference check (original program) |
| test_champion_values_match_original | KEEP | reference check (original program) |
| test_damage_matches_original | KEEP | reference check (original program) |
| test_experience_matches_original | KEEP | reference check (original program) |
| test_time_effects_match_original | KEEP | reference check (original program) |
| test_recruit_first_mirror_champion | KEEP | rule with more than one outcome, no reference check covers it (add candidate, rebirth, possessions, mirror disabled) |
| test_recruit_copies_the_mirror_portrait | KEEP | parser or data format: portrait copied from graphic 26 |
| test_eat_food | KEEP | rule with more than one outcome, no reference check covers it (food value, load, record freed) |
| test_damage_until_death | KEEP | rule with more than one outcome, no reference check covers it (death drops slots and bones) |
| test_death_sets_redraw_flags | KEEP? | caster switch is a rule; the redraw/status-box flags are UI wiring; check overlap with port/test_gameloop.py::test_a_party_that_dies_in_one_tick_shows_the_spell_area_of_the_last_caster |
| test_death_marks_the_champion_for_release | KEEP? | one flag set for the UI (wiring); keep only if it is a regression of a real bug |
| test_party_light_matches_original | KEEP | reference check (original program) |

### test_creatures.py

| test | verdict | reason |
|---|---|---|
| test_directions_to_destination_match_original | KEEP | reference check (original program) |
| test_poison_resistance_matches_original | KEEP | reference check (original program) |
| test_champion_damage_matches_original | KEEP | reference check (original program) |
| test_projectile_impact_attack_matches_original | KEEP | reference check (original program) |
| test_object_for_launcher_matches_original | KEEP | reference check (original program) |
| test_group_activates_and_approaches_party | KEEP? | scenario of the group AI on the port only; no reference check covers it, but it checks only a coarse result |
| test_group_attacks_adjacent_party | KEEP? | scenario on the port only; partly overlaps golden/test_golden_rules.py::test_turn_to_attacker |
| test_creature_dies_and_drops_possessions | KEEP | rule with more than one outcome, no reference check covers it (survive vs die, drops, smoke ends) |
| test_fireball_flies_and_explodes_on_wall | KEEP | rule with more than one outcome, no reference check covers it (projectile path, explosion) |
| test_light_spell_adds_light_event_and_experience | KEEP | rule with more than one outcome, no reference check covers it |
| test_potion_spell_fills_empty_flask | KEEP | rule with more than one outcome, no reference check covers it (no flask vs flask) |
| test_meaningless_spell_fails | KEEP | rule with more than one outcome, no reference check covers it (failure outcome of a cast) |
| test_spell_level_and_experience_rule | KEEP | rule with more than one outcome, no reference check covers it (required level vs one level missing) |
| test_party_shield_adds_defense_and_event | KEEP | rule with more than one outcome, no reference check covers it |
| test_fireball_spell_shoots_projectile | KEEP | rule with more than one outcome, no reference check covers it |

### test_csb_ending.py

| test | verdict | reason |
|---|---|---|
| test_chunk_walk_consumes_the_file | KEEP? | truncated-file rejection is a parser check (keep); the tag counts are constants of ENDA.DAT (remove those asserts) |
| test_first_frame_decodes | REMOVE | covered by test_every_frame_matches_an_independent_decode (first frame, delay, palette) |
| test_frame_sequence_and_timing | REMOVE | covered by test_every_frame_matches_an_independent_decode (all delays) and test_nested_loops (loop rule) |
| test_every_frame_matches_an_independent_decode | KEEP | parser or data format: ANIM format against an independent decoder |
| test_palette_changes_with_the_second_full_frame | REMOVE | covered by test_every_frame_matches_an_independent_decode (palette per frame) and test_palette_waits_for_the_next_frame |
| test_anim_play_holds_each_frame_for_its_delay | KEEP | rule with more than one outcome, no reference check covers it (frame hold timing) |
| test_dm_title_plays_and_stops_on_input | KEEP | rule with more than one outcome, no reference check covers it (BR chunk: input stops TITL) |
| test_enda_ignores_input | KEEP | rule with more than one outcome, no reference check covers it (no BR: input ignored) |
| test_play_sample_volumes_and_period | KEEP | parser or data format: SO chunk volumes and period in the mix |
| test_nested_loops | KEEP | rule with more than one outcome, no reference check covers it (state machine of FO/NE/BN) |
| test_palette_waits_for_the_next_frame | KEEP | rule with more than one outcome, no reference check covers it |
| test_sensor_18_starts_the_ending | KEEP | rule with more than one outcome, no reference check covers it (won, return to entrance, first frame) |
| test_sensor_18_in_a_tick_starts_the_ending | REMOVE | covered by test_sensor_18_wins_over_a_party_death_in_the_same_tick (same tick path, asserts won) |
| test_sensor_18_wins_over_a_party_death_in_the_same_tick | KEEP | rule with more than one outcome, no reference check covers it |
| test_an_event_after_sensor_18_in_the_same_tick_sees_the_won_game | KEEP | rule with more than one outcome, no reference check covers it |
| test_sensor_18_with_frames_plays_at_once_and_waits_for_input | KEEP | rule with more than one outcome, no reference check covers it |
| test_death_shows_restart_only_and_restart_loads_the_save | KEEP? | restart loads the save is a rule (keep); the button pixel checks are UI layout |
| test_csb_intro_frames_and_music | KEEP? | MI chunk starts music on both sides is a rule; the frame count 851 is a constant of the file |
| test_csb_intro_sample_files_play_beside_the_music | KEEP | rule with more than one outcome, no reference check covers it (SO sample beside the music) |
| test_csb_intro_stops_on_input_and_fades_the_music | KEEP | rule with more than one outcome, no reference check covers it (stop and fade timing) |

### test_csb_engine.py

| test | verdict | reason |
|---|---|---|
| test_failed_text_build_publishes_no_image | KEEP | regression of a real bug (the image stayed loaded) |
| test_copied_tables_equal_appb | KEEP | reference check (original data file) (APPB.FTL) |
| test_equal_tables_match_dm | KEEP | reference check (original data file) (KAOS.FTL) |
| test_documented_differences | REMOVE | constants and data tables: hard-coded values of the original files; the port copy is covered by test_copied_tables_equal_appb |
| test_converted_action_sets | KEEP | reference check (original data file): converted layout |
| test_converted_dialog_pointers | KEEP | reference check (original data file) |
| test_csb_spells | KEEP | reference check (original data file): CSB runes find spells 25-28 |
| test_dm_spells | KEEP | rule with more than one outcome, no reference check covers it (DM outcome of the spell count) |
| test_layout_zones_equal_but_endgame_buttons | KEEP? | compares the layout data of two original files (reference), but the subject is UI layout |
| test_prison_new_game_runs_and_renders | REMOVE | wiring, scaffold, CLI arguments (smoke run); the prison start is covered by port/test_csb_adventure.py (fixtures, test_turnbased_csb_starts_the_prison) |
| test_view_shows_prison | REMOVE | wiring, scaffold, CLI arguments: --adventure/--view exit code |
| test_view_loads_csb_from_data_folder | REMOVE | wiring, scaffold, CLI arguments: --data folder and error message |

### test_csb_magic_map.py

| test | verdict | reason |
|---|---|---|
| test_magic_map_scrolls_in_the_dungeon | REMOVE | constants and data tables: scroll positions in MINI.DAT and spell kinds; the other tests fail anyway when they change |
| test_spell_needs_magic_map_in_action_hand | KEEP | rule with more than one outcome, no reference check covers it |
| test_spells_with_magic_map | KEEP | rule with more than one outcome, no reference check covers it (durations per spell and power) |
| test_events_end_the_map_spells | KEEP | rule with more than one outcome, no reference check covers it |
| test_panel_draws_the_map_grid | REMOVE | covered by test_only_the_exact_text_is_a_magic_map (map shown vs not shown); also UI and layout (the user tests them manually) |
| test_ku_shows_the_creature_ahead | REMOVE | covered by test_ku_creature_tile |
| test_dain_shows_an_explosion | REMOVE | covered by test_dain_explosion_tile |
| test_ros_shows_an_invisible_pit | REMOVE | covered by test_ros_pit_and_fake_wall_tiles |
| test_runes_light_with_the_spells | REMOVE | covered by test_rune_of_each_spell (NETA rune) and test_neta_keeps_the_map |
| test_neta_keeps_the_map | KEEP | rule with more than one outcome, no reference check covers it |
| test_panel_draws_again_each_tick | KEEP? | redraw per tick is UI; keep only if it is a regression of a real bug |
| test_creaname_becomes_the_creature_name | KEEP | rule with more than one outcome, no reference check covers it (no group vs group) |
| test_dm_scroll_panel_unchanged | KEEP | rule with more than one outcome, no reference check covers it (DM outcome) |
| test_map_spells_of_another_champion | KEEP | rule with more than one outcome, no reference check covers it |
| test_panel_shows_the_magic_map_graphic | REMOVE | UI and layout (the user tests them manually) |
| test_rune_of_each_spell | KEEP? | rune-to-spell mapping is a rule, but the check is UI drawing |
| test_ku_creature_tile | KEEP? | tile choice is a rule with outcomes (blink); check is UI drawing |
| test_dain_explosion_tile | KEEP? | tile choice is a rule; check is UI drawing |
| test_ros_pit_and_fake_wall_tiles | KEEP? | tile choice is a rule with several outcomes; check is UI drawing |
| test_party_and_footprint_borders | KEEP? | footprints only with DAIN is a rule; borders are UI layout |
| test_eye_keeps_the_leader_map | KEEP? | UI redraw through the eye; keep only if it is a regression of a real bug |
| test_only_the_exact_text_is_a_magic_map | KEEP | rule with more than one outcome, no reference check covers it (text parsing, 4 outcomes) |
| test_door_tiles | KEEP? | tile choice is a rule with several outcomes (state, button, side); check is UI drawing |
| test_stairs_tiles | KEEP? | tile choice is a rule with several outcomes; check is UI drawing |
| test_fireball_projectile_tile | KEEP? | tile choice is a rule; check is UI drawing |
| test_zokathra_tile | KEEP? | tile choice is a rule (wall vs floor); check is UI drawing |
| test_floor_ornament_tile | KEEP? | tile choice is a rule; check is UI drawing |
| test_wall_ornament_tiles | KEEP? | tile choice is a rule with several outcomes; check is UI drawing |

### test_csb_mini.py

| test | verdict | reason |
|---|---|---|
| test_decode_matches_original | KEEP | reference check (original program) (CEDT.FTL) |
| test_original_rejects_changed_byte | MERGE | into test_csb_mini_rejects_changed_byte: checks the reference harness, not the code; one test for both rejections |
| test_csb_mini_rejects_changed_byte | KEEP | parser or data format: checksum rejection |
| test_merge_header | KEEP | reference check (original program) |
| test_merge_champions | KEEP | reference check (original program) |
| test_merge_globals | KEEP | reference check (original program) |

### test_csb_rules.py

| test | verdict | reason |
|---|---|---|
| test_removed_actions | KEEP | rule with more than one outcome, no reference check covers it (DM vs CSB) |
| test_action_without_branch_draws_only_the_stamina_bit | MERGE | into test_removed_actions: it only checks the premise of that test |
| test_sensor_event_priority | KEEP | rule with more than one outcome, no reference check covers it (DM vs CSB) |
| test_version_checker_limit | KEEP | rule with more than one outcome, no reference check covers it (limit 36 vs 31) |
| test_reincarnation | KEEP | rule with more than one outcome, no reference check covers it (DM vs CSB) |

### test_csb_text.py

| test | verdict | reason |
|---|---|---|
| test_strings | KEEP | parser or data format: strings decoded from APPB.FTL/item 694 against the documented original text |
| test_string_index_tables | KEEP | parser or data format: index tables resolve to the right strings |
| test_action_and_object_names | KEEP | parser or data format |
| test_texts_against_dm | KEEP | reference check (original data file): DM vs CSB text sets |
| test_prison_recruit_shows_csb_text | REMOVE | wiring, scaffold, CLI arguments: message path; the strings are covered by test_strings, a CSB message through the path by test_csb_magic_map.py::test_spell_needs_magic_map_in_action_hand |

### test_dungeon.py

| test | verdict | reason |
|---|---|---|
| test_dungeon_matches_dmcsb | KEEP | reference check (dmcsb parse of the original file) |
| test_text_matches_dmcsb | REMOVE | covered by test_decode_text_matches_original (every text and mode against the original code) |
| test_decode_text_matches_original | KEEP | reference check (original program) |
| test_decompress_matches_original | KEEP | reference check (original program) |
| test_uncompressed_file | KEEP | parser or data format: plain file and checksum |
| test_bad_header_fails | KEEP | parser or data format: header validation |
| test_corrupt_links_fail | KEEP | parser or data format: link validation |
| test_truncated_file_fails | KEEP | parser or data format |
| test_bad_text_offset_and_small_buffer | KEEP | parser or data format: text decode edge cases |
| test_door_vertical_bit | KEEP? | checks a raw bit of the loaded bytes against dmcsb; the loader copy is covered by test_dungeon_matches_dmcsb, so it mostly checks the bit constant |

### test_game.py

| test | verdict | reason |
|---|---|---|
| test_random_matches_original | KEEP | reference check (original program) |
| test_math_matches_original | KEEP | reference check (original program) |
| test_relative_movement_matches_original | KEEP | reference check (original program) |
| test_champion_helpers_match_original | KEEP | reference check (original program) |
| test_timeline_matches_original | KEEP | reference check (original program) |
| test_timeline_order | REMOVE | covered by test_timeline_matches_original (1500 random add/extract steps against the original) |
| test_game_new | KEEP | reference check (dmcsb parse of the original file): start location; new game state |

### test_move.py

| test | verdict | reason |
|---|---|---|
| test_start_position | REMOVE | covered by test_game.py::test_game_new (start location against the dungeon header) |
| test_move_into_wall_stays | KEEP | rule with more than one outcome, no reference check covers it (blocked move) |
| test_move_along_corridor | KEEP | rule with more than one outcome, no reference check covers it |
| test_turning | KEEP | rule with more than one outcome, no reference check covers it (direction and cells) |
| test_stairs_change_map | KEEP | rule with more than one outcome, no reference check covers it |
| test_floor_sensor_opens_door | KEEP | rule with more than one outcome, no reference check covers it (sensor, door state sequence) |
| test_door_event_toggles_door | KEEP | rule with more than one outcome, no reference check covers it |
| test_teleporter_moves_party | KEEP | rule with more than one outcome, no reference check covers it |
| test_pit_drops_party | KEEP | rule with more than one outcome, no reference check covers it |
| test_turn_on_stairs_takes_stairs | KEEP | rule with more than one outcome, no reference check covers it |
| test_square_queries_match_original | KEEP | reference check (original program) |
| test_link_and_unlink_keep_lists | KEEP | rule with more than one outcome, no reference check covers it (thing list state) |

### test_recruit.py

| test | verdict | reason |
|---|---|---|
| test_mirrors_match_dmcsb | KEEP | reference check (dmcsb parse of the original file) |
| test_decoded_value_matches_original | KEEP | reference check (original program) |
| test_parse_text_edges | KEEP | parser or data format: recruit text edge cases |
| test_portrait_origin | KEEP? | one hard-coded value of a simple formula (close to a constant); the portrait copy in test_champion.py uses the function, not a check of it |

### test_savefile.py

| test | verdict | reason |
|---|---|---|
| test_round_trip | KEEP | parser or data format |
| test_encode_matches_original | KEEP | reference check (original program) |
| test_encode_rejects_wrong_sizes | KEEP | parser or data format |
| test_malformed_fails | KEEP | parser or data format |
| test_huge_counts_fail | KEEP | parser or data format |
| test_champion_checksum_is_not_checked | KEEP | parser or data format: kept quirk of the original |

### test_sound.py

| test | verdict | reason |
|---|---|---|
| test_sample_decode | KEEP | parser or data format: sound table and sample items |
| test_mix_single_voice | KEEP | rule with more than one outcome, no reference check covers it (mixer against a model) |
| test_mix_replaces_quieter_pair | KEEP | rule with more than one outcome, no reference check covers it (voice replacement) |
| test_compare_loudness_matches_original | KEEP | reference check (original program) |
| test_get_volume_matches_original | KEEP | reference check (original program) |
| test_request_queue_matches_original | KEEP | reference check (original program) |

### test_spells.py

| test | verdict | reason |
|---|---|---|
| test_spell_find_matches_original | KEEP | reference check (original program) |
| test_spell_fields_and_rules | KEEP | rule with more than one outcome (mana cost, draw fails, failure experience) without a reference check; the field asserts are covered by test_tables.py::test_table_matches_big_endian_image (drop them) |

### test_tables.py

| test | verdict | reason |
|---|---|---|
| test_accessor_rejects_out_of_range | REMOVE | constants and data tables: bounds against hard-coded counts (todo candidate) |
| test_symbol_mana_cost_rejects_out_of_range | REMOVE | constants and data tables: bounds against hard-coded sizes (todo candidate) |
| test_table_matches_big_endian_image | KEEP | reference check (original data file) |
| test_action_info_matches_per_action_tables | KEEP | reference check (original data file) |
| test_symbol_base_mana_cost_matches_image | KEEP | reference check (original data file) |
| test_object_types_match_dmcsb_item_list | KEEP? | second reference (dmcsb) for object_info, whose values test_table_matches_big_endian_image covers; it only confirms the address and the count |
| test_counts_follow_layout | REMOVE | constants and data tables: counts and addresses against hard-coded numbers (todo candidate) |
| test_strides_and_bases_match_decompiled_code | KEEP? | checks the decompiled text, not the port; confirms the test's own strides and bases |

### test_text.py

| test | verdict | reason |
|---|---|---|
| test_text_draw_matches_original[x] | KEEP | reference check (original program) |
| test_text_draw_glyph_pixels | REMOVE | covered by test_text_draw_matches_original; also UI and layout (the user tests them manually) |
| test_text_draw_clips | KEEP? | negative-origin clipping is not in the original check (x >= 0), but it is UI drawing |
| test_message_scroll_and_expiry | KEEP | rule with more than one outcome, no reference check covers it (expiry times, scroll) |
| test_message_word_wrap | KEEP | rule with more than one outcome, no reference check covers it |
| test_message_cursor_clamps_large_column | KEEP | regression of a real bug (int16 overflow of the column) |

### Test audit: tests/golden, tests/standard, tests/tools

Counts (102 entries; a parametrized test is one entry):

| verdict | golden | standard | tools | total |
|---|---|---|---|---|
| KEEP | 23 | 38 | 33 | 94 |
| REMOVE | 1 | 0 | 3 | 4 |
| MERGE | 0 | 0 | 1 | 1 |
| KEEP? | 0 | 1 | 2 | 3 |

Most REMOVE: tests/tools/test_emu.py, test_csb_tools.py, test_ghidra.py (1 each), tests/golden/test_golden_screens.py (1).

### The golden screens decision

`tests/golden/test_golden_screens.py` is a regression net, not a "UI and layout" test. It hashes the full frame
buffer that the drawing code (ported from the original drawing functions, e.g. `status_draw_damage`,
`menus_draw_action_damage`) gives for a fixed state. The owner tests layout by eye, but cannot see a one-pixel
or one-color drift after a refactor; the snapshot can. docs/development.md and docs/architecture.md list the
screens with the other golden snapshots. Verdict: KEEP, except `test_spell_area`: its two hashes are byte-equal
to the first two screen hashes of `test_runes[100]` in rules.json (checked).

Note: the golden snapshots are recorded from the port itself, not from the original. They are reference checks
by the policy line "golden snapshot", but they pin behavior through refactors only; correctness against the
original comes from tests/differential.

### tests/golden/test_golden_protocol.py

| test | verdict | reason |
|---|---|---|
| test_protocol_matches_the_golden_snapshot | KEEP | Reference check (golden snapshot) of the JSON protocol, a data format; also checks no machine path in replies. |

### tests/golden/test_golden_rules.py

| test | verdict | reason |
|---|---|---|
| test_fill_at_a_fountain | KEEP | Golden snapshot of a game rule (flask and waterskin fill). |
| test_drink_at_a_fountain [mode] | KEEP | Golden snapshot; rule with two outcomes (vanilla vs advanced water). |
| test_bones_on_the_vi_altar | KEEP | Golden snapshot of the resurrection rule. |
| test_door_button | KEEP | Golden snapshot of a door state change. |
| test_runes [mana] | KEEP | Golden snapshot; rule with two outcomes (enough / not enough mana). Also covers the spell area screens. |
| test_torch_burns | KEEP | Golden snapshot of a timed rule (torch charges). |
| test_turn_to_attacker | KEEP | Golden snapshot of the champion turn rule. |
| test_action_list | KEEP | Golden snapshot; action list per weapon, charges and skill level (many outcomes). |
| test_freeze_life_and_movement_ticks | KEEP | Golden snapshot of timer rules. |
| test_grab_and_drop | KEEP | Golden snapshot of the grab/drop rule (own square vs square ahead). |
| test_grab_under_a_creature | KEEP | Golden snapshot; rule with three outcomes (own cell, other cell, levitating creature). |
| test_alcove | KEEP | Golden snapshot of the alcove rule. |
| test_imaginary_wall | KEEP | Golden snapshot of the imaginary wall rule. |
| test_movement_blocked | KEEP | Golden snapshot; rule with outcomes (blocked by each blocker, not blocked in other direction). |
| test_fuse_sequence | KEEP | Golden snapshot of the end sequence. |
| test_load_colors | KEEP | Golden snapshot; rule with three outcomes (load thresholds: gray, yellow, red). |
| test_visibility | KEEP | Golden snapshot of the explored-squares rule for all corridor squares, directions and lights. |

### tests/golden/test_golden_save.py

| test | verdict | reason |
|---|---|---|
| test_save_matches_the_golden_snapshot | KEEP | Golden snapshot of the save file, a data format. |

### tests/golden/test_golden_screens.py

| test | verdict | reason |
|---|---|---|
| test_status_boxes | KEEP | Golden snapshot regression net of the status boxes and damage draw (see the decision above). |
| test_inventory_panel | KEEP | Golden snapshot regression net; hash differs from all load hashes of test_load_colors. |
| test_action_area | KEEP | Golden snapshot regression net incl. `menus_draw_action_damage`; hash differs from test_action_list. |
| test_spell_area | REMOVE | Covered by another test: its two screen hashes equal the first two of test_golden_rules.py::test_runes[100]. |

### tests/golden/test_golden_view.py

| test | verdict | reason |
|---|---|---|
| test_view_matches_the_golden_snapshot [m] | KEEP | Golden snapshot of every view of every map. |

### tests/standard/test_standard.py

All are the checks of docs/coding-standard.md; no other check catches them (clang warnings do not), so the
policy line "what a standard check catches" does not remove them: they ARE the standard check.

| test | verdict | reason |
|---|---|---|
| test_no_hex_literals | KEEP | Standard check (named values, hex). |
| test_no_decimal_literals | KEEP | Standard check (named values, decimal); different pattern than hex. |
| test_enum_names_a_result_type | KEEP | Standard check. |
| test_constant_blocks_have_a_comment | KEEP | Standard check. |
| test_no_decompiler_names | KEEP | Standard check. |
| test_raw_record_access_only_in_accessors | KEEP | Standard check. |
| test_each_constant_has_one_definition | KEEP | Standard check. |
| test_headers_state_unique_prefixes | KEEP | Standard check (prefix line, collisions, names). |
| test_no_event_numbers_or_offsets_in_names | KEEP | Standard check on identifiers; test_no_event_numbers_in_comments checks comments, no overlap. |
| test_truth_values_are_bool | KEEP | Standard check. |
| test_function_names_are_short | KEEP | Standard check. |
| test_static_functions_have_no_module_prefix | KEEP | Standard check; the opposite rule of the prefix check, no overlap. |
| test_globals_have_the_g_prefix | KEEP | Standard check. |
| test_no_ordinals_in_names | KEEP | Standard check. |
| test_functions_are_short | KEEP | Standard check. |
| test_number_notation | KEEP | Standard check (notation of constant blocks), not covered by the literal checks. |
| test_no_packed_fields_at_call_sites | KEEP | Standard check. |
| test_byte_order_helpers_only_in_bytes_header | KEEP | Standard check; the helpers are used in ui/, so the check has targets. |
| test_kaos_image_only_in_data | KEEP | Standard check. |
| test_each_image_address_has_one_name | KEEP | Standard check. |
| test_platform_code_only_in_platform | KEEP | Standard check (OS headers, stderr, exit); test_layers checks SDL only, no overlap. |
| test_ui_does_not_write_engine_variables | KEEP | Standard check. |
| test_layers | KEEP | Standard check (layer order, include paths, SDL). |
| test_sources_are_formatted | KEEP? | Duplicates `make check` (Makefile line 95: `clang-format --dry-run --Werror $(C_FILES)`); keep one of the two. The test also pins clang-format 23. |
| test_allowlist_names_documented_reasons | KEEP | Standard check of the allowlist (only exception rules or open lines). |
| test_one_public_header_per_module | KEEP | Standard check. |
| test_header_heads | KEEP | Standard check. |
| test_includes_are_used | KEEP | Standard check; clang -Wall does not catch unused includes. |
| test_function_comments | KEEP | Standard check. |
| test_file_order | KEEP | Standard check. |
| test_one_type_per_parameter_name | KEEP | Standard check. |
| test_results | KEEP | Standard check. |
| test_at_most_five_parameters | KEEP | Standard check. |
| test_exports_have_users | KEEP | Standard check (dead API). |
| test_no_event_numbers_in_comments | KEEP | Standard check on comments. |
| test_no_todo_markers | KEEP | Standard check. |
| test_tests_follow_the_rules | KEEP | Standard check of the test rules (no test imports, no machine path, no clang call). |

### tests/standard/test_docs.py

| test | verdict | reason |
|---|---|---|
| test_doc_links_resolve | KEEP | Repository check of the docs; no linter or other test catches a broken relative link. |

### tests/standard/test_vocabulary.py

| test | verdict | reason |
|---|---|---|
| test_every_protocol_name_is_documented | KEEP | Check of the protocol vocabulary (data format) against the docs; no other check. |

### tests/tools/test_adf.py

| test | verdict | reason |
|---|---|---|
| test_extract_matches_amitools | KEEP | Parser (ADF) checked against a reference (amitools); runs without the disks. |
| test_has_file_root_only_any_case | KEEP | Parser rule with more than one outcome (case, folder, subfolder, missing file). |
| test_extract_rejects_other_files | KEEP | Parser: rejects a bad format. |
| test_unsafe_amiga_names_are_rejected [name] | KEEP | Rule with more than one outcome (safe file names); guards path traversal. |
| test_game_disk_matches_amitools [disk] | KEEP | Parser against a reference on the real disks; the synthetic test does not cover the real file systems. |
| test_install_disks_then_files_pass_the_check | KEEP | Rule with outcomes (installed / skipped) and the file check of the data. |
| test_bad_file_names_a_changed_or_missing_file | KEEP | Rule with more than one outcome (changed, wrong adventure, missing). |

### tests/tools/test_csb_tools.py

| test | verdict | reason |
|---|---|---|
| test_csb_prison_state_through_the_tools | KEEP? | Mostly wiring (program start, socket, mcp_server); mcp_server is covered by tests/port/test_control.py::test_mcp_tools and the CSB start by tests/port/test_csb_adventure.py::test_turnbased_csb_starts_the_prison. Unique: `adventure: csb`, start square (0, 9, 0) in the state JSON and the CSB marker of play.brief. Could shrink to a play.brief unit test. |
| test_dm_state_through_the_tools | REMOVE | Wiring; mcp_server on DM is covered by tests/port/test_control.py::test_mcp_tools; only the absent CSB marker of play.brief is new. |

### tests/tools/test_dmtest_constants.py

| test | verdict | reason |
|---|---|---|
| test_values_of_a_header | KEEP | Parser (C header constants) used by all tests. |
| test_division_truncates_toward_zero_as_in_c | KEEP | Parser rule with more than one outcome (C division vs Python division). |
| test_a_constant_may_use_a_later_one | KEEP | Parser rule (forward reference). |
| test_unsigned_cast_wraps | KEEP | Parser rule (unsigned cast). |

### tests/tools/test_dmtest.py

| test | verdict | reason |
|---|---|---|
| test_each_load_has_its_own_globals | KEEP? | Test scaffold (policy: no test for scaffold), but it guards the rule "each test file owns its state"; a break would show only as order-dependent failures elsewhere. |

### tests/tools/test_emu.py

The emulator runs the original program; its guards keep differential tests from passing on a wrong setup.

| test | verdict | reason |
|---|---|---|
| test_word_args | REMOVE | Covered by test_word_push_order_and_negative (same word passing; add is commutative, so it is weaker). |
| test_word_push_order_and_negative | KEEP | Rule of the call ABI (push order, sign). |
| test_mixed_word_long_args | KEEP | Rule of the call ABI (word and long mixed). |
| test_a5_is_data_base | KEEP | Rule of the harness setup (A5 data base) that every original call depends on. |
| test_registers_reset_between_calls | KEEP | Rule: no state leaks between calls of the reference. |
| test_stale_emu_raises | KEEP | Rule with an error outcome (stale emulator). |
| test_cia_access_raises | KEEP | Rule with an error outcome (CIA access). |
| test_unmapped_access_raises | KEEP | Rule with an error outcome (unmapped memory). |
| test_illegal_instruction_raises | KEEP | Rule with an error outcome (CPU exception). |
| test_custom_chip_access_raises | KEEP | Rule with two outcomes (raises; allowed with allow_hw). |
| test_execbase_read_raises | KEEP | Rule with an error outcome (ExecBase read). |
| test_cycle_budget | KEEP | Rule with an error outcome (endless loop). |
| test_kaos_runtime_mul_div | KEEP | Reference check: the original runtime arithmetic in the emulator against exact integer math. |

### tests/tools/test_extract.py

| test | verdict | reason |
|---|---|---|
| test_extract_writes_files_and_returns_manifest | KEEP | Data format: the manifest (name, size, SHA-256) of the extract tool; not covered by test_adf.py, which uses extract only as the reference. |

### tests/tools/test_ftl.py

| test | verdict | reason |
|---|---|---|
| test_parse_and_load_image | KEEP | Parser (FTL) and loader. |
| test_parse_rejects_bad_checksum | MERGE | Into test_parse_rejects_bad_section_checksum as one more case ("code"); same check shape. |
| test_unpack_r23 | KEEP | Parser of the R# pack format (dictionary codes). |
| test_parse_unpacks_packed_code | KEEP | Parser: packed code through parse. |
| test_parse_rejects_missing_section | KEEP | Parser: bad format. |
| test_load_image_rejects_wrong_data_length | KEEP | Parser/loader: bad format. |
| test_parse_rejects_bad_section_checksum [tag] | KEEP | Parser: bad checksums. |
| test_parse_rejects_too_many_sections | KEEP | Parser: bad format. |

### tests/tools/test_ghidra.py

| test | verdict | reason |
|---|---|---|
| test_params_and_symbols | REMOVE | The project CLAUDE.md says tools/ghidra is not tested; the test is wiring of an external tool (Ghidra script), skipped without Ghidra. Doubt: it checks parameter recovery, a rule; if the owner wants it, fix the CLAUDE.md line instead. |

### tests/tools/test_symbols.py

| test | verdict | reason |
|---|---|---|
| test_csv_is_valid | KEEP | Data format of the symbol table (header, unique addresses and names, identifiers). |
| test_function_rows_are_function_starts | KEEP | Check against a reference (the Ghidra decompilation). |
