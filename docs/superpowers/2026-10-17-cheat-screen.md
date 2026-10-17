# Cheat screen: PRD

Date: 2026-10-17. Branch: `feature/2026-10-17-options`. Status: implemented.

## Goal

In the advanced mode, the key C opens a cheat screen of its own. It switches six cheats: full light,
immortality, food and water (today on the automap), one-hit kill, walk through walls, and reveal the map. It
lists all spells with their runes, and it has quick save (F5) and quick load (F9) as items. The save file keeps
the cheat state, as it keeps the explored map. Today the cheats are the keys L, I and F on the automap
(`docs/manual.md:155`, `src/ui/advanced.c:147-150`), and a new or loaded game turns them off.

A claim without a `file:line` source is an assumption; the plan task that relies on it checks it first.

## The port today

- The cheats:
  - `Session` holds `cheat_light`, `cheat_immortal`, `cheat_food` (`src/engine/include/game.h`).
    `game_toggle_cheat` switches one (`src/engine/game.c:82-88`). `game_reset_session` turns them off at each
    new or loaded game (`src/engine/game.c:64-75`, called by `start_game`, `src/ui/gameloop.c:136`).
  - Effects: full light in the view (`src/ui/view.c:1453`) and in the explored depth (`src/engine/explored.c:97`);
    immortality (`src/engine/champion.c:945`); food and water (`src/engine/champion.c:1177`). No effect checks
    the mode; only the advanced mode can switch a cheat on (`src/ui/advanced.c:147`).
  - The automap read-out shows them (`src/ui/advanced.c:56-62`). `get_advanced_key` gives `KEY_L`, `KEY_F`,
    `KEY_I` (`src/platform/app.c:68-73`).
- Free keys: `get_advanced_key` (`src/platform/app.c:57-82`) maps M, Tab, F5, F9, L, F, I, 1-3, and
  `advanced_letter_key` (`src/ui/advanced.c:178-197`) maps W, A, S, D, Q, E. C is free in the advanced mode.
  `KEY_C` does not exist yet (`src/ui/include/command.h`).
- Quick save and quick load: `quick_save` and `quick_load` (`src/ui/advanced.c:123-138`), only in the dungeon
  view (`src/ui/advanced.c:158-166`). A quick load starts the loaded game (`gameloop_continue_loaded_game`).
- Creature damage: `damage_creature` (`src/engine/group.c:741-772`). An invulnerable creature takes no damage
  (`src/engine/group.c:746-747`). Callers:
  - melee: `group_get_melee_action_damage` (`src/engine/group.c:1470`);
  - projectiles and explosions: `src/engine/projectile.c:160,282,551,560,583`, through `group_damage_creature`
    and `group_damage_all_creatures` (`src/engine/group.c:774-801`);
  - a closing door: `group_door_damages_creatures` (`src/engine/group.c:812-817`);
  - a fall: `group_is_killed_by_fall` (`src/engine/group.c:2240`).
  - A projectile or an explosion has no owner: the record has no field for the caster
    (`src/engine/include/record.h`, `rec_projectile_*`, `rec_explosion_*`).
  - Melee experience uses the damage that the hit computed (`src/engine/group.c:1472-1473`).
- Party movement: `move_party` (`src/engine/move.c:442-491`). `is_square_blocking` (`src/engine/move.c:414-427`)
  stops the party at a wall, a solid fake wall, or a door that is not open; `bump_into_wall`
  (`src/engine/move.c:430-440`) damages two champions. A creature on the square stops the party
  (`src/engine/move.c:473-475`). Stairs act before the block check (`src/engine/move.c:460-467`). Outside the
  map, `map_get_square` gives a wall byte (`src/engine/map.c:61-80`).
- Sensors on a wall square: `sensor_process_thing_change` runs the wall sensors of the cell of the thing on a
  wall square (`src/engine/sensor.c:439-455`). A party move (`move_get_move_result`) and a party turn
  (`move_turn_party`, `src/engine/move.c:408,410`) call it.
- A projectile stops at a wall, a solid fake wall or stairs before it enters the square
  (`src/engine/projectile.c:515-523`).
- The loads: every load goes through `savegame_load_from` (`src/engine/savegame.c:551-587`): `savegame_load`
  (`src/engine/savegame.c:525-529`) for the control `load` (`src/control/control.c:77`), the RESUME and
  RESTART loads (`src/ui/startend.c:210`) and the CSB adventure (`src/engine/savegame.c:476`), and
  `savegame_quick_load`. A new game calls `game_start_new` (`src/engine/game.c:38`, from
  `src/ui/gameloop.c:172`).
- The automap draws a square only when `explored_is_seen` is true (`src/ui/automap.c:149`).
- The save extension:
  - Outside vanilla, `append_automap` adds the explored block and a 4-byte big-endian length after the
    original save data (`src/engine/savegame.c:288-308`). The block starts with the magic `AMAP`
    (`EXPLORED_SAVE_MAGIC`, `src/engine/include/explored.h`), then a version byte (`src/engine/explored.c:12,159`).
  - `find_automap` reads only the last block (`src/engine/savegame.c:310-323`). A missing or damaged block gives
    an empty automap (`src/engine/savegame.c:578-579`).
  - A vanilla save has no extension (`src/engine/savegame.c:294-295`).
- The spells: `tables_get_spell(i)` gives the spells of the loaded game until it returns NULL
  (`src/data/tables.c:330-333`): `SPELL_COUNT_DM` 25 (`src/data/include/spells.h`), `SPELL_COUNT_CSB` 29
  (`src/data/include/csb_image.h`). `Spell.symbols` packs the symbols; bits 24-31 hold the power symbol,
  0 = any power (`src/data/include/tables.h`; `spell_find`, `src/data/spells.c:14-31`). A symbol is
  `SPELL_SYMBOL_FIRST + step × 6 + index` (`src/data/spells.c:33-36`). The rune names are `g_rune_names`
  (`src/ui/ui.c:38-41`). The spell table has no spell names.

## Requirements

### The screen

- `2026-10-17-options-menu.md` is implemented first. It builds the shared "viewport screen open" state
  (automap, options menu, cheat screen; at most one open), the shared row helper, and the removal of the keys
  L, I, F and of the cheat lines of the read-out (`src/ui/advanced.c:56-62,147-150`;
  `src/platform/app.c:68-73`). This PRD uses them and does not change them again.
- In the advanced mode (`g_session.mode == GAME_MODE_ADVANCED`), C in the dungeon view (`in_dungeon_view`,
  no screen open) opens the cheat screen. C or Escape closes it; that Escape is used up and does not open the
  options menu. While another screen is open, C does nothing. The turnbased mode has no cheat screen.
- The screen covers the viewport, as the automap does, and the game time stops while it is open
  (the shared screen state).
- Page 1 rows: FULL LIGHT, IMMORTAL, FOOD AND WATER, ONE-HIT KILL, WALK THROUGH WALLS, REVEAL MAP (each with
  ON or OFF), QUICK SAVE F5, QUICK LOAD F9, SPELLS, CLOSE.
- The spell pages list every spell of the loaded game, one spell per line, with NEXT, BACK and CLOSE rows.
  A line shows the power rune and the runes as names, then as numbers, then the spell name, for example
  `FUL IR (X,4,4) FIREBALL`. X stands for any power. A spell with a fixed power symbol shows its name first and
  its number in place of X. The number of a rune is its place in its step, 1 to 6.
- Input:
  - Mouse: a click on a row switches the cheat or runs the item.
  - Keys: Up and Down (also W and S) select a row; Return switches or runs it; F5 and F9 run the quick save and
    the quick load; C and Escape close. Other keys and clicks do nothing.
- QUICK SAVE saves, and the screen stays open; the message area shows the result as today. QUICK LOAD closes
  the screen, then loads as today.

### The cheats

- Full light, immortality, food and water: as today.
- One-hit kill: melee, projectile and explosion damage above 0 kills the creature (the damage becomes its
  health). Damage 0 or below (`damage_all_creatures` can give it, `src/engine/group.c:788`) does nothing, as
  today.
  - `damage_creature` (`src/engine/group.c:741`) gets a source: party or dungeon. The cheat check comes after
    the invulnerable check (`src/engine/group.c:746-747`).
  - Party source: melee (`src/engine/group.c:1470`) and projectiles and explosions
    (`src/engine/projectile.c:160,282,551,560,583`).
  - Dungeon source, never one-hit kill: the closing door (`src/engine/group.c:815`) and the fall
    (`src/engine/group.c:2240`).
  - An invulnerable creature stays invulnerable.
  - Melee experience and the damage that the action area shows use the computed damage, as without the cheat.
- Walk through walls:
  - The party moves into a wall square or a solid fake wall square inside the map, without the bump damage.
  - A square outside the map stops the party, with the bump, as today.
  - A square with a creature stops the party, as today.
  - Doors, pits, stairs, teleporters and floor sensors work as normal.
  - No wall sensor of a wall square runs for the party on it: not on a move into or out of it, and not on a
    turn (`src/engine/move.c:408,410`).
  - A party on a wall square or a solid fake wall square can always leave it into a square that is not a wall
    or a solid fake wall, also with the cheat off (for example after the player switches it off, or after a
    load without cheats). Walls still block it.
  - A projectile stops at the wall before it reaches the party in the wall, and a creature cannot step into
    the wall square to reach it (assumption, checked first).
- Reveal map: while the cheat is on, the automap shows every square of the drawn level. The explored state does
  not change, so switching the cheat off shows the real explored squares again.

### The save file

- In the advanced mode, the save adds a cheat block before the explored block: `[original][cheat block]
  [length][explored block][length]`. The cheat block is the magic `CHTS`, a version byte 1, and one byte of
  cheat bits (bit 0 light, 1 immortal, 2 food, 3 kill, 4 walls, 5 map).
- The load reads the blocks from the end. It strips a block only when its length fits the file and its magic
  is known (`AMAP` or `CHTS`); the order does not matter. A second block with the same magic is ignored (the
  last one in the file counts). The scan stops at the first block that does not fit; the bytes before it go to
  `savefile_decode` (`src/engine/savegame.c:569`) as today. The stripped blocks never go to `savefile_decode`.
- An old save with only the explored block loads with the cheats off and the automap kept.
- On a load in the advanced mode, the cheats come from the cheat block. Without the block, with a damaged block
  (wrong size, version, or an unknown bit), or in the vanilla and turnbased modes, all cheats are off. A damaged
  cheat block does not change how the explored block loads.
- A new game turns all cheats off in `game_start_new`. `game_reset_session` (the start of a new or loaded game)
  no longer changes them.
- `savegame_load_from` sets the loaded cheats after a successful restore, next to `explored_load`
  (`src/engine/savegame.c:578-579`). So every load path gets them.
- A load that fails and puts the running game back keeps the cheats of the running game.

```gherkin
Feature: Cheat screen
  Scenario: Open and close the screen
    Given the advanced mode and the dungeon view
    When the player presses C
    Then the cheat screen covers the viewport and the game time stops
    When the player presses C or Escape
    Then the dungeon view shows again

  Scenario: No cheat screen outside the advanced mode
    Given the vanilla or the turnbased mode
    When the player presses C
    Then nothing happens

  Scenario: One-hit kill
    Given ONE-HIT KILL is on and a creature with 200 health in front of the party
    When a champion hits it for 3 damage
    Then the creature dies and drops its possessions

  Scenario: One-hit kill and a door
    Given ONE-HIT KILL is on
    When a door closes on a creature
    Then the creature takes the normal door damage

  Scenario: Walk through a wall
    Given WALK THROUGH WALLS is on and a wall in front of the party
    When the party moves forward
    Then the party stands on the wall square and no champion takes damage

  Scenario: Leave a wall with the cheat off
    Given the party stands on a wall square and WALK THROUGH WALLS is off
    When the party moves into a corridor square
    Then the party stands on the corridor square
    When the party moves forward into a wall
    Then the party does not move

  Scenario: The map border
    Given WALK THROUGH WALLS is on and the party on the last column of the map, facing out
    When the party moves forward
    Then the party does not move

  Scenario: Reveal the map
    Given REVEAL MAP is on
    When the player opens the automap
    Then the automap shows all squares of the level
    When the player switches REVEAL MAP off
    Then the automap shows only the explored squares

  Scenario: Quick save from the cheat screen
    Given the cheat screen is open
    When the player clicks QUICK SAVE F5
    Then QUICK.DAT holds the game and the message area shows GAME SAVED

  Scenario: The save keeps the cheats
    Given the advanced mode, FULL LIGHT and WALK THROUGH WALLS on
    When the player saves and loads the game
    Then FULL LIGHT and WALK THROUGH WALLS are on and the other cheats are off

  Scenario: An old save
    Given an advanced save with only the explored block
    When the player loads it in the advanced mode
    Then all cheats are off and the automap shows the explored squares

  Scenario: A vanilla save
    Given the vanilla mode
    When the player saves the game
    Then the save file is the same as today
```

## Invariants

- The vanilla mode does not change: a vanilla save has no extension, and the golden snapshots stay the same.
- The original part of an advanced save is the same as a vanilla save (`tests/port/test_modes.py:222-240`).
- A cheat can be on only in the advanced mode.
- No game tick runs while the cheat screen is open.
- At most one viewport screen is open: the cheat screen never opens over the automap or the options menu.
- The stripped extension blocks never reach `savefile_decode`.

## Decisions

- Key C: free in the advanced mode (`src/platform/app.c:57-82`, `src/ui/advanced.c:178-197`). L, I and F become
  free again.
- The screen is a viewport screen like the automap and the options menu (`2026-10-17-options-menu.md`): the
  same row helper, the same shared screen state (coordinator ruling). The options menu is implemented first and
  builds both, and it changes the shared files (`src/ui/advanced.c`, `src/platform/app.c`,
  `src/ui/gameloop.c:253`) one time.
- The cheat bits go into a new block of the save extension, not into the explored block: the explored block
  belongs to `engine/explored`, and its size check (`src/engine/explored.c:174-177`) rejects any extra byte.
  A separate block keeps both blocks simple, and the explored block does not change.
- The cheat block comes before the explored block, so the explored block stays the last block, where today's
  reader looks for it.
- The cheat reset moves from `game_reset_session` to `game_start_new` and to `savegame_load_from`:
  `start_game` runs after the load (`src/ui/gameloop.c:198-200`) and would turn the loaded cheats off.
- One-hit kill uses a source on `damage_creature`, not a check of the caller: the door calls the public
  `group_damage_all_creatures` (`src/engine/group.c:815`), so only a source keeps doors and falls out.
- Doors and falls never one-hit kill, because "damage the party deals" excludes the dungeon itself.
- An invulnerable creature stays invulnerable under one-hit kill, so the end of the game still needs the fuse.
- Walk through walls passes walls and solid fake walls only. Doors keep their rules, so the door state stays
  consistent.
  - Changed (owner, 2026-10-17, manual test): walk through walls passes every blocking square inside the map,
    also a door that is not open; only the map border and a creature stop the party. The door state does not
    change when the party enters the door square. A door that closes on the party in its square goes back open
    as normal (`process_door_animation`, `src/engine/event.c`), but with the cheat on the party takes no door
    damage, so a closed door the party walked into does not hurt it each tick. Wall sensors do not run for the party in a wall, because they react to things in the cells of
  the wall faces. The party can always leave a wall square, so a load without cheats cannot trap it
  (coordinator ruling). Assumption, checked first: the view and the explored marking work with the party on a wall
  square.
- Reveal map only changes what the automap draws, so switching it off restores the real explored state.
- The spell names come from a port table, keyed by the spell kind and type, because the game files have no
  spell names (coding standard 3.12 exception, allowlist line with this reason). Assumption, checked first:
  kind and type are unique among the DM and the CSB spells.
- Assumption, checked first: the game font has `(`, `)` and `,`; else the numbers are separated by spaces.

- One-hit kill needs no projectile owner: a creature projectile that hits a creature also kills it (owner,
  2026-10-17). An owner field would change the projectile record.

## Known consequences

- An object that the party drops while on a wall square stays in the wall square.
- The cheats persist in an advanced save; the manual says today that they are not saved (`docs/manual.md:155`).
- An older port build reads only the last block (the explored block); it does not read the cheat block.
  Whether it loads the rest of such a file is not checked and is not a requirement.

## Tests

- Port test of one-hit kill (a rule with more than one outcome): melee and projectile damage above 0 kill;
  damage 0 and below does not; an invulnerable creature lives; door and fall damage stay normal.
- Port test of walk through walls (a rule with more than one outcome): wall and solid fake wall pass without
  damage; map border, creature square and closed door stop; the cheat off stops at a wall; with the cheat off
  the party leaves a wall square into a corridor but not into a wall; no wall sensor runs on a move into the
  wall or a turn in it.
- Port test of the save blocks (a data format), through `savegame_load_from`: an advanced save with cheats
  loads them; a save with only the explored block, and one with a damaged cheat block, loads with the cheats
  off and the automap kept; blocks in either order load; a duplicate block is ignored; unknown trailing bytes
  stop the scan; a vanilla save has no extension; a load in the vanilla and turnbased modes turns the cheats
  off; a new game turns them off.
- Port test of the rune text of a spell (a rule with more than one outcome): a spell with any power, and a
  spell with a fixed power symbol.
- Existing tests that change:
  - `test_cheat_keys_work_only_on_the_advanced_automap` (`tests/port/test_modes.py:381-399`): the automap keys
    go away; the test goes away.
  - `tests/dmtest/game.py:202-208`: the `Session` structure gets the new cheat fields.
- No automated tests for the drawing of the screen, the reveal map drawing, and the key and mouse mapping (UI);
  the owner tests them manually.

## Docs

- `docs/manual.md:155`: the cheats move to the cheat screen, the new cheats, and the cheats are saved.
- `docs/manual.md:169`: a new row for C (the options menu PRD changes the automap row).
- `docs/manual.md:179`: the extension block also keeps the cheats.
- `docs/architecture.md:76,99`: the cheat block and the cheat screen module.
- `src/engine/include/game.h`: the comments "L, I, and F on the automap" and "Not saved".
- `src/ui/include/advanced.h`: the comment of `advanced_process_key`.
- `docs/todo.md:9-12`: remove the item.
