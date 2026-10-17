# Options menu on Escape: PRD

Date: 2026-10-17. Branch: `feature/2026-10-17-options`. Status: implemented.

## Goal

In the advanced mode, Escape opens an options menu: sound volume, music volume, and the music choice. The game
stays frozen while the menu is open. Today Escape only freezes the game (`docs/manual.md:116,161,170`), and the
music choice is keys 1-3 on the automap (`docs/manual.md:156`, `src/ui/advanced.c:151-154`). The vanilla mode
does not change.

A claim without a `file:line` source is an assumption; the plan task that relies on it checks it first.

## The port today

- Escape:
  - `command_process_key` (`src/ui/command.c:161-177`) first asks `advanced_process_key`. Outside the automap,
    `advanced_process_key` does not use Escape (`src/ui/advanced.c:157-175`).
  - In the advanced mode with an open inventory, Escape closes the inventory (`src/ui/command.c:172-174`).
    Else the key table gives `COMMAND_FREEZE_GAME`, and `freeze_game` (`src/ui/command.c:725-734`) stops the
    game time, patterns the movement, spell and action areas, shows GAME FROZEN, and sets the frozen input.
  - With the automap open, Escape closes the automap (`src/ui/advanced.c:144-146`).
- The automap is the model for a screen in the viewport:
  - `g_automap_open` stops the game time: `gameloop_run_frame` returns at once (`src/ui/gameloop.c:253-254`).
  - It opens only in the dungeon view: `in_dungeon_view` (`src/ui/advanced.c:39-44`): no inventory, dialog,
    freeze, sleep, candidate, death, frame wait, or won game.
  - `show_automap` draws into `ui_get_viewport()` and calls `ui_draw_viewport()` (`src/ui/advanced.c:69-76`);
    the text uses `text_draw` in the game font (`src/ui/advanced.c:47-65`; `FONT_CHAR_WIDTH` 6,
    `FONT_LINE_HEIGHT` 6, `FONT_BASELINE_GAP` 1 in `src/data/include/graphics.h`).
  - While it is open, `advanced_process_click` uses up every click (`src/ui/advanced.c:101-115`, called from
    `src/ui/command.c:147`), and `advanced_process_key` uses up every key (`src/ui/advanced.c:144-156`).
- The music choice:
  - Gate: `g_session.mode == GAME_MODE_ADVANCED` (`src/ui/advanced.c:151`). The turnbased mode has no music.
  - `music_select` (`src/ui/music.c:66`) sets `g_music_soundtrack` (`MUSIC_ORIGINAL`, `MUSIC_SUPER_NES`,
    `MUSIC_FM_TOWNS` in `src/ui/include/music.h`). Super NES does nothing in CSB (`src/ui/include/music.h`).
  - Every game start selects Original: `start_game` (`src/ui/gameloop.c:139`).
  - The read-out line "1 2 3 MUSIC ..." (`src/ui/advanced.c:63-64`) and the key words `KEY_1` to `KEY_3`
    (`src/platform/app.c:74-79`).
- The mixers:
  - `audio_mix` (`src/ui/audio.c:124-145`) writes the Amiga voices: the game sounds and the Amiga music voice
    of the start screens (`src/ui/anim.c:180,341`). It has no volume control of its own.
  - `music_mix` (`src/ui/music.c:143-160`) adds the MP3 music, divided by `MUSIC_VOLUME_DIVISOR` 6
    (`src/ui/music.c:24`), with clipping.
  - Both run on the game thread, one after the other (`src/platform/platform.c:254-255`).
- The saves folder:
  - `startend_save_dir` (`src/ui/startend.c:75-84`). The default is the data folder of the adventure
    (`src/ui/gameloop.c:73`); `--saves DIR` replaces it (`src/platform/app.c:484-485,517-518`;
    `docs/manual.md:44`). The folder must exist.
  - The port has no settings file. `data/file.h` has `file_read`, `file_write`, and `file_replace`.
- `start_game` (`src/ui/gameloop.c:134-151`) runs at every new and loaded game; it closes the automap
  (`src/ui/gameloop.c:138`).
- The dialog (`src/ui/dialog.c:114-189`) has a fixed layout of one to four buttons and a blocking wait for a
  click (`dialog_get_choice`). It cannot change a value in place.

## Requirements

### The shared viewport screen (built here, used by the cheat screen)

- This PRD is implemented first. It builds two parts that `2026-10-17-cheat-screen.md` uses:
  - One shared "viewport screen open" state for the automap, the options menu, and the cheat screen. At most
    one screen is open. It replaces the `g_automap_open` checks: the time stop (`src/ui/gameloop.c:253-254`),
    the input (`src/ui/advanced.c:101-115,144-156`), and the reset in `start_game` (`src/ui/gameloop.c:138`).
  - One row helper: it draws rows of text in the viewport with the game font and maps a click to a row.
- `advanced_process_key` checks the open screen first, before `in_dungeon_view`. `in_dungeon_view`
  (`src/ui/advanced.c:39-44`) does not check the screens, so the open screen check must come first.
- While a screen is open, the key that opens another screen does nothing. Escape closes only the open screen
  and is used up: it does not open the options menu in the same press.
- This PRD changes `src/ui/advanced.c:56-65,144-156` and `src/platform/app.c:68-79` one time for both PRDs:
  the read-out loses the cheat and music lines, and the keys L, I, F, 1, 2, 3 go away.

### The menu

- In the advanced mode, Escape in the dungeon view (`in_dungeon_view`, no screen open) opens the options menu.
  Escape with an open inventory closes the inventory, as today. Escape with the automap open closes the
  automap, as today. In other states (sleep, candidate, frame wait) Escape works as today.
- The menu covers the viewport. The game time stops while it is open. The music and the sounds that play go
  on, so the player hears a new volume at once.
- The menu has four rows:
  - SOUND: the sound volume, 0 (off) to 7 (full).
  - MUSIC VOLUME: the music volume, 0 (off) to 7 (full). The row stays active when the music is ORIGINAL.
  - MUSIC: ORIGINAL (no music), SUPER NES, FM-TOWNS. In CSB the row skips SUPER NES.
  - CONTINUE: closes the menu.
- Input while the menu is open:
  - Mouse: a volume row shows `-` and `+`; a click on them lowers or raises the value by one step, and stops
    at 0 and 7. A click on the MUSIC row selects the next music, with wrap. A click on CONTINUE closes the menu.
  - Keys: Up and Down (also W and S) select a row. On a volume row, Left and Right (also A and D) lower and
    raise the value and stop at 0 and 7. On the MUSIC row, Right selects the next music and Left the previous
    one, with wrap. Return on CONTINUE closes. Escape closes from every row.
  - All other keys and clicks do nothing.
- A music change in the menu calls `music_select`: the track of the party position starts at once, as the
  automap key does today.

### The volumes

- `audio_mix` scales its output by sound volume / 7.
- `music_mix` scales the MP3 samples by music volume / 7, on top of `MUSIC_VOLUME_DIVISOR`.
- At volume 0 the track goes on without sound, so its timing does not change (FM-Towns CSB waits for the end of
  the current track, `docs/manual.md:158`).
- Step 7 gives the same samples as today.
- `start_game` sets the volumes and the music: in the advanced mode from the settings file, in the vanilla and
  the turnbased modes 7, 7 and ORIGINAL. The start screens and the entrance use the volumes of the last game,
  or 7 before the first game.

### The settings file

- `SETTINGS.TXT` in the saves folder holds the two volumes and the music choice.
- Format: text lines `key=value`: `sound_volume=0..7`, `music_volume=0..7`,
  `music=original|super-nes|fm-towns`. The port ignores an unknown key.
- The parser is in `data/` (a file format); the menu is in `ui/`.
- Read: the advanced mode reads the file at each game start (`start_game`), in place of the fixed
  `MUSIC_ORIGINAL` (`src/ui/gameloop.c:139`). A missing file, a file above 1 KiB, or a line that the port
  cannot read gives the default of that value: sound 7, music volume 7, music ORIGINAL.
- `music=super-nes` in CSB gives ORIGINAL in the game. The port does not write the file for this; the file
  keeps `super-nes` until the player changes a value.
- Write: when the menu closes and a value changed, the port writes a temporary file and then replaces
  `SETTINGS.TXT` with it (`file_write`, `file_replace`). When the write fails, the message area shows UNABLE TO
  SAVE OPTIONS, and the values stay in use. When no value changed, the port does not write.
- The vanilla and the turnbased modes do not read or write the file.

```gherkin
Feature: Options menu on Escape
  Scenario: Escape opens the menu in the advanced mode
    Given the advanced mode and the dungeon view
    When the player presses Escape
    Then the options menu covers the viewport
    And the game time does not change while the menu is open

  Scenario: Escape in vanilla
    Given the vanilla mode and the dungeon view
    When the player presses Escape
    Then the game freezes and shows GAME FROZEN, as today

  Scenario: Escape with an open inventory
    Given the advanced mode and an open inventory
    When the player presses Escape
    Then the inventory closes and the menu does not open

  Scenario: Escape closes the automap only
    Given the advanced mode and the open automap
    When the player presses Escape
    Then the automap closes and the options menu does not open

  Scenario: One screen at a time
    Given the options menu is open
    When the player presses M
    Then the automap does not open

  Scenario: Change the sound volume
    Given the options menu is open and the sound volume is 7
    When the player clicks "-" on the SOUND row two times
    Then the sound volume is 5
    And the game sounds play at 5/7 of the full level

  Scenario: The volume stops at the limits
    Given the options menu is open and the music volume is 0
    When the player presses Left on the MUSIC VOLUME row
    Then the music volume stays 0

  Scenario: Select the music
    Given the options menu is open in Dungeon Master and the music is ORIGINAL
    When the player clicks the MUSIC row two times
    Then the music is FM-TOWNS and the track of the level starts
    When the player presses Right on the MUSIC row
    Then the music is ORIGINAL

  Scenario: CSB skips Super NES
    Given the options menu is open in CSB and the music is ORIGINAL
    When the player presses Right on the MUSIC row
    Then the music is FM-TOWNS

  Scenario: Close the menu
    Given the options menu is open
    When the player presses Escape or clicks CONTINUE
    Then the dungeon view shows again and the game time runs

  Scenario: The choice persists
    Given the player set the sound volume 3 and the music FM-TOWNS and closed the menu
    When the player starts the program again and loads a game in the advanced mode
    Then the sound volume is 3 and the music is FM-TOWNS

  Scenario: No write without a change
    Given the options menu is open
    When the player closes it without a change
    Then the port does not write SETTINGS.TXT

  Scenario: The write fails
    Given the saves folder is not writable
    When the player changes the sound volume and closes the menu
    Then the message area shows UNABLE TO SAVE OPTIONS and the new volume stays in use

  Scenario: A damaged settings file
    Given SETTINGS.TXT holds "sound_volume=9" and "music=loud"
    When a game starts in the advanced mode
    Then the sound volume is 7 and the music is ORIGINAL

  Scenario: Vanilla after an advanced game
    Given an advanced game with the sound volume 3
    When the player starts a game in the vanilla mode
    Then the sound volume is 7
```

## Invariants

- The vanilla mode does not change: Escape, sounds, and golden snapshots stay the same.
- The settings are not in the save file.
- Step 7 of both volumes gives the same mixer output as today.
- No game tick runs while the menu is open.
- At most one viewport screen is open.

## Known consequences

- In the advanced mode, Escape in the dungeon view no longer freezes the game and does not show GAME FROZEN.
- The automap keys 1, 2 and 3 go away.
- The music choice persists; the manual says today that it is not saved (`docs/manual.md:156`).
- A file `SETTINGS.TXT` appears in the saves folder after the first change.

## Decisions

- Gate: `g_session.mode == GAME_MODE_ADVANCED`, the gate of the automap music (`src/ui/advanced.c:151`). The
  turnbased mode has no keyboard control (`docs/manual.md:181`) and no music, so it gets no menu.
- The menu is a viewport screen like the automap, not a dialog: the dialog has a fixed layout of one to four
  buttons and a blocking wait (`src/ui/dialog.c:163-189`), and it cannot change a value in place. The menu uses
  the game font (`text_draw`) and the interface palette, as the automap read-out does.
- The menu stops the game time the way the automap does (`src/ui/gameloop.c:253-254`), not with
  `freeze_game`: the frozen input tables would take the clicks, and the GAME FROZEN text would cover the menu.
  The player sees the same result: no time passes.
- One shared screen state and one row helper for the automap, the options menu and the cheat screen
  (coordinator ruling). The options menu is implemented first, so it builds both.
- The settings are player preferences, not game state: a small text file in the saves folder, not the save
  file. The saves folder exists and is writable, and `--saves` already chooses it. A text file is easy to read
  and to repair.
- Each adventure keeps its own settings file in its saves folder (owner, 2026-10-17). The default saves folder
  is the data folder of the adventure (`src/ui/gameloop.c:73`).
- The port reads the file at each game start, so a manual edit takes effect at the next game. It writes the
  file only on a change, when the menu closes, through a temporary file, so a failed write does not damage the
  old file.
- The read limit of 1 KiB keeps a large wrong file out of the parser; a real settings file has three short lines.
- The volume scale is linear: sample × step / 7. Eight steps are few, and a linear scale is simple to check.
- The sound volume scales all of `audio_mix`: the game sounds and the Amiga music voice of the start screens.
- MUSIC VOLUME stays active at ORIGINAL: it sets the level for a later music choice, and a row that changes
  its state would confuse the keyboard selection.
- In CSB the MUSIC row skips SUPER NES, because `music_select` ignores it there; a row value that does nothing
  would confuse the player.
- New UI module for the menu (name in the plan; not `options`, which `platform/` uses). The settings parser is
  a pure function in `data/`, so a port test can call it through the library.

## Tests

- Port test of the settings parser (a data format): all three keys, each default for a missing line, a value
  out of range, an unknown key, an unknown music name, a file without a final newline, a file above 1 KiB,
  and `music=super-nes` in CSB (ORIGINAL).
- Port test of the mixer scale (a rule with more than one outcome): `audio_mix` of one sample at step 7 gives
  today's value, at step 3 gives 3/7 of it, at step 0 gives 0.
- Port test: the game time does not change while the menu is open (the same check as
  `test_automap_stops_the_game_time_and_draws_the_map`, `tests/port/test_modes.py:141`).
- Port test of the volume reset (a rule with more than one outcome): after an advanced game with volume 3, a
  vanilla game start sets 7.
- Port test of the screen rule (a rule with more than one outcome): Escape with the automap open closes only
  the automap; with the menu open, M does not open the automap.
- Existing tests that change:
  - `test_escape_rule` (`tests/port/test_modes.py:195-209`): in the advanced mode the second Escape opens the
    menu, not the freeze.
  - `test_music_keys_work_only_on_the_advanced_automap` (`tests/port/test_music.py:77-88`): the keys 1-3 go
    away; the test goes away.
- No automated tests for the drawing of the menu and its key and mouse mapping (UI); the owner tests them
  manually.

## Docs

- `docs/manual.md:116`: Escape row (vanilla stays).
- `docs/manual.md:156`: the music choice moves to the options menu and persists.
- `docs/manual.md:161`: Escape in the advanced mode opens the options menu.
- `docs/manual.md:169-170`: the automap row loses 1, 2 and 3; the Escape row.
- `docs/manual.md`: a new item for the options menu and `SETTINGS.TXT`.
- `docs/architecture.md:99,112`: the new modules (menu, settings parser, shared screen), and the volume of
  `music_`.
- `src/ui/include/advanced.h`, `src/ui/include/music.h`: the comments that name the keys 1, 2, and 3.
- `docs/todo.md:5-7`: remove the item.
