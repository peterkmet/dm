# Ideas for later

User wishes for future versions. Not planned yet.

- Weapons go straight into a hand of a champion, not through the inventory.
- The automap (Tab, M) scrolls with the mouse, so the whole level can be seen.
- Fix the automap: walls and corridors look swapped.
- Tab in an open inventory closes the inventory.
- A click on a fountain with an empty hand lets the whole party drink, not only the leader.
- An application icon (macOS app bundle), so the Dock does not show the generic (exec) icon.
- Fix the automap: it marks squares that the party cannot see. automap_mark_seen marks the side squares at the depth where a wall blocks the view (example: level 2, party at (11,10) facing west marks (10,9) behind two walls). Mark a side square at depth d only when the square ahead at depth d or the side square at depth d-1 is open.
- Fix eating: process_click_on_mouth (inventory.c) adds the food value but sets only CHAMPION_ATTR_STATISTICS, so the food and water bars do not redraw. The original (0x10f8da) also sets CHAMPION_ATTR_PANEL when the panel shows food and water, calls champion_draw_state at once, and blinks the mouth icon (0xcd/0xce) 4 times when the object is used up. Test: open inventory, food in hand, click the mouth, the food bar pixels change.
- Check: in the DOS version, a selected rune stayed selected during sleep, so a fighter could train a little mana. Check what the Amiga original (KAOS) does with the selected runes during sleep, and that the port does the same.
