# Ideas for later

User wishes for future versions. Not planned yet.

## Turn-based view lists side squares that the screen does not draw (bug)

- The view (`state_json_view`, `state_json_visible_things`, and `is_seen` in `port/src/state_json.c`) lists all 3 squares of each depth. The screen and the automap (`side_is_visible` in `port/src/automap.c`) show a side square only when the center square of its depth or the side square one row nearer is open.
- Effect: the JSON shows squares, creatures, and objects behind walls, and a hidden creature can stop a batch with `creature`. On map 1 there are 161 such cases. Example: map 1 (0,8) facing south lists (1,9) as floor.
- Fix: export the rule as `automap_side_is_visible` and skip the hidden side squares in the three loops. Tests that read `squares[4]` as the center square at depth 1 must find the square by place.
- Test: at map 1 (0,8) facing south, (1,9) is not in the view and not in the map.

## Turn result reports the health loss of each champion (feature)

- A batch step that stops with `bumped` or `moved` does not report other damage of that step, and a single `act` never reports damage. The player then sees a health loss without a cause (run 4, session 1: Iaido lost 7).
- Add `health_lost` to the response of `act`, `act_many`, and `wait`: the health loss of each champion since the start of the request.
- Test: pending damage 5 and a wall bump in `act_many` give `stopped` `bumped` and `health_lost` with 6 for the champion.
