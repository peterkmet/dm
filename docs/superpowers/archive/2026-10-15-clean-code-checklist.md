# Clean-code pass: manual test checklist

Branch `feature/2026-10-15-clean-code`. PRD: `2026-10-15-clean-code.md`. The automated suite is green (655 passed); these items have no automated test.

Start: `make -C port && build/dm` (DM), and the same with the CSB data for the CSB items.

## Drawing (Tasks 9-12, 20, 36)
- [ ] The mouse pointer has the correct colors in the dungeon and in the inventory.
- [ ] Action icons are recolored when a champion cannot act; the invisibility icon is recolored.
- [ ] A dialog draws its buttons; a pressed button draws as pressed.
- [ ] The champion parade (Hall of Champions): the skill lines show.
- [ ] The food and water bars have their shadow; the status box shows the portrait.
- [ ] The credits screen draws.
- [ ] CSB: the restart button draws.
- [ ] Dungeon view: walk past walls, doors (open, closed, side view), pits (also invisible and in the ceiling), stairs up and down (front and side), teleporter fields and alcoves at every distance and side. Nothing is missing or in the wrong place.

## Rules moved from ui to engine (Tasks 32-35)
- [ ] Drink from a fountain: one champion (click in the view) and the whole party.
- [ ] Fill a flask or waterskin at a fountain.
- [ ] Vi altar: put the bones of a dead champion on it; the champion comes back.
- [ ] Press a door button; the door opens or closes.
- [ ] Runes: add symbols, delete one, a champion with too little mana cannot add a symbol; change the caster.
- [ ] Swap two champions by dragging the champion icon; move a champion to an empty cell.
- [ ] A champion is hit from behind; the champions turn to the attacker.
- [ ] Sleep and wake up.
- [ ] Open and close a champion inventory.
- [ ] The action list: the weapon actions show; an empty charge item hides its charged actions.
- [ ] Torches burn down over time; the view gets darker; the light spells still work.
- [ ] Quick save and quick load; the new-game screen after a save shows the resume option.
- [ ] Cheats (light, immortal, food) toggle.
- [ ] A champion dies and the whole party dies: the spell area is cleared, the game-over screen shows.
- [ ] DM ending: reach the endgame sensor; the ending shows; the saved game is cleared after the ending.
- [ ] CSB ending: the fuse sequence plays.
- [ ] Mode switch (real time / turn based) on macOS.

## Windows build (Tasks 13-17, user decision: checked at the end)
- [ ] `make -C port windows` builds with 0 warnings; `dm.exe` starts.
- [ ] The mode menu has 2 entries; `dm.exe --mode turnbased` prints the message that the mode is macOS only.
- [ ] Extract the ADF on Windows; the game folders are created and the game starts from them.
