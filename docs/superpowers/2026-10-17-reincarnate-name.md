# Name entry of a reincarnated champion: PRD

Date: 2026-10-17. Branch: `feature/2026-10-17-reincarnate-name`. Status: reviewed (one `prd-reviewer` pass, as
part of the todo pass of 2026-10-17); the plan is not written yet.

## Goal

After REINCARNATE the port asks for a new name and title, as the original does. Today the port keeps the old
name (`champion_click_rebirth_panel`, `src/engine/champion.c:448-476`; `docs/rules/champions.md:44`).

A claim without a `file:line` source is an assumption; the plan task that relies on it checks it first.

## The original

`champion_rename` (KAOS.c:16384-16548, called from KAOS.c:16604):

- Panel: `g_panel_content` 8 (KAOS.c:16411-16412). The original fills zone 553 (`ZONE_NAME_TITLE`) with
  color 12, blits graphic 27 into `ZONE_PANEL` with transparent color 4, and prints 7 underscores in zone 574
  and 19 underscores in zone 575 in color 13 (KAOS.c:16413-16420). Pointer: the arrow (KAOS.c:16423-16426).
  No palette change.
- At entry the original clears the name and the title (KAOS.c:16431-16432).
- Input: the mouse table `KAOS_MOUSE_INPUT_NAME_ENTRY` (`0x10378c`, `src/data/kaos_addresses.h:182`,
  `TABLES_MOUSE_NAME_ENTRY`, `src/data/tables.h:467`) and the key table at `0x103994` (not in the port yet).
  The secondary tables are empty. The original restores the earlier tables only on a successful OK
  (KAOS.c:16433-16442, 16480-16483).
- Commands: 165 backspace, 166 OK, 167 next field, 168-193 A-Z, 194 `,`, 195 `.`, 196 `;`, 197 `:`,
  198 space (character table at `0x100c46`). A right click anywhere types a space (mouse table, screen zone 2).
  Keys: Backspace, Return (next field, not OK), A-Z in both cases, `,` `.` `;` `:`, Space. No key gives OK.
- Cursor: `_` in color 9 at the input position; a typed character prints in color 13 on 12 (KAOS.c:16457,
  16499, 16521, 16526-16536).
- Rules (KAOS.c:16445-16547):
  - The name holds up to 7 characters; the 7th character moves the input to the title.
  - The title holds up to 19 characters; at 19 the cursor hides and the entry ignores characters.
  - The entry does not accept a space as the first character of a field.
  - Next field works only in the name field with a non-empty name.
  - Backspace on an empty name does nothing; at title position 0 it goes back to the name and deletes the last
    name character; otherwise it deletes the previous character.
  - OK does nothing in the name field while the name is empty; an empty title is allowed.
  - On OK the entry removes the trailing spaces of the name. When the name is equal to the name of another
    champion of the party, the entry refuses it without a message, puts the untrimmed name back and goes on.
- After the rename: clear the experience, add the 12 random statistic points, print "<new name>
  REINCARNATED." in the champion color, close the inventory, unfreeze the game (KAOS.c:16604-16632).
- No tick runs during the entry: `champion_rename` is a blocking input loop (KAOS.c:16445-16547).

## Requirements

- `champion_click_rebirth_panel` does the unlink and the sensor change. For reincarnate it starts the name
  entry, except in the turnbased mode, where it reincarnates at once with the old name, as today.
- A new engine function runs on OK: it reincarnates (experience and statistics), prints the message, closes the
  inventory and unfreezes. `click_in_candidate_panel` (`src/ui/command.c:355-377`) closes the inventory at once
  only for resurrect, cancel and the turnbased reincarnate.
- During the entry `gameloop_run_frame` asks for no tick, and `gameloop_tick` does not unfreeze the game
  (today it unfreezes when `candidate_champion_index < 0`, `src/ui/gameloop.c:316-317`). Only OK ends the entry.
- During the entry `amiga_key` (`src/platform/app.c:85-129`) gives the plain Amiga key word for each letter in
  every mode; `advanced_letter_key`, `advanced_process_key` (`src/ui/advanced.c:156-195`) and the Tab and
  Escape rule of `command_process_key` (`src/ui/command.c:171-173`) do nothing.
- Dungeon Master and Chaos Strikes Back. The CSB mouse table is at `0x10420c` of the CSB image
  (`src/data/csb_image.c:140`). Assumption, checked first: the CSB `champion_rename` (`0x116312`,
  `docs/reference/csb-appb-symbols.csv:393`) has the same rules, and its key and character tables exist in
  APPB.FTL; the plan adds them to the CSB image.

```gherkin
Feature: Name entry of a reincarnated champion
  Scenario: A new name and title
    Given the vanilla or the advanced mode and a candidate in front of the party
    When the player clicks REINCARNATE, types "ZED", presses Return, types "THE NEW", clicks OK
    Then the champion has the name "ZED" and the title "THE NEW"
    And the message area shows "ZED REINCARNATED."

  Scenario: No time passes
    Given the name entry is open
    When the player waits
    Then the game time does not change

  Scenario: A duplicate name
    Given a party with the champion "ZED"
    When the player enters the name "ZED" for a reincarnated champion and clicks OK
    Then the entry stays open, the input is at the end of the name, the title is empty, and no message shows

  Scenario: The advanced letters
    Given the advanced mode and the name entry
    When the player types "WASDQEM"
    Then the name shows "WASDQEM" and the party does not move

  Scenario: The turnbased mode
    Given the turnbased mode
    When the program player sends recruit with reincarnate
    Then the champion keeps the old name, as today
```

## Invariants

- No tick runs between the REINCARNATE click and OK.
- The save file format does not change.

## Known consequences

- Ctrl+Q does nothing during the name entry, because the name key table replaces the interface key table.

## Decisions

- The turnbased mode keeps the old name on reincarnate (owner, 2026-10-17). The protocol does not change.
- After a refused duplicate, the input goes to the end of the name field and the title clears. A full name
  (7 characters) takes no more characters; Backspace and Next field work. The decompiled code points the input
  back at the name buffer but keeps the title state (KAOS.c:16491), so later characters would go into the name.
  The port uses a defined state and records it as a deviation; the quirk is not verified in the emulator.
- In vanilla, `amiga_key` gives the letters, Space, Backspace and `,` `.` `;` `:` their Amiga key words; the
  active key table decides what they do. Assumption, checked first: the game key tables of the original have no
  entries for these keys, so the game outside the name entry does not change.
- The rename rules (fields, limits, backspace, OK, duplicate) live in `engine/champion.c` as a small state
  machine; the drawing and the input tables live in `ui/`.
- CSB is in scope, because the port plays CSB and CSB has the same reincarnate panel.

## Tests

- Port tests of the rename rules (a state machine): the 7 and 19 limits, the switch at 7, a first space
  refused, Backspace across the fields, OK with an empty name ignored, the trailing-space trim, the duplicate
  refusal, a full name that takes only Backspace and Next field.
- No automated tests for the panel drawing and the key mapping (UI); the owner tests them manually.

## Docs

- `docs/rules/champions.md:44`: the rename.
- `docs/formats/dungeon.md:188`: remove the name entry deviation; add the duplicate deviation.
- `docs/manual.md:52`: the rename is available.
- `docs/llm/llm-mode.md:544`: the turnbased mode keeps the name.
- `src/ui/command.c:779`: the comment of the ignored commands.
- `docs/todo.md`: remove the item.
