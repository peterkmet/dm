# Final refactor: manual test checklist

Branch `feature/2026-10-16-final-refactor`. The automated gate is green: `make check` (749 tests), `make windows`,
and the PRD acceptance (Task 132). This list holds what no test covers: the screen, the input and the sound.

## Start and options

- [ ] `make` builds with 0 warnings; `build/dm` starts and shows the start screens (logo, title, menu).
- [ ] `build/dm --help` prints the option list; `build/dm --frams` prints an error and exits with 1.
- [ ] `build/dm --adventure xyz` prints `--adventure must be dm or csb`.
- [ ] `build/dm --filter scale2x` (and `scale3x`) draws a smooth picture; without `--filter` the picture is sharp.
- [ ] The mouse pointer (arrow and hand) looks as before, in the window and in full screen.

## Dungeon Master

- [ ] Enter the dungeon (entrance door animation), choose champions in the Hall of Champions.
- [ ] Walk, turn, open a door, press a button, pick up and drop an object, throw it.
- [ ] Fight a creature: the attack, the damage splats (small and large), a champion that dies.
- [ ] Cast a spell (the spell area, the rune symbols, a fireball explosion).
- [ ] Inventory: the champion panel, food and water bars, the eye and mouth icons, scroll text, the
      status boxes of all champions.
- [ ] Sounds play at the right side and volume; the music of the intro plays and stops.
- [ ] Save, quit to the start screen, load the save again; the game goes on at the same place.
- [ ] Personal play: `make play`, then `playthroughs/local/dm --data game --saves playthroughs/local`. Move your
      saves from `playthroughs/lego/` to `playthroughs/local/` first (backup: `QUICK-before-chaos.DAT`).

## Chaos Strikes Back

- [ ] `build/dm --adventure csb` starts CSB; the dialogs and texts draw correctly; one level plays.

## Developer modes and LLM mode

- [ ] `build/dm --show <n>` shows the graphic n; `--view` shows the view and the keys move it.
- [ ] `uv run dm-play` drives a few turns through the protocol (look, move, a batch); the replies are the same as
      before.
- [ ] Close the client in the middle of a batch: the game goes on and a new client can connect. (Changed: the
      batch now stops one action earlier than before.)

## Windows

- [ ] `make windows`; `build/dm.exe` starts on Windows, plays, saves and loads.

## Decisions for the owner

- [ ] Rule 4.8: the stored attack sound ordinal of the KAOS creature table counts as "a game record stores it"
      (two allowlist lines). Confirm or reject.
- [ ] Read the "Rulings of the run" list in the Decisions section of the PRD; reject any ruling you disagree with.
