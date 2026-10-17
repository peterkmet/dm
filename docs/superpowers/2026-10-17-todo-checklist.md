# Todo pass: manual test checklist

Branch `feature/2026-10-17-todo`. Build with `make`, then run `build/dm` (add `--mode advanced` where a line says
so). The automated suite (`make check`) is green; these items need eyes, ears or a keyboard.

## Name entry of a reincarnated champion

- [ ] Vanilla, DM: recruit a candidate with REINCARNATE. The keyboard panel opens, the name line is empty, the
      underscores show 7 and 19 places, the cursor is gold.
- [ ] Type a name with the keyboard; the 7th letter jumps to the title. Return jumps to the title earlier.
- [ ] Backspace deletes; at the start of the title it goes back into the name.
- [ ] `,` `.` `;` `:` work from the keyboard (`:` with Shift on your layout). A right click types a space.
- [ ] A space as the first character of a field does nothing. OK with an empty name does nothing.
- [ ] OK closes the panel; the message area shows "<NAME> REINCARNATED."; the status box shows the new name.
- [ ] The name of another champion of the party is refused: the panel stays open, the title is empty.
- [ ] During the entry the game time stands still (no creature moves, torches do not burn down).
- [ ] Advanced mode: W A S D Q E M type letters and do not move the party; Tab and Escape do not close the
      panel; F5 and F9 do nothing.
- [ ] CSB: the same panel works.
- [ ] Turnbased (`dm-play recruit` with reincarnate): the champion keeps the old name.

## Credits on the entrance

- [ ] The DM entrance fades in from black at the start.
- [ ] A click on the credits zone fades to black, the credits fade in, the pointer is hidden.
- [ ] A click or a key returns to the entrance with a fade; after 36 seconds it returns by itself.
- [ ] ENTER and RESUME still work after the credits.

## Damage splat

- [ ] Walk into a wall or let a creature hit a champion: the status box shows the red splat with the number,
      and it goes away after a moment.
- [ ] With the inventory of that champion open, the splat shows on the portrait.

## Thieves Eye

- [ ] Cast Thieves Eye: the front wall shows a round hole with the corridor behind it; a door in front shows
      the eye; standing in a door frame, the frame shows the hole.

## Sound on another map

- [ ] A door or a creature on another level is not heard on the party level (no stray sounds after a level
      change).

## Advanced mode

- [ ] The damage of a hit stays in the action area for two ticks; a click on the action area during that time
      shows the icons and the champion acts at once.
- [ ] The action icons keep their colors on level 11 and 12 and in dim light (compare with level 1 in full
      light).

## Developer view

- [ ] `build/dm --view`: W/Up forward, S/Down back, A/Left and D/Right sideways, Q/Delete/KP7 and
      E/Insert/KP9 turn; the view walks through walls and stops at the map border.

## Pointer and filter

- [ ] `build/dm --filter scale2x` and `--filter scale3x`: the pointer edges are smoothed like the picture; the
      hot spot is where the click lands; the pointer at the screen edge is clipped, not wrapped.
- [ ] Without `--filter` the pointer looks as before.

## Discard when records run out

- [ ] No manual test: the differential test `tests/differential/test_discard.py` compares it with the original.
