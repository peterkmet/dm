# Ideas for later

User wishes for future versions. Not planned yet.

## Turn result reports the health loss of each champion (feature)

- A batch step that stops with `bumped` or `moved` does not report other damage of that step, and a single `act` never reports damage. The player then sees a health loss without a cause (run 4, session 1: Iaido lost 7).
- Add `health_lost` to the response of `act`, `act_many`, and `wait`: the health loss of each champion since the start of the request.
- Test: pending damage 5 and a wall bump in `act_many` give `stopped` `bumped` and `health_lost` with 6 for the champion.

## Automap: show where the corridors go on (advanced mode)

- Black means two things on the automap (`port/src/automap.c:171-190`): a square that the party has not seen, and a square outside the map. An unseen square next to a corridor looks the same as the map border, and a wall that the party has not seen looks the same as an open corridor.
- Draw the squares outside the map as walls (dark gray): in DM they are always walls.
- Draw an unseen square next to a seen floor square in another color (for example dark blue): the corridor can go on there. Black then means only "far and not explored".
- Only the automap of the advanced mode changes. `get_map` of the turn-based mode stays.

## Automap: move the map by dragging (advanced mode)

- Today a click on the automap scrolls it (`automap_scroll`).
- Change: press the mouse button on the map, move the mouse, and release: the map follows the mouse. A click without a move does not scroll.
