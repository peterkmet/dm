# Options pass: manual test checklist

Branch `feature/2026-10-17-options`. Build with `make`, then run `build/dm --mode advanced` (vanilla where a line
says so). The automated suite (`make check`) is green; these items need eyes, ears or a keyboard.

## Options menu (Escape)

- [ ] Escape in the dungeon view opens OPTIONS; the game time stands still; Escape or CONTINUE closes it.
- [ ] SOUND and MUSIC VOLUME go 0 to 7 with Left/Right and with a click on `-` and `+`; 0 is silent.
- [ ] MUSIC cycles ORIGINAL, SUPER NES, FM-TOWNS (CSB skips SUPER NES); the track changes.
- [ ] After a restart of the program the values are back (`SETTINGS.TXT` in the saves folder).
- [ ] Escape with an open inventory closes the inventory; on the automap it closes the map.
- [ ] Vanilla: Escape freezes the game as before.

## Cheat screen (C)

- [ ] C opens CHEATS; C or Escape closes it; Escape there does not open the options menu.
- [ ] FULL LIGHT, IMMORTAL, FOOD AND WATER work as before on the automap.
- [ ] ONE-HIT KILL: a hit or a spell kills a creature; a closing door does not.
- [ ] WALK THROUGH WALLS: the party walks into and through walls, not past the map border or into a creature.
- [ ] Turn the cheat off inside a wall: the party can still step out to a free square.
- [ ] REVEAL MAP shows the whole level on the automap; off shows the real explored state again.
- [ ] QUICK SAVE F5 and QUICK LOAD F9 work from the screen; GAME SAVED shows.
- [ ] SPELLS: the pages list every spell with runes and numbers (for example `OH EW RA (X,3,2,5)`).
- [ ] Save, quit, RESUME: the cheats are still on.

## Save slots

- [ ] Ctrl+S, SAVE: a list of 8 slots; a click saves into that slot; the row then shows the level and the time.
- [ ] RESUME on the entrance: the load list with the 8 slots and QUICK; EMPTY rows do nothing.
- [ ] CSB: the same lists, files `CSBGAMEn.DAT`.
- [ ] After a death: RESTART THIS GAME loads the slot of the last save.
- [ ] Vanilla: SAVE and RESUME work as before with `DMGAME.DAT`.

## Chaos Strikes Back start

- [ ] Advanced CSB: choose champions in the prison, take the stairs down: the game starts in the dungeon at once.
- [ ] With no champion the stairs give the refusal message and lead to the prison level below.
- [ ] Vanilla CSB: the prison save works as before.

## Sounds

- [ ] Advanced: in a fight with several creatures more sounds play at once; no crackle or clipping.

## Release builds

- [x] macOS zip and Windows zip (Wine CX): a clean folder with the three `.adf` disks extracts `game/`,
      `game-csb/`, `game-csb/disk-b/` with files equal to the reference data, and DM and CSB start (checked
      2026-10-17).
