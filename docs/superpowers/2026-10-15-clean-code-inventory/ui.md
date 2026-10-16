# Inventory: ui/ writes to engine variables (R8) and square drawers (R9)

Repo state: main @ b3d1852. All line numbers are of that tree. Read-only inventory.

Method: the set of `g_*` names of `port/src/engine/*.h` (107 names), then every write in
`port/src/ui/*.c` (dr_mp3.h skipped) by the R1.7 pattern (`=`, op-`=`, `++`, `--`, after optional
`[..]`, `.f`, `->f`), plus `memset`/`memcpy` on such a variable, plus writes through pointers taken
from engine globals (`Champion *c = &g_champions[..]`, `int16_t *cheat = &g_cheat_*`).
Not engine: `g_adventure` (data/adventure.h:16; select.c:66, :94 write it - out of R1.7 scope),
`g_advanced_features`, `g_action_count`, `g_action_list_minimum_skill_levels`,
`g_action_area_contains_icons` (ui headers).

Kind column: **R** game rule, **F** redraw flag (champion `attributes` CHAMPION_ATTR_* or `g_redraw`),
**S** UI state kept in an engine variable (open inventory, caster, acting champion), **I** start/reset
of a game, **C** consume a request or notice flag of the engine, **T** technical trick.

## 1. R8 - every write

| file:line | variable / field | kind | game rule / purpose |
|---|---|---|---|
| ui/advanced.c:114 | g_saved_game_exists | T | quick save restores the flag that savegame_save_as set (quick save is not a saved game) |
| ui/advanced.c:115 | g_last_save_game_time | T | same, restores save time |
| ui/advanced.c:125 | g_saved_game_exists | T | quick load restores the flag |
| ui/advanced.c:126 | g_last_save_game_time, g_last_load_game_time | T | quick load clears both times |
| ui/advanced.c:137-138 | g_cheat_light / g_cheat_immortal / g_cheat_food via `int16_t *cheat` (pointer write `*cheat = !*cheat`) | R | cheat toggle on the advanced automap |
| ui/command.c:241 | g_magic_caster_champion_index = -1 | S | force redraw idiom: clear, then menus_set_magic_caster(caster) (command_unfreeze_game) |
| ui/command.c:249 | g_inventory_champion_ordinal = 0 | S | force reopen idiom: clear, then command_toggle_inventory(open-1) |
| ui/command.c:296 | g_champions[i].attributes \|= CHAMPION_ATTR_PANEL | F | mouth: food/water panel redraw |
| ui/command.c:319 | g_inventory_champion_ordinal = 0 | S | show_candidate: force open of candidate inventory |
| ui/command.c:333 | g_inventory_champion_ordinal = index + 1 | S | resurrect panel: "the rule clears it; the panel closes here" |
| ui/command.c:341 | g_magic_caster_champion_index = -1 | S | force redraw idiom (click_in_candidate_panel) |
| ui/command.c:346 | g_redraw \|= REDRAW_ACTION_AREA | F | |
| ui/command.c:377 | g_champions[moved].direction | R | icon drag: moved champion faces party direction |
| ui/command.c:378 | g_champions[moved].attributes \|= ICON | F | |
| ui/command.c:384 | g_champions[other].cell | R | icon drag: swap cells |
| ui/command.c:385 | g_champions[other].attributes \|= ICON | F | |
| ui/command.c:388 | g_champions[moved].cell | R | icon drag: move to clicked cell |
| ui/command.c:465 | g_champions[g_leader_index].load += | R | fill at fountain: load changes with the new weight |
| ui/command.c:466 | g_champions[leader].attributes \|= LOAD | F | |
| ui/command.c:515 | g_champions[g_leader_index].water = FOUNTAIN_WATER | R | drink at fountain (leader) |
| ui/command.c:518 | g_champions[i].water = FOUNTAIN_WATER | R | drink at fountain, advanced: every living champion |
| ui/command.c:553 | c->current_mana -= cost (c = &g_champions[caster], :548) | R | rune mana cost |
| ui/command.c:554 | c->attributes \|= STATISTICS | F | |
| ui/command.c:555-556 | c->symbols[step], c->symbols[step+1] | R | add rune |
| ui/command.c:557 | c->symbol_step | R | add rune |
| ui/command.c:568 | c->symbol_step (c = :566) | R | delete rune |
| ui/command.c:569 | c->symbols[step] = 0 | R | delete rune |
| ui/command.c:665 | g_party_is_sleeping = 1 | R | party goes to sleep (go_to_sleep, :660) |
| ui/gameloop.c:263 | g_candidate_champion_ordinal = 0 | I | start_game |
| ui/gameloop.c:264 | g_party_is_sleeping = 0 | I | start_game |
| ui/gameloop.c:265 | g_game_won = 0 | I | start_game (same line: ui g_do_not_draw_fluxcages) |
| ui/gameloop.c:266 | g_cheat_light, g_cheat_immortal, g_cheat_food = 0 | I | start_game (same line: ui g_automap_open) |
| ui/gameloop.c:269 | memset g_champion_skill_level_up | I | start_game |
| ui/gameloop.c:301 | g_saved_game_exists = 0 | I | new game |
| ui/gameloop.c:302 | g_leader_empty_handed = 1 | I | new game |
| ui/gameloop.c:317 | g_leader_empty_handed = 1 | I | reset_champions_of_loaded_game (champion_reset_data_to_start_game) |
| ui/gameloop.c:318 | g_leader_hand_object = THING_NONE | I | same |
| ui/gameloop.c:319 | g_leader_index = -1 | I | same |
| ui/gameloop.c:321 | g_champions[i].attributes &= KEEP_MASK | F | clear stale redraw flags of a loaded game |
| ui/gameloop.c:324 | g_magic_caster_champion_index = -1 | S | force redraw idiom after load |
| ui/gameloop.c:333 | g_new_party_map_index = -1 | I | start_loaded_game |
| ui/gameloop.c:356 | g_fuse_sequence_requested = 0 | C | run_requested_sequence consumes the engine request |
| ui/gameloop.c:371 | g_random_seed = seed | T | projectile motion drawing restores the seed (view draws with game random) |
| ui/gameloop.c:399 | g_game_time++ | R | advance_game_time |
| ui/gameloop.c:401 | g_party.freeze_life_ticks-- | R | advance_game_time |
| ui/gameloop.c:404 | g_disabled_movement_ticks-- | R | advance_game_time |
| ui/gameloop.c:405 | g_projectile_disabled_movement_ticks-- | R | advance_game_time |
| ui/gameloop.c:414 | g_dead_champions = 0 | C | release_dead_champions consumes the death notice |
| ui/gameloop.c:424 | g_champions[caster] = before[caster] | T | temporary swap to draw the spell area of the last death |
| ui/gameloop.c:426 | g_champions[caster] = dead | T | swap back |
| ui/select.c:55 | g_game_mode = mode | I | game mode selection (set_mode) |
| ui/startend.c:191 | g_saved_game_exists = 0 | R | QUIT in the save dialog deletes the restart option |
| ui/startend.c:405 | g_game_time++ | R | fuse_update: one tick of the fuse sequence |
| ui/startend.c:480 | g_game_won = 1 | R | fuse_sequence |
| ui/startend.c:483 | g_party.magical_light_amount = FUSE_MAGICAL_LIGHT | R | fuse_sequence |
| ui/startend.c:484 | g_party.shield_defense, .spell_shield_defense, .fire_shield_defense = FUSE_SHIELD | R | fuse_sequence |
| ui/startend.c:514 | g_saved_game_exists = 0 | R | fuse_sequence end |
| ui/startend.c:531 | g_game_won = 1 | R | endgame_sensor (CSB) - inside the hook |
| ui/startend.c:536 | g_saved_game_exists = 0 | R | endgame_sensor (DM) - inside the hook |
| ui/startend.c:537 | g_game_won = 1 | R | endgame_sensor (DM) |
| ui/ui.c:207 | g_inventory_champion_ordinal = 0 | S | ui_draw_interface force reopen idiom |
| ui/ui.c:214 | g_magic_caster_champion_index = -1 | S | force redraw idiom |
| ui/ui.c:216 | g_redraw \|= REDRAW_ACTION_AREA | F | |
| ui/ui_champion.c:326 | c->attributes &= KEEP_MASK (c = &g_champions[i], :282) | F | ui_draw_champion_state clears flags after drawing |
| ui/ui_champion.c:333 | g_champions[i].attributes \|= attributes | F | ui_draw_all_champion_states |
| ui/ui_champion.c:441 | g_inventory_champion_ordinal = champion_index + 1 | S | inventory opens |
| ui/ui_champion.c:452 | g_champions[i].attributes \|= INVENTORY_ALL | F | |
| ui/ui_champion.c:460 | g_inventory_champion_ordinal = 0 | S | inventory closes |
| ui/ui_champion.c:463 | g_champions[i].attributes \|= STATUS_BOX | F | |
| ui/ui_champion.c:833 | g_champions[inv-1].attributes \|= PANEL | F | CSB scroll redraw |
| ui/ui_menus.c:58 | g_action_list_indices[0] | R | action list: first action always |
| ui/ui_menus.c:66 | g_action_list_indices[n] | R | action list: skill level + charges rule |
| ui/ui_menus.c:71 | g_action_list_indices[n] = ACTION_NONE | R | action list tail |
| ui/ui_menus.c:92 | g_acting_champion_ordinal = index + 1 | R/S | acting champion (after rule checks :86-90: action disabled, health, action set) |
| ui/ui_menus.c:95 | c->attributes \|= ACTION_HAND (c = :85) | F | |
| ui/ui_menus.c:151 | g_redraw &= ~REDRAW_ACTION_AREA | F | |
| ui/ui_menus.c:187 | c->direction = c->direction_maximum_damage_received (c = :184) | R | idle champion turns to its last attacker |
| ui/ui_menus.c:188 | c->attributes \|= ICON | F | |
| ui/ui_menus.c:191 | c->maximum_damage_received = 0 | R | same rule, per tick reset |
| ui/ui_menus.c:203 | g_champions[acting-1].attributes \|= ACTION_HAND | F | |
| ui/ui_menus.c:211 | g_action_damage = 0 | C | consume the action damage after drawing it |
| ui/ui_menus.c:271 | g_magic_caster_champion_index = champion_index | R/S | caster selection (rule: living champion, :270) |
| ui/ui_menus.c:288 | g_redraw &= ~(SPELL_AREA\|TABS\|SYMBOLS) | F | |
| ui/ui_menus.c:294 | g_acting_champion_ordinal = 0 | I | menus_reset |
| ui/ui_menus.c:296 | memset g_action_list_indices | I | menus_reset |
| ui/ui_menus.c:299 | g_action_damage = 0 | I | menus_reset |
| ui/ui_menus.c:300 | g_redraw = 0 | I/F | menus_reset |
| ui/view.c:1623 | g_dungeon_view_palette_index = 0 | R | view_set_light (set_dungeon_view_palette) |
| ui/view.c:1626 | g_dungeon_view_palette_index = LIGHT_DARKEST | R | same |
| ui/view.c:1629 | g_dungeon_view_palette_index++ | R | same, light thresholds |

Total: 90 table rows (some rows cover 2 lines); pointer writes are at command.c:553-557, :568-569, ui_menus.c:95, :187-191,
ui_champion.c:326, advanced.c:138).

Rule code in ui that writes engine **data through engine setters** (not caught by R1.7, but R8 names it):
- ui/gameloop.c:203-214 torch burn: `weapon_set_charges`, `rec_object_clear_important`.
- ui/command.c:457-467 fill at fountain: `junk_set_charges`, `potion_set_type`.
- ui/command.c:425-430 vi altar: builds a TimelineEvent and `timeline_add_event`.
- ui/command.c:497-503 door button: `sound_request_play` + `sensor_add_event(EVENT_DOOR, ..., EFFECT_TOGGLE, g_game_time + 1)`.
- ui/startend.c:477-515 fuse sequence: `group_set_health`, `group_set_cells`, `group_set_directions`, `group_set_creature_type`, `explosion_create`, `group_delete` (interleaved with drawing; a scripted sequence - keep in ui, see group G).

### Hooks
- `g_endgame_sensor_hook` (engine/timeline_process.h:50, defined timeline_process.c:28, called :271), set by ui/gameloop.c:81 (`= endgame_sensor`). Rule 8 of docs/coding-standard.md:31 allows it; R1.7 excludes `_hook` names.
- `g_frame_hook` is set in platform/main.c:318 (not an engine variable).
- Note: the hook body ui/startend.c:528-539 writes g_game_won and g_saved_game_exists (rule state). Proposal: the engine sets them in event_wall before the hook call (timeline_process.c:271) and passes the adventure decision; the hook keeps only the UI (animation, endgame). Keep-order check: CSB keeps the saved game (no clear), DM clears it after the wait (`wait_frames` - no game state change in between, so moving it before the call is safe).

### Groups (rules to move)

Engine prefixes: champion.h `champion_`; action.h `action_`; game.h `game_ timeline_ ...`; inventory.h `inventory_ object_ process_ chest_ close_`; magic.h `magic_`; sensor.h `sensor_`; savegame.h `savegame_`; group.h `group_ creature_ party_`; random.h `random_`.

| id | rule | ui code (lines) | proposed engine function | file |
|---|---|---|---|---|
| A | Fountain drinking | command.c:514-520 (+ FOUNTAIN_WATER command.c:42) | `void champion_drink_from_fountain(bool whole_party);` sets water 2048 for the leader, or every living champion; the sound request stays in engine too (`sound_request_play(SOUND_SWALLOW...)`). ui passes `g_advanced_features`. | engine/champion.c/.h |
| B | Fill flask/waterskin at fountain | command.c:457-467 (+ FILLED_WATERSKIN_CHARGES :43) | `bool inventory_fill_leader_hand_at_fountain(void);` changes charges/type, adjusts load, sets CHAMPION_ATTR_LOAD | engine/inventory.c/.h |
| C | Vi altar rebirth | command.c:425-430 (in drop_object :416-434) | `void sensor_add_vi_altar_rebirth_event(uint16_t bones, int16_t x, int16_t y, int16_t cell);` (or `inventory_drop_leader_hand_object(int16_t cell, int16_t x, int16_t y, bool vi_altar)` taking :422-430 whole). Constant VI_ALTAR_REBIRTH_EFFECT moves. | engine/sensor.c/.h |
| D | Door button press | command.c:497-503 | `void sensor_press_door_button(int16_t x, int16_t y);` switch sound + EVENT_DOOR toggle at g_game_time+1. The hit test (door_button_present + in_box wall box) stays in ui. | engine/sensor.c/.h |
| E | Rune mana cost, add/delete rune | command.c:546-572 (add :546-562, delete :564-572) | `bool magic_add_champion_symbol(int16_t symbol);` (cost, mana, symbols, step, STATISTICS flag; returns false when the mana is short) and `bool magic_delete_champion_symbol(void);` ui keeps the 3 draw calls. | engine/magic.c/.h |
| F | Party light amount, palette index, torch burn | gameloop.c:174-200 (gameloop_party_light), :203-214 (decrease_torches_light_power), :216-221 (gameloop_set_light), view.c:1619-1630 (view_set_light) | `int16_t inventory_get_party_light(void);` (F0337 first half), `void inventory_set_dungeon_view_palette(int16_t map_index, int16_t light);` (writes g_dungeon_view_palette_index; needs a tables accessor for LIGHT_THRESHOLDS per R2), `void inventory_decrease_torches_light_power(void);` Callers to update: advanced.c:50, gameloop.c:162, :219, :226, :439, view.c, tests. | engine/inventory.c/.h |
| G | Game time tick | gameloop.c:397-406 (advance_game_time) | Order now: time++, torches, freeze_life--, **menus_refresh_action_area (ui)**, time effects, movement ticks--. Proposal: `void game_advance_time(void);` = :399-401 (calls inventory_decrease_torches_light_power) and `void game_apply_tick_effects(void);` = :403-405; ui calls refresh between them, so the order stays exact. Alternative: move H out of menus_refresh_action_area first, then one function. Plus fuse tick startend.c:405 → `game_advance_time_only()` or reuse a `g_game_time` increment function. | engine/game.c/.h |
| H | Idle champion turns to its last attacker | ui_menus.c:180-192 (in menus_refresh_action_area :179) | `uint16_t champion_turn_to_last_attackers(void);` sets direction, ICON flag, clears maximum_damage_received; returns a champion mask (or ui draws by ICON flags). Note: ui draws inside the loop (:189); drawing after the loop is pixel-equal only if draw order does not matter (each status box is separate - it is). | engine/champion.c/.h |
| I | Champion icon drag (move/swap cells) | command.c:365-390 (writes :377-388) | `int16_t champion_move_to_cell(int16_t champion_index, int16_t cell);` sets direction=party direction, swaps cell with the champion there, ICON flags; returns the other index or -1. ui keeps dragged_icon_cell_plus_1 and the fill/draw calls. | engine/champion.c/.h |
| J | Action list / acting champion | ui_menus.c:53-72 (menus_set_action_list), :83-99 (menus_set_acting_champion rule part :85-92) | `void action_set_action_list(int16_t action_set_index);` and `bool action_set_acting_champion(uint16_t champion_index);` moves g_action_count, g_action_list_minimum_skill_levels (now ui.h:79-80) to action.h. PRD R8 hints at an allowlist exception "as for the action area of dm2" - decide: move or allowlist with reason. | engine/action.c/.h |
| K | Magic caster selection | ui_menus.c:268-273; force idiom command.c:240-242, :340-342, ui.c:213-215, gameloop.c:323-325 | `bool magic_set_caster(int16_t champion_index);` (rule: living champion; returns changed). The 4 force-redraw idioms become a ui call `menus_draw_spell_area()` (draw_spell_area :250 made public) without touching the variable. | engine/magic.c/.h |
| L | Sleep | command.c:665 | `void champion_go_to_sleep(void);` (counterpart of existing champion_wake_up) | engine/champion.c/.h |
| M | Fuse end state | startend.c:480-484, :514 | `void game_start_fuse_sequence(void);` sets won, magical light, 3 shields; `g_saved_game_exists = 0` → `savegame_forget(void)` or inside an engine `game_end(bool won)`. The scripted group calls :492-509 may stay in ui (sequence with drawing) - flag as exception or move to `group_`/`projectile_` helpers. | engine/game.c, savegame.c |
| N | Saved game flag at QUIT/endgame | startend.c:191, :514, :536 | `void savegame_forget(void);` (sets g_saved_game_exists 0). | engine/savegame.c/.h |
| O | Quick save/load keep flags | advanced.c:108-128 | `int16_t savegame_quick_save(dir)` / `savegame_quick_load(dir)` that do not change g_saved_game_exists and the save times; or a `bool keep_flags` argument of savegame_save_as/load_from. | engine/savegame.c/.h |
| P | Cheats | advanced.c:137-138 | `void game_toggle_cheat(int16_t cheat);` (enum CHEAT_LIGHT/IMMORTAL/FOOD) | engine/game.c/.h |
| Q | Game start/load reset | gameloop.c:263-269, :301-302, :317-324, :333, select.c:55 | `void game_reset_session(void);` (candidate, sleeping, won, cheats, level-up table); new game :301-302 into game_new() or `game_start_new()`; :317-319 `champion_reset_data_to_start_game()` (F0278 name; puts hand object back, returns leader/caster to restore); :333 into map_reset()/move_reset(); select.c:55 `game_set_mode(int16_t mode)`. | engine/game.c, champion.c |
| R | Inventory open/close ordinal | command.c:249, :319, :333, ui.c:207, ui_champion.c:441, :460 | Design decision: (a) `inventory_set_open_champion(int16_t ordinal)` setter (passes R1.7, little value); (b) keep the ordinal writes in ui_draw_inventory_open/close only and change the "clear then reopen" idioms to a `ui_draw_inventory_redraw(i)` that needs no clear; allowlist the 2 remaining lines with reason "UI state the engine reads". command.c:333 comment says the rule clears it - check champion_click_in_resurrect_reincarnate_panel. | engine/inventory.c or allowlist |

**Flag writes (kind F)**: command.c:296, :346, :378, :385, :466, :554; gameloop.c:321; ui.c:216; ui_champion.c:326, :333, :452, :463, :833; ui_menus.c:95, :151, :188, :203, :288, :300. Proposal: they stay. Rule 8 lets engine set and ui clear/consume redraw flags; ui also sets them to ask its own drawer (ui_draw_champion_state draws from attributes). The R1.7 check needs a filter: a write whose field is `attributes` with a `CHAMPION_ATTR_*` operand, or `g_redraw` with `REDRAW_*`. Those inside a moved rule (command.c:378/385/466/554, ui_menus.c:188) go into the engine function with the rule.

**Consume flags (kind C)**: gameloop.c:356 (g_fuse_sequence_requested), :414 (g_dead_champions), ui_menus.c:211 (g_action_damage). Proposal: either engine "take" functions (`projectile_take_fuse_request()`, `champion_take_dead_champions()`, `action_take_damage()`) or allowlist as flags. Simplest uniform: take functions (return old value, clear).

**Technical (kind T)**: gameloop.c:371 seed restore → `random_get_seed()/random_set_seed()` in random.c or allowlist reason "drawing must not change the game random"; gameloop.c:424-426 temporary champion swap → pass `const Champion *` to the spell-area drawer (menus_refresh_spell_area / draw_spell_area take the caster record), removes both writes.

## 2. Tests that exercise each rule now

| id | tests now | reference (original/emulator)? |
|---|---|---|
| A fountain drink | tests/test_modes.py:537 test_empty_hand_at_a_fountain (vanilla + advanced, via click) | no (expected values) |
| B fill at fountain | **none** | - |
| C vi altar event creation | **none** (tests/test_actions.py:294 test_vi_altar_rebirth adds the event itself, covers only event_vi_altar_rebirth) | - |
| D door button | **none** (control test_view_stops_at_a_closed_door :289 is the view) | - |
| E rune cost/add/delete | tests/test_control.py:368 test_a_rune_without_enough_mana_fails (refusal), :383 test_a_cast_leaves_the_cleared_runes_inverted... (one add with mana 100); cost formula / multiplier and delete: **none** | no |
| F light amount | tests/test_champion.py:511 test_party_light_matches_original (emulator, calls gameloop_set_light, checks g_dungeon_view_palette_index); tests/test_gameloop.py:264 test_party_light_sets_the_darkness_of_view_and_rules; tests/test_modes.py:586 test_cheat_light_draws_the_bright_palette; tests/test_automap.py:231 | yes (test_champion) |
| F torch burn | **none** | - |
| G game time tick | indirect only: gameloop_tick in tests/test_gameloop.py, test_modes.py:195 (automap stops time), test_save.py, test_csb_ending.py; freeze_life and movement tick decrements: **none** direct. tests/test_creatures.py:99 `tick()` re-implements the tick (g_game_time += 1) - could call game_advance_time after the move | no |
| H turn to attacker | **none** | - |
| I icon drag | tests/test_control.py:667 test_swap_champions_and_look (positions swap) | no |
| J action list | tests/test_ui.py:259 test_render_action_menu (list[0] != NONE only); tests/test_actions.py:258 sets the list by hand; skill/charge filter: **none** | no |
| K caster | tests/test_ui.py:235 (menus_set_magic_caster(1)), test_gameloop.py:394, control set_caster tests | no |
| L sleep | tests/test_save.py:375 test_new_game_after_sleep_runs_at_normal_speed | no |
| M fuse | tests/test_save.py:336 test_fuse_sequence_ends_the_game | no |
| N saved flag | tests/test_save.py:133, :293; tests/test_csb_ending.py:458-487 (sensor 18 keeps it) | no |
| O quick save | tests/test_modes.py:153 test_quick_save_and_quick_load (asserts g_saved_game_exists == 0 at :171) | no |
| P cheats | tests/test_modes.py:566, :613, :626 | no |
| Q reset | tests/test_save.py:111 round trip, tests/test_gameloop.py:135, test_modes quick load | no |
| endgame hook | tests/test_csb_ending.py:394-487 (sensor 18 tests) | no |

Rules with no test: B, C (ui part), D, E (cost/delete), torch burn, H, J (filter), freeze_life/movement tick decrement.
PRD R8 (line 182) allows a differential test for a moved rule without a test: candidates with an
original routine: E (rune cost, MENU.C add symbol), torch burn (F0338), H (turn in the action-area refresh), J (set action list), D/C (event fields).

## 3. R9 - square drawers in ui/view.c

Shared helpers: `aspect` :301, `wall` :353, `things` :850, `field` :900, `wall_ornament` :944 (returns alcove; sets g_facing_* and the D1C click box), `floor_ornament` :988, `door_button` :1019, `door` :1035, `ceiling_pit` :1079, `stairs_front` :1086 (graphic `stairs + g (+STAIRS_DOWN)`, zone `up_zone (+ZONE_STAIRS_DOWN_DELTA)`).
Dispatcher: view_draw :1869; tables :1871 (function pointers), :1872 (forward/right); loop :1897-1901. Enum DRAWN_SQUARE_COUNT :1091.
Modes: `S` = side-mirror mode = BLIT_TRANSPARENT for L and C, BLIT_FLIPPED for R. `W` = wall mode = BLIT_TRANSPARENT for L/R, BLIT_OPAQUE for C.

### Common shape ("full" drawers d3l2 d3r2 d3l d3r d3c d2l d2r d2c d1l d1r d1c)
```
aspect
switch element:
  WALL:   wall(ws_wall, zone_wall, W); [side ornament]; [front ornament -> if alcove: things(CELLS_ALCOVE)]; return
  PIT:    D3: if (!state) wall(pit, zone_pit, S)   D2/D1: wall(state ? invisible_pit : pit, zone_pit, S); break
  DOOR_SIDE / STAIRS_SIDE: per square (see table)
  DOOR_FRONT: floor_ornament(view); things(door_back); frames...; [door_button]; door(view); things(door_front); return
  STAIRS_FRONT: stairs_front(state, stairs_front_gfx, zone_stairs_up_front, S); break
  CORRIDOR / TELEPORTER: break
  default: return
floor_ornament(view); [ceiling_pit(ceil_gfx, ceil_zone, S)]; things(cells); if TELEPORTER field(view_square, zone_wall)
```

### Per function

| fn | lines | fwd,right | wall id / zone / W | side orn | front orn | pit (gfx, zone, S) | side elements | door back / frames / button / front | stairs front | ceiling pit | cells |
|---|---|---|---|---|---|---|---|---|---|---|---|
| d3l2 | 1096-1121 | 3,-2 | WS_WALL_D3L2 / ZONE_WALL_D3L2 / T | a.right VIEW_WALL_D3L2_RIGHT | - | if !state GFX_PIT_D3L2, ZONE_PIT_D3L2, T | DOOR/STAIRS side: cells=CELLS_BL_BR_FR | DOOR_BACK_BL_BR / none / - / DOOR_FRONT_FL_FR | **explicit**: up LAYOUT_STAIRS_UP_FRONT_D3L2, down LAYOUT_STAIRS_DOWN_FRONT_D3L2, zone ZONE_STAIRS_UP_FRONT_D3L2 (+DELTA), T | - | CELLS_BL_BR_FL_FR |
| d3r2 | 1123-1148 | 3,2 | WS_WALL_D3R2 / ZONE_WALL_D3R2 / T | a.left VIEW_WALL_D3R2_LEFT | - | if !state GFX_PIT_D3L2, ZONE_PIT_D3R2, F | cells=CELLS_BR_BL_FL | DOOR_BACK_BR_BL / none / - / DOOR_FRONT_FR_FL | **explicit**: LAYOUT_STAIRS_UP_FRONT_D3R2 / LAYOUT_STAIRS_DOWN_FRONT_D3R2, ZONE_STAIRS_UP_FRONT_D3R2, F | - | CELLS_BR_BL_FR_FL |
| d3l | 1150-1178 | 3,-1 | WS_WALL_D3L / ZONE_WALL_D3L / T | a.right VIEW_WALL_D3L_RIGHT | VIEW_WALL_D3L_FRONT | if !state GFX_PIT_D3L, ZONE_PIT_D3L, T | cells=CELLS_BL_BR_FR | BL_BR / [WS_DOOR_FRAME_LEFT_D3L @ZONE_DOOR_FRAME_LEFT_D3L T, WS_DOOR_FRAME_RIGHT_D3L @ZONE_DOOR_FRAME_RIGHT_D3L F] / - / FL_FR | STAIRS_FRONT_D3L, ZONE_STAIRS_UP_FRONT_D3L, T | - | BL_BR_FL_FR |
| d3r | 1180-1209 | 3,1 | WS_WALL_D3R / ZONE_WALL_D3R / T | a.left VIEW_WALL_D3R_LEFT | VIEW_WALL_D3R_FRONT | if !state GFX_PIT_D3L, ZONE_PIT_D3R, F | cells=CELLS_BR_BL_FL | BR_BL / [WS_DOOR_FRAME_LEFT_D3R @LEFT_D3R T, **WS_DOOR_FRAME_LEFT_D3L** @ZONE_DOOR_FRAME_RIGHT_D3R F] / BUTTON_D3R / FR_FL | STAIRS_FRONT_D3L, ZONE_STAIRS_UP_FRONT_D3R, F | - | BR_BL_FR_FL |
| d3c | 1211-1237 | 3,0 | WS_WALL_D3C / ZONE_WALL_D3C / O | - | VIEW_WALL_D3C_FRONT | if !state GFX_PIT_D3C, ZONE_PIT_D3C, T | **none: side elements return** (default) | BL_BR / [WS_DOOR_FRAME_D3C @LEFT_D3C T, WS_DOOR_FRAME_D3C @RIGHT_D3C F] / BUTTON_D3C / FL_FR | STAIRS_FRONT_D3C, ZONE_STAIRS_UP_FRONT_D3C, T | - | BL_BR_FL_FR |
| d2l2 | 1239-1245 | 2,-2 | WS_WALL_D2L2 / ZONE_WALL_D2L2 / T | mini: wall, else if TELEPORTER field(VIEW_SQUARE_D2L2, ZONE_WALL_D2L2); nothing else | | | | | | | |
| d2r2 | 1247-1253 | 2,2 | WS_WALL_D2R2 / ZONE_WALL_D2R2 / T | mini, same shape | | | | | | | |
| d2l | 1255-1287 | 2,-1 | WS_WALL_D2L / ZONE_WALL_D2L / T | a.right VIEW_WALL_D2L_RIGHT | VIEW_WALL_D2L_FRONT; **alcove: ceiling_pit then things** | state ? GFX_INVISIBLE_PIT_D2L : GFX_PIT_D2L, ZONE_PIT_D2L, T | STAIRS_SIDE: wall(stairs+STAIRS_SIDE_D2L, ZONE_STAIRS_SIDE_D2L, T) then fall into DOOR_SIDE: cells=CELLS_BR_FL_FR | BL_BR / [WS_DOOR_FRAME_TOP_D2L @TOP_D2L T] / - / **door, then ceiling_pit**, FL_FR | STAIRS_FRONT_D2L, ZONE_STAIRS_UP_FRONT_D2L, T | GFX_CEILING_PIT_D2L, ZONE_CEILING_PIT_D2L, T | BL_BR_FL_FR |
| d2r | 1289-1318 | 2,1 | WS_WALL_D2R / ZONE_WALL_D2R / T | a.left VIEW_WALL_D2R_LEFT | VIEW_WALL_D2R_FRONT; alcove: things only (**no ceiling_pit**) | state ? GFX_INVISIBLE_PIT_D2L : GFX_PIT_D2L, ZONE_PIT_D2R, F | STAIRS_SIDE: wall(stairs+STAIRS_SIDE_D2L, ZONE_STAIRS_SIDE_D2R, F) fall into DOOR_SIDE: cells=CELLS_BL_FR_FL | BR_BL / [WS_DOOR_FRAME_TOP_D2R @TOP_D2R T] / - / door, **no ceiling_pit**, FR_FL | STAIRS_FRONT_D2L, ZONE_STAIRS_UP_FRONT_D2R, F | GFX_CEILING_PIT_D2L, ZONE_CEILING_PIT_D2R, F | BR_BL_FR_FL |
| d2c | 1320-1348 | 2,0 | WS_WALL_D2C / ZONE_WALL_D2C / O | - | VIEW_WALL_D2C_FRONT | state ? GFX_INVISIBLE_PIT_D2C : GFX_PIT_D2C, ZONE_PIT_D2C, T | none (return) | BL_BR / [WS_DOOR_FRAME_TOP_D2C @TOP_D2C T, WS_DOOR_FRAME_D2C @LEFT_D2C T, WS_DOOR_FRAME_D2C @RIGHT_D2C F] / BUTTON_D2C / FL_FR | STAIRS_FRONT_D2C, ZONE_STAIRS_UP_FRONT_D2C, T | GFX_CEILING_PIT_D2C, ZONE_CEILING_PIT_D2C, T | BL_BR_FL_FR |
| d1l | 1350-1376 | 1,-1 | WS_WALL_D1L / ZONE_WALL_D1L / T | a.right VIEW_WALL_D1L_RIGHT | - | state ? GFX_INVISIBLE_PIT_D1L : GFX_PIT_D1L, ZONE_PIT_D1L, T | STAIRS_SIDE: up stairs+STAIRS_UP_SIDE_D1L @ZONE_STAIRS_UP_SIDE_D1L / down stairs+STAIRS_DOWN_SIDE_D1L @ZONE_STAIRS_DOWN_SIDE_D1L, T; cells unchanged. DOOR_SIDE = corridor | DOOR_BACK_BR / [WS_DOOR_FRAME_TOP_D1L @TOP_D1L T] / - / DOOR_FRONT_FR | STAIRS_FRONT_D1L, ZONE_STAIRS_UP_FRONT_D1L, T | GFX_CEILING_PIT_D1L, ZONE_CEILING_PIT_D1L, T | CELLS_BR_FR |
| d1r | 1378-1404 | 1,1 | WS_WALL_D1R / ZONE_WALL_D1R / T | a.left VIEW_WALL_D1R_LEFT | - | state ? GFX_INVISIBLE_PIT_D1L : GFX_PIT_D1L, ZONE_PIT_D1R, F | STAIRS_SIDE: up STAIRS_UP_SIDE_D1L @ZONE_STAIRS_UP_SIDE_D1R / down STAIRS_DOWN_SIDE_D1L @ZONE_STAIRS_DOWN_SIDE_D1R, F. DOOR_SIDE = corridor | DOOR_BACK_BL / [WS_DOOR_FRAME_TOP_D1R @TOP_D1R T] / - / DOOR_FRONT_FL | STAIRS_FRONT_D1L, ZONE_STAIRS_UP_FRONT_D1R, F | GFX_CEILING_PIT_D1L, ZONE_CEILING_PIT_D1R, F | CELLS_BL_FL |
| d1c | 1406-1436 | 1,0 | WS_WALL_D1C / ZONE_WALL_D1C / O | - | VIEW_WALL_D1C_FRONT | state ? GFX_INVISIBLE_PIT_D1C : GFX_PIT_D1C, ZONE_PIT_D1C, T | none (return) | BL_BR / [WS_DOOR_FRAME_TOP_D1C @TOP_D1C T, WS_DOOR_FRAME_LEFT_D1C @LEFT_D1C T, WS_DOOR_FRAME_RIGHT_D1C @RIGHT_D1C **T** (not flipped)] / BUTTON_D1C / FL_FR | STAIRS_FRONT_D1C, ZONE_STAIRS_UP_FRONT_D1C, T | GFX_CEILING_PIT_D1C, ZONE_CEILING_PIT_D1C, T | BL_BR_FL_FR |
| d0l | 1438-1451 | 0,-1 | WS_WALL_D0L / ZONE_WALL_D0L / T, return | - | - | state ? GFX_INVISIBLE_PIT_D0L : GFX_PIT_D0L, ZONE_PIT_D0L, T, **fall through** to corridor | STAIRS_SIDE: wall(stairs+STAIRS_SIDE_D0L, ZONE_STAIRS_SIDE_D0L, T), **return**; DOOR_SIDE = corridor | no door front / stairs front: nothing drawn | - | corridor/teleporter/door side/pit: ceiling_pit(GFX_CEILING_PIT_D0L, ZONE_CEILING_PIT_D0L, T) + things(CELLS_BR); **no floor ornament** | CELLS_BR |
| d0r | 1453-1466 | 0,1 | WS_WALL_D0R / ZONE_WALL_D0R / T | - | - | GFX_*_D0L, ZONE_PIT_D0R, F | STAIRS_SIDE_D0L @ZONE_STAIRS_SIDE_D0R F, return | same as d0l | - | GFX_CEILING_PIT_D0L, ZONE_CEILING_PIT_D0R, F | CELLS_BL |
| d0c | 1468-1482 | 0,0 | no wall case | - | - | state ? GFX_INVISIBLE_PIT_D0C : GFX_PIT_D0C, ZONE_PIT_D0C, T | DOOR_SIDE: wall(WS_DOOR_FRAME_D0C, ZONE_DOOR_FRAME_D0C, T) | - | **twice**: stairs_front(STAIRS_FRONT_D0C, ZONE_STAIRS_UP_FRONT_D0C_LEFT, T) then (…_RIGHT, F) | always: GFX_CEILING_PIT_D0C, ZONE_CEILING_PIT_D0C, T | always things(CELLS_BL_BR); field if teleporter; no floor ornament |

Floor ornament view (VIEW_x) and things/field view square (VIEW_SQUARE_x) are per square; field zone is always ZONE_WALL_x of the square. floor_ornament() itself derives flip from the view id (:992-994, in floor_ornament :988).

### Proposed descriptor
```c
typedef struct { int16_t ws, zone; uint8_t mode; } WallPart;   /* one wall() call; ws = wall_set_ids index */

typedef struct {
    int8_t forward, right;           /* from view_draw :1872 */
    uint8_t shape;                   /* SHAPE_FULL, SHAPE_WALL_OR_FIELD (D2L2/D2R2), SHAPE_D0_SIDE (D0L/D0R), SHAPE_D0C */
    uint8_t mirror;                  /* S: BLIT_TRANSPARENT or BLIT_FLIPPED (pit, stairs, ceiling pit) */
    int16_t view, view_square;       /* VIEW_D* (floor ornament, door), VIEW_SQUARE_D* (things, field) */
    WallPart wall;                   /* wall id, ZONE_WALL_*, BLIT_TRANSPARENT/OPAQUE; zone also for field() */
    uint8_t side_face;               /* FACE_NONE, FACE_RIGHT (a.right), FACE_LEFT (a.left) */
    int16_t side_ornament_view, front_ornament_view;   /* VIEW_WALL_*, -1 none */
    int16_t pit, invisible_pit, pit_zone;              /* invisible_pit -1: D3 hides an invisible pit */
    uint16_t cells, side_cells;      /* side_cells 0: a door or stairs side returns (center squares) */
    int16_t stairs_side, stairs_side_zone;             /* D2 one graphic; D0 one graphic + return */
    int16_t stairs_side_down, stairs_side_down_zone;   /* D1 only: up/down graphics; -1 otherwise */
    int16_t stairs_front, stairs_front_zone;           /* STAIRS_FRONT_* relative to `stairs` */
    uint16_t door_back, door_front;
    WallPart frames[3]; uint8_t frame_count;
    int16_t button;                  /* BUTTON_* or -1 */
    int16_t ceiling, ceiling_zone;   /* GFX_CEILING_PIT_*, -1 for D3 */
    uint8_t flags;                   /* SQ_CEILING_AFTER_DOOR, SQ_CEILING_IN_ALCOVE (D2L), SQ_STAIRS_LAYOUT_IDS (D3L2/D3R2),
                                        SQ_DOOR_SIDE_IS_FLOOR (D1L/D1R, D0), SQ_FRONT (D1C) */
} SquareView;
static const SquareView square_views[DRAWN_SQUARE_COUNT] = { /* back to front, as now */ };
static void draw_square(const SquareView *s, int16_t dir, int16_t x, int16_t y);
```
view_draw keeps its loop :1897-1901 and calls `draw_square(&square_views[i], dir, sx, sy)`; the forward/right arrays :1872 go into the table.

draw_square call order (must be exactly):
1. aspect; if SQ_FRONT: g_square_ahead_element = a.element.
2. WALL: if SQ_FRONT clear g_facing_*; wall(wall); if side_face: wall_ornament(side); if front_ornament_view >= 0 and wall_ornament(front) returns alcove: [SQ_CEILING_IN_ALCOVE: ceiling_pit]; things(CELLS_ALCOVE). D2L: return before ceiling_pit when not alcove. return.
3. PIT: invisible_pit < 0 ? (if !state wall(pit)) : wall(state ? invisible_pit : pit); zone pit_zone, mode mirror.
4. sides, door front, stairs front as in the table; 5. floor_ornament, ceiling_pit (if ceiling >= 0), things(cells), field if teleporter.

### Special cases that do not fit the table cleanly
1. d2l2/d2r2 (:1239-1253) and d0l/d0r/d0c (:1438-1482): different control flow (no floor ornament, fall-through pit, early return for stairs side, d0c always draws ceiling+things and stairs front twice). Best: 3 small shape branches in draw_square, or keep them as 3 functions (d2_far_side, d0_side, d0c) with table data.
2. d2l: ceiling_pit inside the alcove case (:1263) and after the door (:1271); d2r and others do not. Original asymmetry (kept quirk?) - keep with flags; check against Ghidra before "fixing".
3. d3l2/d3r2 stairs front use absolute LAYOUT_STAIRS_* ids (:1112-1113, :1139-1140), not `stairs + STAIRS_FRONT_*`.
4. d3r right door frame uses WS_DOOR_FRAME_LEFT_D3L (:1197), d3l uses WS_DOOR_FRAME_RIGHT_D3L (:1167): data, fits.
5. d1c right frame is BLIT_TRANSPARENT (:1424), not flipped: data, fits.
6. d1c side effects: g_square_ahead_element (:1410) before the switch, g_facing_* clear only in the wall case (:1413). Non-wall ahead leaves the facing flags as they were (state_json.c:718 depends on this comment). Keep exactly.
7. Side elements: d3l2/d3r2/d3l/d3r change cells; d2l/d2r draw stairs side then change cells (fall-through); d1l/d1r draw up/down stairs side, keep cells, and treat DOOR_SIDE as floor; d3c/d2c/d1c return. Needs side_cells + stairs_side fields + flag.
8. D3 pit: only drawn when not invisible (`if (!a.state)`), D2/D1/D0 draw the invisible pit graphic.
9. Mirror relation: R squares reuse L graphics (GFX_PIT_D3L2 for d3r2, STAIRS_FRONT_D3L for d3r, GFX_CEILING_PIT_D1L for d1r, ...) with BLIT_FLIPPED - data, fits.
10. VIEW_D* enum order is L, C, R (view.c:142) while the draw order is L, R, C - table index != view id.

### Tests covering view drawing
- tests/test_view.py:173 test_render_view: 5 positions (start, door, ornament, text, stairs). Checks only: render twice gives the same fb, more than 4 colors. Writes PNGs (to tmp_path and a hard-coded SCRATCH of another session, test_view.py:14). **No golden image, no emulator.**
- tests/test_view.py:283 test_render_things: 9 positions (objects, creatures, D3 creatures, teleporter, fluxcage, fireball, party explosion, dark). Checks >4 colors and that an explosion changes pixels. No reference.
- Emulator tests in test_view.py cover only the primitives: :90 shrink, :115 zones, :203 masked fill, :54 blit/flip.
- Other: tests/test_control.py:1024 (alcove flag via view_draw), :1040 (fountain flag), :289 closed door; test_modes fountain/cheat-light; test_csb_engine.py:172 (render runs). These check flags, not pixels.
- Coverage of the 16 squares: not measured; no test compares any square's pixels to a reference. So R9 has no safety net now. Proposal (allowed by CLAUDE.md "golden snapshot"): before R9, add a golden test that renders every (x, y, dir) of maps 0-2 (or all maps) with fixed light and stores a CRC-32 of fb + palette per view in a git-tracked file; generated from the current code, compared after the table change. Add an assertion/count that each of the 16 squares saw each element kind (wall, pit, door front, door side, stairs front/side, teleporter, alcove) at least once, so all branches are covered.
