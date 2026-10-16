# Movement, sensors, and timeline rules

This page describes the movement rules of the C port: the current map, square thing lists, party movement, sensors, doors, pits, teleporters, stairs, and the event dispatch of `event_process_expired`. The code is in `src/engine/map.c`, `src/engine/move.c`, `src/engine/sensor.c`, and `src/engine/event.c`. The rules follow KAOS range 8 (MOVESENS.C, TIMELINE.C, COMMAND.C) and the dungeon access functions of range 4. See [range-4-6.md](../reference/symbols/range-4-6.md) and [range-7-9.md](../reference/symbols/range-7-9.md) for the record layouts.

## Start a game

Load KAOS and the dungeon. Then start a new game and put the party on the start square.

```c
tables_load("game/KAOS.FTL");
dungeon_load("game/Dungeon.DAT");
game_start_new();
move_party_enter_dungeon();
```

`move_party_enter_dungeon` resets the current map (`map_reset`), selects the party map, and calls `move_get_move_result` with a party move that enters the square (`.enters = true`). The party enters the start square and its floor sensors.

## Game loop order

The game loop calls these functions in this order on each tick:

1. `move_process_new_party_map()`. When `g_new_party_map_index` is not −1, the party map changes, the party enters its square on the new map, and `g_new_party_map_index` becomes −1.
2. `event_process_expired()`. Repeat step 1 while `g_new_party_map_index` is not −1.
3. Increment `g_game_time`.
4. The command step calls `move_party(command)` or `move_turn_party(command)`.

## Current map

All square and thing functions work on the current map (`g_current_map_index`). `map_set_current_map(map)` selects a map. `map_set_current_map_and_party_map(map)` also sets `g_party_map_index`. The ornament lists of the original `set_current_map_and_party_map` are view data. `view_load_map` handles them.

Call `map_reset()` after `dungeon_load`. The current map pointers then point to the new buffers at the next `map_set_current_map`.

| Function | Original | Work |
|---|---|---|
| `map_get_square(x, y)` | `0x10a8ac` | Square byte. Outside the map: a wall byte. The wall gets bit 1, 3, 2, or 0 when the adjacent square above, below, left, or right is a corridor or a pit. |
| `map_get_square_type(x, y)` | — | `map_get_square(x, y) >> 5`. |
| `map_is_square_open(x, y)` | `0x10bce6` | 1 for a corridor, pit, door, or teleporter. |
| `map_get_level_change_location(place, delta)` | `0x10aa0c` | Finds the map on level `level + delta` that covers the same dungeon position. Returns the `Position` on that map, or map −1 and the unchanged square when no map matches. |
| `map_get_stairs_exit_direction(x, y)` | `0x10aad2` | Bit 3 of the stairs square selects the axis (set: north–south). The exit side is the side that is not a wall or stairs. |
| `map_get_square_first_thing(x, y)` | `0x10ac4e` | First thing, or `0xfffe`. |
| `map_get_square_first_object(x, y)` | `0x10ac7c` | First thing with type 4 or higher. |
| `rec_thing_next(thing)` | `0x10abb4` | Next-thing word of the record. |
| `map_link_thing_to_list(thing, list, x, y)` | `0x10acba` | See [Thing lists](#thing-lists). |
| `map_unlink_thing_from_list(thing, list, x, y)` | `0x10adf4` | See [Thing lists](#thing-lists). |
| `map_find_thing((Location){x, y}, THING_GROUP)` | `0x10bb5c` | First group thing, or `0xfffe`. |
| `object_get_type(thing)` | `0x1079a4` | Object type from the object info table (`0x101068`). −1 for `0xffff` and for a thing that is not an object. |
| `map_get_creature_attributes(group)` | `0x10a6e4` | Attribute word of the creature type (`0x10192e + type * 0x1a + 2`). |

### Thing lists

`map_link_thing_to_list(thing, list, x, y)`:

1. Set the next word of `thing` to `0xfffe`.
2. When `x < 0`, append `thing` to the list that starts at `list`.
3. When the square has no list, set bit 4 of the square. Add 1 to the first-thing index of every later column of the dungeon. Insert `thing` into `g_square_first_things`. The last entry must be `0xffff`, else the function stops with system error 72 (`0x48`).
4. Else append `thing` to the end of the square list.

`map_unlink_thing_from_list` is the inverse. It compares things without the cell bits. When the list becomes empty, it clears bit 4, removes the entry, writes `0xffff` to the last entry, and subtracts 1 from the later column indices.

## Party movement

### Commands

| Command | Value | Function |
|---|---|---|
| Turn left | 1 | `move_turn_party` |
| Turn right | 2 | `move_turn_party` |
| Move forward | 3 | `move_party` |
| Move right | 4 | `move_party` |
| Move backward | 5 | `move_party` |
| Move left | 6 | `move_party` |

`move_party(command)`:

1. Each living champion loses `load * 3 / maximum_load + 1` stamina (`champion_decrement_stamina`).
2. On stairs, a backward move takes the stairs (`move_take_stairs`).
3. The target square is the party square plus the step counts of the arrow (`0x103c56`: forward 1, 0, −1, 0; `0x103c5e`: right 0, 1, 0, −1), relative to the party direction.
4. A stairs target: the party leaves its square, moves onto the stairs square, and takes the stairs.
5. These targets block the move: a wall, a door with state 2, 3, or 4, and a fake wall with bit 2 (open) and bit 0 (imaginary) clear. With champions, a group on the target also blocks the move.
6. Else the party moves with `move_get_move_result`. On stairs, the party only enters the new square. `g_disabled_movement_ticks` becomes the maximum of 1 and `champion_get_movement_ticks` of each living champion. `g_projectile_disabled_movement_ticks` becomes 0.

`move_turn_party(command)` takes the stairs when the party is on stairs. Else the party leaves the floor sensors of its square, turns (`move_set_party_direction`), and enters the floor sensors again.

`move_take_stairs(up)` removes the party from its square, sets `g_new_party_map_index` and the party square with `map_get_level_change_location` (−1 for up, +1 for down), and turns the party to the stairs exit direction of the new map.

`move_set_party_direction(direction)` adds the direction change to the cell and the direction of each champion. The loop runs one time also without champions, as in the original.

### `move_get_move_result`

`move_get_move_result(move, destination)` moves the party (`thing` `0xffff`), a group, an object, or a projectile. The `Move` request holds the thing, `from`, and `to`. The original codes these as coordinates: `enters` is `src_x < 0` (the thing only arrives), `leaves` is `dst_x < 0` (the thing is only removed), and `keeps_cell` is `src_x == -2` (no teleporter rotation of an object). `uses_rope` is the original `g_use_rope_to_climb_down_pit`. The function returns `MOVE_RESULT_DONE`, `MOVE_RESULT_STOPPED` (the mover died, was deleted, waits off the map, or became active or inactive), or `MOVE_RESULT_TURNED` (a teleporter turned an attacking group).

One square chain (`follow_square_chain`) serves every mover kind (`MOVER_PARTY`, `MOVER_GROUP`, `MOVER_OBJECT`, `MOVER_PROJECTILE`). It repeats these rules on the destination square, at most 99 times (`SQUARE_CHAIN_MAXIMUM`). An open teleporter has bit 3 set; its scope is bits 5–6 of byte 2 of the teleporter record, the target is byte 3 bits 0–4 (X), word 2 bits 5–9 (Y), and byte 4 (map); bit 7 of byte 2 makes it audible, bits 2–3 hold the rotation, and bit 4 makes the rotation absolute. An open pit has bit 3 set and bit 0 clear.

| Square | Mover | Rule |
|---|---|---|
| Teleporter | party | Needs scope bit 2. Plays sound 14 (`SOUND_TELEPORT`) now when audible. The party turns. The port counts the teleport. |
| Teleporter | group | Needs scope bit 1 (creatures). Plays sound 14 (`SOUND_BUZZ`) after the tick when audible. `group_turn_by_teleporter` turns the creature directions and cells. |
| Teleporter | object | Any scope except 1 (creatures only). The object rotates its cell, unless `keeps_cell`. |
| Teleporter | projectile | Any scope except 1 (creatures only). The projectile rotates its flight direction and its cell. |
| Teleporter | all | A closed teleporter, another scope, or a teleporter that points to its own square ends the loop. |
| Pit | all | An open pit takes a thing that does not levitate one level down (`map_get_level_change_location(place, 1)`). |
| Pit | party | `champion_fall_damage(uses_rope)`: `champion_damage_all(20, 0x30, 2)` and sound 7, or with the rope a stamina loss of `load * 25 / maximum_load + 1` for each living champion. Only the first pit of the chain uses the rope. |
| Pit | group | `group_is_killed_by_fall`: each creature takes `FALL_DAMAGE` (20) on the source map. Creatures that died drop their fixed possessions on the destination. When all creatures die, the loop ends. |
| Stairs | object, group | Down stairs (bit 2 clear) go one level down; up stairs do not change the level. The thing steps out in the exit direction and takes the cell on the side it came from, next to the stairs. |
| Stairs | party, projectile | The loop ends. |
| Other | all | The loop ends. |

After the loop:

- The `MoveDestination` (when the caller gives one) holds the map, the square, the cell, and the projectile direction (the original `g_move_result_*` globals).
- A party move to a new square adds a scent. The scent list keeps 24 entries. `party_add_scent_strength` updates the strengths. The original compares scent `i` with strength `count − 1 − i`. The port keeps this.
- A group that died in a fall, or that reached a map that does not allow its creature type, drops its possessions and is deleted (`group_remove_after_move`).
- A move to the same square does nothing when the party direction or the cell of an object or a group did not change.
- The thing leaves the source square with `sensor_process_thing_addition_or_removal(..., 0)`. A levitating object (projectile, explosion) only unlinks.
- The party on a new map sets `g_new_party_map_index`. The party on the same map enters the floor sensors. An object enters the square with its sensors. A projectile only links.
- A group enters the square with `group_enter_square` (see [creatures.md](creatures.md)).

### Champion functions

Movement calls these functions of `src/engine/champion.c` and `src/engine/inventory.c` (see `docs/rules/champions.md`): `champion_decrement_stamina`, `champion_get_maximum_load`, `champion_get_movement_ticks`, `champion_damage_all`, `champion_fall_damage`, `party_delete_scent`, `champion_add_skill_experience`, `champion_add_candidate_to_party`, `inventory_put_object_in_leader_hand`, `inventory_get_object_removed_from_leader_hand`, and `object_get_type`. `object_get_type` needs `tables_load()`.

## Sensors

### Floor and wall sensors

`sensor_process_thing_addition_or_removal(x, y, thing, party_on_square, add)` runs when a thing enters (`add` 1) or leaves (`add` 0) a square. `thing` `0xffff` is the party.

1. The function scans the square: a group present, objects present, objects of the same type, objects of another type. On a wall square it only scans the cell of the thing.
2. On a floor square, the party entering a square prints the visible texts on the square (`dungeon_decode_text` mode 1, color 15). Not when the party only turns.
3. The function runs each sensor with a type other than 0. A floor sensor triggers on these conditions:

| Type | Condition | Value |
|---|---|---|
| 1 | Party not on the square, no objects, no group. | `add` |
| 2 | The party or a group (type ≤ 4), party not on the square, no group. | `add` |
| 3 | The party, with champions. Data 0: party not on the square. | Data 0: `add`. Else: `add` and party direction + 1 equals the data. |
| 4 | An object of the data type, no other object of this type. | `add` |
| 5 | The party on a stairs square. | `add` |
| 7 | A group (type ≤ 4, not the party), no other group. | `add` |
| 8 | The party. | The party holds an object of the data type (all slots of the living champions, the leader hand, and chest contents). |
| 9 | The party enters, party not on the square. | Data < 37. |

A wall sensor in the cell of the thing triggers on these conditions:

| Type | Condition |
|---|---|
| 1 | No objects in the cell. |
| 2 | The thing has the data type, and no other object of this type is in the cell. |
| 3 | The thing does not have the data type, and no object of another type is in the cell. |

The trigger rule: XOR the value with the revert bit (byte 5 bit 5). Effect 3 (hold) becomes 0 (set) for a true value and 1 (clear) for a false value. Other effects trigger only on a true value. The audible bit (byte 5 bit 6) plays sound 1. Then `sensor_trigger_effect` runs.

### Wall click

`sensor_is_triggered_by_click_on_wall(x, y, cell)` handles a click on the front wall cell with the leader hand. The command step calls it. It returns 1 when a sensor triggered. Only sensors in the clicked cell count. Without a leader, only type 127 counts. `g_leader_empty_handed` tells if the leader hand is empty.

| Type | Rule |
|---|---|
| 1 | Always triggers. Effect 3 is ignored. |
| 2 | Triggers when the revert bit equals "empty handed". |
| 3, 4 | Triggers when the leader hand holds the data type (inverted by the revert bit). Type 4 consumes the object. |
| 11 | As type 3, last sensor of the cell only. Rotates the cell sensors. Consumes the object. |
| 12 | Empty hand, last sensor of the cell only. Rotates the cell sensors. |
| 13 | Exchanges the object of the data type between the hand and the cell. Rotates the cell sensors. |
| 16 | Last sensor of the cell only. Exchanges the hand object of the data type with the first object of the square. |
| 17 | As type 3, last sensor of the cell only. The sensor record becomes free. Consumes the object. |
| 127 | Champion portrait: `champion_add_candidate_to_party(data)`. |

### Sensor effect

`sensor_trigger_effect(sensor, effect, x, y, cell)`:

1. A once-only sensor (byte 5 bit 2) sets its type to 0.
2. A local sensor (byte 4 bit 3) calls `sensor_trigger_local_effect(word 6 >> 4, ...)`. Local effect 10 gives 300 experience in skill 8: to the leader for a wall sensor, else shared by the living champions. Local effects 1 and 2 rotate the sensors of the cell (`sensor_process_rotation_effect`): the first sensor moves behind the last sensor of the cell.
3. A remote sensor adds a square event at the target (word 6 bits 6–10: X, bits 11–15: Y) after the delay (word 4 bits 7–10). The target square type selects the event type (`0x100d4a`): wall 6, corridor 5, pit 9, stairs 0, door 10, teleporter 8, fake wall 7. Only a wall target keeps the target cell (word 6 bits 4–5).

`sensor_add_event(type, at, cell, effect, time)` adds the event on the current map. The priority is 1 for effect 0, 3 for effect 1, 2 for effect 2, and 0 for other effects. Example: a door button click adds `sensor_add_event(10, at, 0, 2, g_game_time + 1)`.

## Doors

The door state is bits 0–2 of the door square: 0 open, 1–3 partly closed, 4 closed, 5 destroyed.

- Event 10 (door): no effect on a destroyed door. Toggle (2) becomes 1 for an open door, else 0. Effect 0 on an open door and effect 1 on a closed door do nothing. Else the event becomes event 1 at the same time.
- Event 1 (door animation): one step per tick. Effect 0 decrements the state, effect 1 increments it. Each step plays sound 2 and adds the event again at time + 1, until the state is 0 or 4.
- A closing door on the party square (state not 0) opens fully when the party has champions, damages the party (`champion_damage_all(5, 4, 2)`, sound 15), and tries again at time + 2.
- A closing door with a group that is not non-material (creature attribute bit 6) goes back one step when the creature height is at most the state, plays sound 4, and tries again at time + 2. The height is 1, or creature attribute bits 7–8 for a door with bit 5 of byte 3.
- Event 2 (door destruction): the state becomes 5.
- `event_is_door_destroyed_by_attack(at, attack, magic, ticks)` (`0x11d5e0`): force (`magic` false) needs door word bit 8. Magic needs door word bit 7. The attack must reach the door defense (low byte of `g_current_map_door_info[door type]`). The door must be closed (state 4). `ticks` 0 destroys the door at once. Else event 2 destroys it after `ticks`. Returns 1 when the door breaks.

## Pits, teleporters, and fake walls

- Event 8 (teleporter) and event 9 (pit): toggle becomes 1 for an open square (bit 3), else 0. Effect 0 sets bit 3 and calls `event_move_teleporter_or_pit_square_things`. Other effects clear bit 3.
- `event_move_teleporter_or_pit_square_things(x, y)` moves the party, the group, and each object of the square again with a move from the square to the same square. It updates the events of moved projectiles and explosions from the `MoveDestination`.
- Event 7 (fake wall): bit 2 is open. Toggle becomes 1 for an open wall, else 0. Effect 1 closes the wall. It waits (time + 1) while the party or a material group is on the square. Other effects open it.

## Square events on corridors and walls

- Event 5 (corridor): each text thing changes visibility (byte 3 bit 0). A text that becomes visible on the party square prints as a message. A group generator sensor (type 6) plays sound 14 when audible and disables itself. A generator that is not once-only and has a delay (byte 6) comes back with event 65 after the delay. A delay above 127 means `(delay − 126) * 64` ticks.
- Event 65: the first disabled sensor of the square becomes type 6 again.
- Event 6 (wall): texts in the event cell change visibility. A counter (sensor type 6) with data not 0 counts up (effect 0, up to 511) or down. It triggers at 0, or on each event with effect 3. An AND/OR gate (type 5) sets, clears, or toggles bit `cell` of its data, and compares bits 4–7 with bits 0–3. After the sensors, the cell sensors rotate when a local effect asked for it.

## Timeline dispatch

`event_process_expired()` runs every expired event. For each event it selects the event map, calls the handler, and selects the party map again. Event 70 runs on the party map.

Implemented event types:

| Type | Action |
|---|---|
| 1 | `event_process_door_animation` |
| 2 | `event_process_door_destruction` |
| 5 | `event_process_corridor` |
| 6 | `event_process_wall` |
| 7 | `event_process_fake_wall` |
| 8 | `event_process_teleporter` |
| 9 | `event_process_pit` |
| 10 | `event_process_door` |
| 20 | `sound_request_play(c, at, SOUND_MODE_AFTER_TICK, 0)` |
| 24 | Unlinks and frees the fluxcage thing in `c`, unless `g_game_won` is set. |
| 53 | `timeline_add_watchdog_event` |
| 65 | `event_enable_group_generator` |
| 70 | `event_process_light`: one step of the light table (`0x100cb4`), the rest after 4 ticks. |
| 71 | Decrements the invisibility count. |
| 72 | Subtracts `b` from the shield defense of champion `priority`. |
| 73 | Decrements the thieves eye count. |
| 74 | Subtracts `b` from the party shield defense. |
| 77 | Subtracts `b` from the spell shield defense. |
| 78 | Subtracts `b` from the fire shield defense. |
| 79 | Decrements the footprints count. |

Event types without effect in the port:

| Type | Original handler | Step |
|---|---|---|
| 11 | `event_enable_champion_action` | Champions |
| 12 | `event_hide_damage_received` | Champions |
| 13 | `event_process_vi_altar_rebirth` | Champions |
| 25 | `explosion_process_step_event` | Projectiles |
| 29–41 | `group_process_reaction_event` | Creatures |
| 48, 49 | `projectile_process_move_event` | Projectiles |
| 60, 61 | `group_process_move_event` | Creatures |
| 75 | `champion_poison` | Champions |
| 0, 3, 4, 14–19, 21–23, 26–28, 42–47, 50–52, 54–59, 62–64, 66–69, 76, 80 and higher | None in the original | — |

The redraw calls of events 70–79 (`set_dungeon_view_palette`, `ui_draw_champion_state`) belong to the drawing step.

## Gaps

These rules need later steps. The port has no code for them yet:

- Groups: `move_get_move_result` returns 0 for a group and does not move it. The party that arrives on a group square does not delete the group. A blocked move into a group does not send event −1 to the group. The closing door does not damage a group. The group generator does not create a group.
- Projectiles: wall launchers (sensor types 7–10, 14, 15) do not launch. `projectile_is_killed_by_impact` does not run.
- Champions: a blocked move does not damage the two front champions and plays no sound. This needs `group_get_target_champion_index` and `group_set_ordered_cells_to_attack`.
- Objects: wall sensor type 12 does not create an object (`projectile_get_object_for_launcher_or_generator`).
- Endgame: wall sensor type 18 does nothing.
- Drawing: the pit fall animation and the redraw calls are not part of the rules.

## Differences from the original

- Event 65 stops at the end of the thing list. The original stops only at `0xffff`.
- `sensor_process_rotation_effect` stops when the cell has no sensor. The original reads past the list.
- Wall sensor type 16 does not exchange when the square has no object. The original puts `0xfffe` into the leader hand.

## Tests

`tests/differential/test_move.py` needs `game/KAOS.FTL` and `game/Dungeon.DAT` and skips without them.

- `map_get_square`, `map_is_square_open`, `map_get_square_first_thing`, `map_get_stairs_exit_direction`, and `map_get_level_change_location` give the same results as the original functions in `dmtools.emu` on maps 0–2, including the squares around the map.
- Start position from the dungeon header. A move into a wall does not move the party. A corridor move follows the step tables of KAOS. Turns change the direction and the champion cells. A turn on stairs takes the stairs.
- Stairs on map 0 at (3, 15) lead to map 1 at (3, 1), direction 0.
- The floor sensor at map 0 (6, 9) opens the door at (5, 9): the door state is 4, 3, 2, 1, 0 on the ticks after the move.
- A door event (type 10, toggle) opens and closes a door.
- The teleporter at map 1 (30, 3) moves the party to (30, 1) with direction 0.
- The pit at map 1 (7, 8) drops the party to map 2 and adds pending damage to the champion.
- `map_link_thing_to_list` and `map_unlink_thing_from_list` restore the square first things and the column indices.
