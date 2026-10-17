# Credits on the entrance: PRD

Date: 2026-10-17. Branch: `feature/2026-10-17-entrance-credits`. Status: implemented (todo pass of 2026-10-17).

## Goal

A click on the credits zone of the entrance shows the credits with the palette fades of the original. Today the
port ignores the click (`process_other_command`, `src/ui/command.c:779`; deviation `src/ui/entrance.h:17`,
`docs/formats/dungeon.md:185,188`; `docs/todo.md:9`).

A claim without a `file:line` source is an assumption; the plan task that relies on it checks it first.

## The original

- The entrance mouse table has command 203 on zone 411 (`KAOS_MOUSE_INPUT_ENTRANCE`,
  `src/data/kaos_addresses.h:169`). No key gives 203.
- `entrance_show_credits` (KAOS.c:33268-33295): hide the pointer, fade the palette to black
  (`palette_fade_to(g_chip_block + 0x188)`), draw graphic 5 (`GFX_CREDITS`) to the screen, fade to the credits
  palette (`0x100a46`), drop the pending input (it also waits while a mouse button is held, KAOS.c:33279-33284),
  wait up to 1800 frames (36 s) or until any input (KAOS.c:33285-33293), then draw the entrance again
  (`process_entrance` redraws while `g_new_game == 0xcb`, KAOS.c:33224-33236).
- `g_chip_block + 0x188` is black: the chip block is allocated with `MEMF_CLEAR` (KAOS.c:3068), and nothing
  writes these 32 words; every use is a `palette_fade_to` argument.
- `entrance_draw` always fades to black and then to the entrance palette (KAOS.c:33150, 33174).
- The endgame credits fade too (KAOS.c:31477-31490); the port's `show_credits` does not (out of scope).
- The fade: `palette_fade_to` (KAOS.c:4549-4561) copies the 32 current colors and calls `palette_fade`
  (KAOS.c:4463-4544):
  - 15 steps, one vertical blank per step, so 15 frames per fade; each step sets both palette halves.
  - The target has 16 colors and serves both halves (`target[i & 0xf]`).
  - Per 4-bit channel: d = |target − start|; the step adds `table[step * 16 + d]` (0 or 1, negated when the
    target is lower) to the work color. After 15 steps each channel is at the target.
  - The step table: 15 rows × 16 bytes at offset `0x248` of VDEO (`reference/decomp/dm/VDEO.c:2135`); column k
    sums to k. `docs/reference/symbols/range-1-3.md:117` describes the fade. Assumption, checked first: the
    table is readable from the game files the port loads; otherwise the port names it as a table that is not in
    the loaded files (coding standard 3.12 exception, allowlist).

## The port today

- `entrance_draw` (`src/ui/entrance.c:77-82`) clears the screen, sets the entrance palette, draws the doors; no
  fade. No fade code exists in `src/`.
- The pointer: `set_pointer` (`src/platform/app.c:141-150`) hides it only in the game
  (`src/ui/gameloop.c:415`); the entrance has no way to hide it yet.
- In the turnbased mode `g_frame_hook` is NULL (`src/platform/app.c:378`), so a fade ends at once.
- `tests/port/test_save.py:269-289` counts the frames of the endgame QUIT path to the entrance (`< 2000`); the
  entrance fades add 30 frames.

## Requirements

- A palette fade in `ui/` follows `palette_fade_to` and `palette_fade`. It runs its frames through
  `g_frame_hook` (`ui/frame.h`).
- Command 203 sets `g_entrance_choice` to a new value `ENTRANCE_CREDITS`; `run_entrance`
  (`src/platform/app.c:284-314`) calls the entrance credits, then `show_entrance` (`src/platform/app.c:240-246`),
  which restores the entrance input and the pointer.
- The entrance credits reuse the credits drawing of `show_credits` (`src/ui/startend.c:391-400`), moved into one
  shared function; the fades and the 1800-frame limit are new.
- Every `entrance_draw` fades as in the original, also the first one.

```gherkin
Feature: Credits on the entrance
  Scenario: Show the credits
    Given the Dungeon Master entrance
    When the player clicks the credits zone
    Then the screen fades to black, the credits fade in
    And a click, a key or 36 seconds return to the entrance with a fade
```

## Decisions

- The fade follows the original (owner, 2026-10-17); so every `entrance_draw` fades (KAOS.c:33150, 33174).
- Out of scope: a fade for the dialogs and the endgame screens (`docs/manual.md:52`).

## Tests

- None (UI); the owner tests it manually.

## Docs

- `docs/formats/dungeon.md:185,188`: remove the credits deviations.
- `docs/manual.md:52`: the credits fade is available.
- `src/ui/entrance.h:17`, `src/ui/command.c:779`: the deviation comments.
- `docs/todo.md`: remove the item.
