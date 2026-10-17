# Save slots: PRD

Date: 2026-10-17. Branch: `feature/2026-10-17-options`. Status: implemented.

## Goal

In the advanced mode, the player saves to one of 8 save slots and resumes from one of them (owner decision,
2026-10-17; `docs/todo.md:14`). Today the game has one save file per adventure and the quick save.

A claim without a `file:line` source is an assumption; the plan task that relies on it checks it first.

Implementation notes: the font has `-` and `:`. The message panel of the one-choice dialog is 201 x 76 pixels, so a
row is 8 pixels high (9 rows: 72 pixels). `QUICK SAVE  LEVEL 14  2026-10-17 14:32` is 227 pixels and does not fit
the viewport, so the quick save row is labelled `QUICK`. The code numbers the slots 0-7 (slot 0 is `DMGAME.DAT`);
the lists show 1-8. The info read also checks the byte sum word of the dungeon part.

## The port today

### Files

- One save file and one backup per adventure: `DMGAME.DAT`, `DMGAME.BAK` (DM) and `CSBGAME.DAT`, `CSBGAME.BAK`
  (CSB) (`src/data/adventure.c:27-32`). The quick save is `QUICK.DAT`, without a backup
  (`src/engine/savegame.c:28`, `594-617`).
- The folder: the data folder, or `--saves DIR` (`src/ui/startend.c:75-84`, `src/platform/app.c:484-485`,
  `517-518`; `docs/manual.md:96`).
- `savegame_save` writes the file of the adventure (`src/engine/savegame.c:325-329`). `write_save_file` first
  renames the old file to the backup name (`src/engine/savegame.c:331-342`), as the original
  (`docs/formats/save.md:209`).
- `savegame_load_from` reads the file, else the backup; after a load from the backup it renames the backup back to
  the file name (`src/engine/savegame.c:551-587`; `docs/formats/save.md:216-222`). A file that exists but does not
  decode is `SAVEGAME_DAMAGED`; the backup is not tried then (`src/engine/savegame.c:559-562`).
- `savegame_save_as` and `savegame_load_from` already take a file name and a backup name (`src/engine/include/savegame.h`).
- The CSB prison: the save merges the party into `MINI.DAT` and writes the result to `CSBGAME.DAT`
  (`savegame_make_adventure`, `src/engine/savegame.c:456-479`; `src/ui/startend.c:109-128`).

### Dialogs

- The DM save dialog: CANCEL, SAVE, QUIT; after SAVE the dialog stays open, in advanced with the message GAME SAVED
  (`startend_save_game`, `src/ui/startend.c:174-204`; `docs/manual.md:98`).
- The CSB save dialog: CANCEL, SAVE AND PLAY, SAVE AND QUIT; it closes after a save (`save_game_csb`,
  `src/ui/startend.c:152-172`; `docs/manual.md:88`).
- RESUME: `startend_load_saved_game` loads the one file; a missing file asks CAN'T FIND SAVED GAME with LOAD SAVED
  GAME and CANCEL; a damaged file shows THE GAME IS DAMAGED! (`src/ui/startend.c:206-235`). The DM entrance calls
  it at `src/platform/app.c:329-336`, the CSB screen at `src/platform/app.c:498`.
- RESTART THIS GAME on THE END calls the same function (`src/ui/startend.c:383`); THE END offers it only when
  `g_session.saved_game_exists` is true (`src/ui/startend.c:424`).
- The dialog has at most 4 choices (`src/ui/include/dialog.h`: `DIALOG_CHOICE_COUNT`); the choice zones and the
  divider patches come from the original layout (`src/ui/dialog.c:40-73`); the clicks use the original mouse
  tables, one per choice count (`src/ui/dialog.c:163-184`). Thus 8 slots do not fit into the choices.
- The dialog graphic, the version text and the messages are drawn in the viewport (224 x 136,
  `src/ui/include/view.h`: `VIEWPORT_MAX_W`, `VIEWPORT_MAX_H`) with the game font (`src/ui/dialog.c:75-133`).
  On the inactive screen (entrance) the viewport is centered (`src/ui/dialog.c:27-36`, `ZONE_DIALOG_ON_SCREEN`).
- A port click hook exists: `command_process_click` gives a click first to `advanced_process_click`
  (`src/ui/command.c:147`, `src/ui/advanced.c:101-114`), which takes the automap clicks.

### Quick save and the LLM protocol

- F5 and F9 in the advanced mode, in the dungeon view (`src/ui/advanced.c:122-138`, `157-165`). A quick save is not
  a saved game: the saved-game flag and the save time stay (`src/engine/savegame.c:594-617`).
- The `turnbased` protocol: `save` calls `startend_save_current_game`, `load` calls `savegame_load` on the file of
  the adventure (`src/control/control.c:75-112`).

## Requirements

### Slots and file names

- Slot 1 is the file of today: `DMGAME.DAT` / `DMGAME.BAK`, `CSBGAME.DAT` / `CSBGAME.BAK`.
- Slots 2 to 8: `DMGAME2.DAT` to `DMGAME8.DAT` with `DMGAME2.BAK` to `DMGAME8.BAK`; CSB `CSBGAME2.DAT` to
  `CSBGAME8.DAT` with `CSBGAME2.BAK` to `CSBGAME8.BAK`. All names are 8.3 names.
- Each slot has its own backup with the rule of today: a save renames the old file of the slot to the backup of
  the slot; a load without the file reads the backup of the slot and renames it back.
- `QUICK.DAT` does not change. It is not a slot.
- The engine gets one name function for a slot (slot 1 to 8, adventure) and save, load and slot info functions
  that take the slot. `savegame_save` and `savegame_load` stay and use slot 1.

### Slot info

- For each slot the game reads the file, else the backup, and shows one line:
  - `n  EMPTY` when neither file exists;
  - `n  DAMAGED` when the file does not decode, with the rule of `savegame_load_from`;
  - else `n  LEVEL l  YYYY-MM-DD HH:MM`: `l` is the level of the party map plus 1; the date and time are the local
    modification time of the file that the line reads.
- The info read is a new read-only parser in `engine/savegame.c`:
  1. `file_read` the file; strip the extension blocks at the end of the file: the automap block (`find_automap`,
     `src/engine/savegame.c:311-323`) and, when the cheat-screen PRD is in, the cheat block `CHTS`
     (`docs/superpowers/2026-10-17-cheat-screen.md:104-107`).
  2. `savefile_decode` the rest. It works on its own copy of the bytes (`src/engine/savefile.c:99-107`).
  3. The party map index is the word `SAVE_GLOBAL_PARTY_MAP_INDEX` of block 0 (`src/engine/savegame.c:97`).
  4. The level is bits 0-5 of word 8 of that map descriptor in the dungeon part (`docs/formats/dungeon.md:48-50`,
     `83`). The read uses the accessor of the module that owns the map descriptor (coding standard 1.9).
- The info read does not call `restore_game` (it frees the running dungeon, `src/engine/savegame.c:571`),
  `explored_load` or the cheat load. It does not change the game state, the explored state or the cheats.
- A `QUICK.DAT` of the other adventure: DM and CSB use the same name (`src/engine/savegame.c:28`), and they share it
  when `--saves` gives one folder to both (`docs/manual.md:96`). The header layout of `g_adventure` decides the
  decode (`src/engine/savefile.c:37-45`, `108-111`), so the file of the other adventure fails the header check and
  shows `DAMAGED`. The file does not name its adventure in a field that both layouts share. A CSB quick save in the
  prison is a plain save (`docs/manual.md:91`) and shows `LEVEL 1`.
- Assumption, checked first: the game font has the characters `-` and `:`. Else the line is
  `n  LEVEL l  DD.MM.YYYY HH.MM`.

### The slot list

- One new dialog: the dialog graphic and the version text as `dialog_draw`, the rows in the game font in the
  viewport, and one choice CANCEL in the single-choice zone (`ZONE_CHOICE_SINGLE` with the one-choice patch).
- The save list has 8 rows (slots 1-8). The load list has 9 rows: slots 1-8, then `QUICK SAVE` with its info
  (`EMPTY`, `DAMAGED`, or the level and the time of `QUICK.DAT`).
- Layout budget: the viewport is 224 x 136. A row is 9 pixels high (6 character rows, `FONT_LINE_HEIGHT`, and 3
  blank rows); 9 rows are 81 pixels. The longest line, `8  LEVEL 14  2026-10-17 14:32`, has 29 characters of 6
  pixels (`FONT_CHAR_WIDTH`, `src/data/include/graphics.h`): 174 pixels. Assumption, checked first: the rows fit
  between the top of the dialog graphic and the single-choice zone of CANCEL.
- A click on a row selects it. CANCEL or Escape closes the list without a choice.
- The load list: an EMPTY or DAMAGED row is gray and a click on it does nothing. The save list: every row can be
  selected; a save over a DAMAGED file makes that file the backup of the slot.
- The row boxes are port layout constants of the new module. The clicks reach the module through a hook as
  `advanced_process_click` (`src/ui/command.c:147`), while the list is open. The click position on the inactive
  screen is moved by the viewport position of `ZONE_DIALOG_ON_SCREEN`.
- While the list is open, the module takes every key through a hook as `advanced_process_key`
  (`src/ui/command.c:163`): Escape closes the list, all other keys do nothing. So no game command runs, and Escape
  does not freeze the game (`src/ui/command.c:172`).
- The selected row shows the pressed state for 5 frames, as a dialog choice (`src/ui/dialog.c:135-161`).

### Save and load in the advanced mode

- The current slot: the slot of the last save or load from a slot in this run; at the program start it is 1. It
  lives in `ui/startend.c`. It is not in the session: `game_reset_session` runs at every game start
  (`src/ui/gameloop.c:136`).
- DM: SAVE opens the save list. A selected slot saves to it, sets the current slot, and the dialog of today comes
  back with GAME SAVED. CANCEL in the list goes back to the dialog of today.
- CSB: SAVE AND PLAY and SAVE AND QUIT open the save list first, then save to the slot and go on as today. CANCEL
  in the list goes back to the game, as CANCEL of today. In the prison, the new adventure goes into the selected
  slot (`savegame_make_adventure` takes the slot).
- RESUME on the DM entrance and on the CSB screen opens the load list. A slot loads with the dialogs of today
  (LOADING GAME, then GAME LOADED) and sets the current slot. QUICK SAVE loads `QUICK.DAT` with the rules of F9
  (`savegame_quick_load`): the saved-game flag stays false, so THE END offers no RESTART until the first save.
  CANCEL shows the entrance or the CSB screen again.
- RESTART THIS GAME loads the current slot, without the list. When the slot file and its backup are missing, the
  dialog CAN'T FIND SAVED GAME of today shows (`src/ui/startend.c:227-233`); in the advanced mode LOAD SAVED GAME
  opens the load list, CANCEL goes on to the credits as today. A damaged file shows THE GAME IS DAMAGED!
  (`src/ui/startend.c:218-225`), then the credits and the entrance, as today (`src/ui/startend.c:383-386`).
- The cheats: a slot load sets the cheats from the cheat block of the slot file, the QUICK SAVE row from
  `QUICK.DAT`, with the rules of the cheat-screen PRD (`docs/superpowers/2026-10-17-cheat-screen.md:108-110`).
- The quick save paths: F5 and the QUICK SAVE F5 row of the cheat screen
  (`docs/superpowers/2026-10-17-cheat-screen.md:72`, `81`). The save list has no quick save row.
- `startend_save_current_game` takes the slot. The LLM protocol passes slot 1.
- QUIT asks before it quits when no save or load is recent, as today (`src/ui/startend.c:96-106`).

```gherkin
Feature: Save slots in the advanced mode
  Scenario: Save to a slot
    Given an advanced DM game on level 3
    When the player clicks SAVE in the save dialog and clicks slot 4
    Then the game writes DMGAME4.DAT and the old DMGAME4.DAT becomes DMGAME4.BAK
    And the save dialog shows GAME SAVED

  Scenario: Resume from a slot
    Given DMGAME.DAT and DMGAME4.DAT exist
    When the player clicks RESUME on the entrance
    Then the list shows "1  LEVEL ..." and "4  LEVEL 3 ..." and EMPTY for the other slots
    When the player clicks slot 4
    Then the game loads DMGAME4.DAT

  Scenario: Resume the quick save
    Given QUICK.DAT exists
    When the player clicks RESUME and clicks the QUICK SAVE row
    Then the game loads QUICK.DAT

  Scenario: Restart after death
    Given the player loaded slot 4
    When all champions die and the player clicks RESTART THIS GAME
    Then the game loads DMGAME4.DAT

  Scenario: Restart without the slot file
    Given the player loaded slot 4 and DMGAME4.DAT and DMGAME4.BAK are deleted
    When all champions die and the player clicks RESTART THIS GAME
    Then the dialog says CAN'T FIND SAVED GAME
    When the player clicks LOAD SAVED GAME
    Then the load list shows

  Scenario: Vanilla
    Given a vanilla game
    When the player clicks SAVE or RESUME
    Then the dialogs and the file names are those of today
```

## Invariants

- Vanilla does not change: dialogs, files, golden snapshots.
- One gate for the lists, the slots and the current slot: `g_session.mode == GAME_MODE_ADVANCED`. Not
  `g_advanced_features`: it is also true in the `turnbased` mode (`src/ui/select.c:78`). The GAME SAVED messages
  keep their gate of today (`src/ui/startend.c:168`, `190`).
- The `turnbased` mode never opens a list. The LLM protocol does not change: `save` calls
  `startend_save_current_game` with slot 1 and `load` calls `savegame_load` (slot 1) directly
  (`src/control/control.c:75-112`), not the dialogs.
- The info read never changes the game state, the explored state or the cheats.
- A save of today is slot 1 without a conversion. A vanilla game reads the slot 1 file of an advanced game.
- The save file format does not change (`docs/formats/save.md`).

## Decisions

- Slot 1 keeps `DMGAME.DAT` and `CSBGAME.DAT`: old saves, vanilla, the LLM protocol, CEDT (LOAD CHAMPIONS reads
  `DMGAME.DAT`, `docs/csb/new-adventure.md:52`) and the tests stay valid without a change.
- Names `DMGAMEn.DAT`: 8.3 names on every file system, and the same stem as the original.
- One backup per slot: the rule of the original per file, no new rule.
- The slot shows the level and the file date, no name. A name needs a text entry, which is out of scope. The file date
  tells the player which save is the latest. The game time is not real time: a tick waits at most
  `WAIT_FOR_INPUT_FRAMES` (`src/ui/include/gameloop.h`), and input makes it earlier.
- No question before a save over a used slot: the backup of the slot keeps the old game.
- A list with its own rows instead of more dialog choices: the original mouse tables and divider patches exist
  only for 1 to 4 choices (`src/ui/dialog.c:40-59`, `163-167`). The rows reuse the dialog graphic and the game
  font, so the list looks like the original dialog.
- The quick save is in the load list only: F5 and the cheat screen row stay the only quick save paths.
- Clicks only, no number keys: the keys 4 to 8 have no key word (`get_advanced_key`, `src/platform/app.c:56-81`
  maps only 1 to 3), and Q is turn left in the advanced mode (`advanced_letter_key`, `src/ui/advanced.c`).
- A `QUICK.DAT` of the other adventure shows DAMAGED: the file has no field that names its adventure for both
  header layouts, and the decode of `g_adventure` fails on it (`src/engine/savefile.c:37-45`, `108-111`). No new
  check.
- RESTART THIS GAME without its file: LOAD SAVED GAME opens the load list, so the player can pick another slot;
  the other outcomes stay as today.
- The current slot is UI state in `ui/startend.c`, not in the session and not in the save file.


## Known consequences

- Vanilla and CEDT see only slot 1. Slots 2-8 and `QUICK.DAT` show only in the advanced mode.
- The file date is the modification time: a copy of the file or a clock change can make it wrong.
- No question before a save over a used slot, and one backup per slot: the second save over a slot loses the
  oldest game of that slot.

## Tests

- `tests/port/test_save.py` (rules with more than one outcome):
  - the file name of each slot: slot 1 `DMGAME.DAT`, slot 4 `DMGAME4.DAT`, CSB slot 4 `CSBGAME4.DAT`;
  - a save to slot 4 twice keeps the first file as `DMGAME4.BAK` and does not touch `DMGAME.DAT`;
  - a load of slot 4 without `DMGAME4.DAT` reads `DMGAME4.BAK` and renames it;
  - CSB: `savegame_make_adventure` into slot 3 writes `CSBGAME3.DAT` and does not touch `CSBGAME.DAT`
    (`tests/port/test_csb_adventure.py`).
- `tests/port/test_save.py` (data format): the slot info of a saved new game gives level 1; an empty slot gives
  EMPTY; a truncated file gives DAMAGED; a file with an automap block gives the same level; the info read leaves
  the game state and the explored state unchanged.
- No tests for the list and the dialogs. They are UI; the owner tests them manually.

## Docs

- `docs/manual.md:94-101` (Saved games): the slots, the file names, the list, RESUME with the quick save.
- `docs/manual.md:87-88`, `91`: the CSB RESUME and save dialog in the advanced mode.
- `docs/formats/save.md:10`: the slot file names.
- `docs/architecture.md`: the new module of the list; `engine/savegame.h` row.
- `docs/todo.md:14-16`: remove the item.
