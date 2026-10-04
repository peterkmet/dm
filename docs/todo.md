# Ideas for later

User wishes for future versions. Not planned yet.

- Weapons go straight into a hand of a champion, not through the inventory.
- The automap (Tab, M) scrolls with the mouse, so the whole level can be seen.
- Fix the automap: walls and corridors look swapped.
- A click on a fountain with an empty hand lets the whole party drink, not only the leader.
- An application icon (macOS app bundle), so the Dock does not show the generic (exec) icon.
- Check: in the DOS version, a selected rune stayed selected during sleep, so a fighter could train a little mana. Check what the Amiga original (KAOS) does with the selected runes during sleep, and that the port does the same.
- DM intro: play game/TITL.DAT with the ANIM player (port/src/anim.c) before the entrance; check that TITL.DAT uses only the chunk tags the player knows, and check what APPB.FTL and SWSH.FTL show in the original intro.
- Entrance door animation after ENTER, as in KAOS.FTL; check in the original code whether a sound plays when the doors open.
- Optional: CSB intro (game-csb/disk-b/ANIM.DAT); it needs the music chunks (MI, MD, MF, SF) and MIDI/NAKED.AMG.
- Port champion_draw_changed_object_icons (0x112dd2): a waterskin in the hand keeps the full icon after a drink until the hand changes.
