# KAOS ranges 7–9: symbol notes

This page uses the original KAOS names; the port names differ.

This page describes the functions of `KAOS.FTL` in partition ranges 7, 8, and 9 of the [module map](../kaos-map.md): `0x116a4e`–`0x127e34`. The symbols are in [kaos-symbols.csv](../kaos-symbols.csv). The CSV has one row for each of the 283 functions and rows for the globals that these functions own. For a global, the column `signature` holds the C type.

Signatures use 16-bit types for word arguments, because the original `int` is 16 bits. Struct names (`TimelineEvent`, `Group`, `Champion`, `Projectile`, `Teleporter`, `FieldAspect`) refer to the records that this page describes.

## Range 7: blitter, dungeon view, projectiles, group helpers

### Blitter

Three functions do all rectangle copies in KAOS. `blit_bitmap` is the entry point. `blit_raw` programs the Amiga blitter. `blit_box_filled_with_masked_bitmap` builds a patterned bitmap and then calls `blit_bitmap`.

#### Bitmap and box formats

- A bitmap pointer points to the pixel data. The word at offset −4 is the width. The word at offset −2 is the height.
- The pixel data has 4 bitplanes. The planes follow each other (not interleaved). One plane is `((width + 15) / 16) * 2 * height` bytes. Bit 15 of a word is the leftmost pixel. Plane 0 holds bit 0 of the color index.
- A box is 4 words: `x, y, width, height`.
- `box_to_bounds` converts a box to bounds: `x1, x2, y1, y2`. The bounds are inclusive.
- `blit_bitmap` and `blit_raw` use the widths and heights of the arguments, not the headers. The plane size of the source uses `src_height`. The plane size of the destination uses `dst_height`.

#### `blit_bitmap` and `blit_raw`

Signature: `blit_bitmap(src, dst, box, src_x, src_y, src_width, dst_width, transparent_color, src_height, dst_height)`. The old CSV signature had no `src_height` and `dst_height`.

Semantics for a chunky 8-bit port:

```c
for (y = box.y; y < box.y + box.height; y++)
  for (x = box.x; x < box.x + box.width; x++) {
    c = src[(src_y + y - box.y) * src_width + src_x + x - box.x];
    if (transparent_color == -1 || c != (transparent_color & 15)) dst[y * dst_width + x] = c;
  }
```

- `transparent_color == -1` (0xffff) makes an opaque copy. Any other value is a color key. Only bits 0–3 of the key are compared.
- `dst == g_screen_bitmap` selects the screen. `blit_bitmap` passes NULL for the screen, and `blit_raw` replaces NULL with `g_screen_bitmap`.
- `blit_raw` does nothing when `bounds` is NULL.
- There is no clipping in `blit_raw`. The callers clip through `get_zone`.

Hardware details that a port can ignore:

- `blit_bitmap` waits for the video beam when the destination is the screen. When the box is 64 rows or more, it splits the copy into 2 strips (64–127 rows) or 3 strips (128 rows or more). Before the first strip, it waits until the beam is below the strip. `g_blit_skip_beam_wait` cancels one wait.
- `blit_raw` uses a temporary mask buffer at `g_video + 0x138`. For a color key, two blits make the mask: blit 1 compares planes 0 and 1 with key bits 0 and 1, blit 2 adds planes 2 and 3. Two more blits set the mask bits to the left of `x1` and to the right of `x2`. The tables `g_blit_mask_from_pixel` (`0xffff >> i`) and `g_blit_mask_before_pixel` (`~(0xffff >> i)`) supply the edge masks. Then 4 blits (one per plane) use minterm `0xB8`: `D = mask ? C : A`, where A is the shifted source and C is the destination.
- An opaque copy with equal source and destination shift and no partial edge words uses 4 plain copies (minterm `0xF0`).

#### `blit_box_filled_with_masked_bitmap`

Signature: `(units, dst, mask, temp, box, last_unit_index, first_unit_index, dst_width, transparent_color, src_x, src_y, dst_height, unit_plane_words)`. The function was named `blit_setup`. `draw_field` and `draw_objects_creatures_projectiles_explosions` call it. ReDMCSB calls the same function `F0133_VIDEO_BlitBoxFilledWithMaskedBitmap`.

1. The box gives `W = ((x2 - x1) >> 4) + 1` words and `H = y2 - y1 + 1` rows. `temp` gets a bitmap of `W * 16` by `H` pixels.
2. The function fills `temp` word by word, in row order. Each 16-pixel word takes the next unit of `units`. A unit is one word per plane. The planes of `units` are `unit_plane_words` words apart. The unit index starts at `first_unit_index`. When it reaches `last_unit_index`, it restarts at 0.
3. When the low byte of `transparent_color` is `0xff`, or bit 7 is set, there is no mask. The function clears bit 7 and copies the unit unchanged.
4. Otherwise `mask` supplies one word per output word, linearly. Where a mask bit is 1, the pixel comes from the unit. Where a mask bit is 0, the pixel gets `transparent_color`.
5. It calls `blit_bitmap(temp, dst, box, src_x, src_y, W * 16, dst_width, transparent_color, H, dst_height)`. The pixels with the transparent color stay invisible.

Port: for each destination pixel in the box, take the pattern pixel, skip it when the mask bit is 0, and skip it when it equals the transparent color. The callers pick a random `first_unit_index`, so fields and explosions flicker.

### Dungeon view graphics loading

`load_current_map_graphics` (ReDMCSB `F0096`) runs after each map change.

- It reads the map descriptor (`g_current_map`): byte 0x0a low nibble = floor ornament count, byte 0x0b low nibble = wall ornament count − 1 (the loop uses `<=`), byte 0x0d low nibble = door ornament count, high nibble = creature type count, byte 0x0e = door sets 0 and 1, byte 0x0f low nibble = floor set, high nibble = wall set.
- `load_floor_set` loads graphics `0x4e + 2 * set` (floor) and `0x4f + 2 * set` (ceiling) into layout slots `g_wall_set_layout_ids[0]` and `[1]`. `g_current_floor_set` prevents reloads.
- `load_wall_set` loads 7 door frame graphics (`0x56 + 0x28 * set` …) and 15 wall graphics (`0x5d + 0x28 * set` …). `g_current_wall_set` prevents reloads.
- When the mirrored-wall memory option (`0x1001d5`) is on, `copy_wall_bitmap_and_flip_horizontal` makes 15 mirrored wall bitmaps in `g_wall_set_flipped_layout_ids`. Bitmaps with a width that is not a multiple of 16 get a realignment shift of `16 - (width & 15)`.
- It fills `g_stairs_graphics` (18 IDs, `0x6c + 0x28 * wall_set`), `g_door_graphics` (`0xf6 + 3 * door_set`, depth D3/D2/D1 × door set 0/1).
- It fills the ornament info tables (`g_current_map_*_ornament_info`: graphic index, coordinate set). Wall ornament graphics are `0x103 + 2 * ornament` (2 graphics each). Floor ornaments are `0x181 + 6 * ornament`. Door ornaments are `0x1b9 + ornament`.
- It computes the derived bitmap byte counts for the scaled ornaments with `get_scaled_bitmap_byte_count`. Scale 14/32 is D3. Scale 21/32 is D2. A scaled size is `(size * scale + scale / 2) >> 5`.
- `apply_creature_replacement_colors(color, set)` copies 6 RGB words into palette entry `color` of each of the 6 dungeon view palettes, and 2 bytes into `g_palette_changes_creature_d2/d3`. Colors 9 and 10 are replaceable. The replacement set comes from the creature aspect (byte 5: low nibble color 9, high nibble color 10).
- It builds a list of graphic indices and calls `graphics_dat_load_graphics`. `g_always_loaded_graphics` (71 words) starts the list. Sound sample graphics have flag `0x4000`.

### Viewport

- `g_viewport_bitmap` is an off-screen 224 × 136 bitmap. `g_viewport_buffer` is its pixel data.
- `clear_viewport` fills the 4 planes with 0 and clears `g_viewport_needs_clear`.
- `draw_viewport(palette_mode)` (ReDMCSB `F0097`) copies the viewport to screen zone 7. Mode 0 uses `g_palette_top_and_bottom`. Mode 1 uses `g_dungeon_view_palettes[g_party_light_level]` (6 light levels). Mode 2 keeps the palette. A palette change waits for the vertical blank.
- Zones: a zone number indexes the layout table of `Graphics.DAT` (range 6, `get_zone`). Zone bit 14 (`0x4000`) adds `g_zone_relative_x/y` to the position. Bit 15 is handled by `get_zone`. `get_zone` writes the result box into `g_drawn_box`.

### Dungeon view drawing

`draw_dungeon_view(direction, map_x, map_y)` (ReDMCSB `F0128`):

1. Clear the viewport when `g_viewport_needs_clear` is set.
2. Clear `g_dungeon_view_clickable_boxes` (6 boxes: cells 0–3, alcove, wall ornament or door button). Box x = −1 means empty.
3. Allocate `g_temp_bitmap`.
4. `g_use_flipped_wall_bitmaps = (map_x + map_y + direction) & 1`. When set: draw the ceiling (zone 700) normal and the floor (zone 701) mirrored, and swap in the mirrored wall IDs (save in `g_wall_set_layout_ids_saved`). When clear: draw the ceiling mirrored and the floor normal.
5. Draw objects only (no walls) of D4L, D4R, D4C with `draw_objects_creatures_projectiles_explosions`, view squares 17, 18, 16, cell order `0x0001`.
6. Draw the 16 squares, back to front. Each function gets `(direction, map_x, map_y)` of that square. `update_map_coordinates_after_relative_movement(direction, forward, right, &x, &y)` computes the square.

| Function | Square | Forward, right | View square | Wall ornament view walls |
|---|---|---|---|---|
| `draw_square_d3l2` | D3L2 | 3, −2 | 14 | 0 (right side) |
| `draw_square_d3r2` | D3R2 | 3, +2 | 15 | 1 (left side) |
| `draw_square_d3l` | D3L | 3, −1 | 12 | 2 (right side), 4 (front) |
| `draw_square_d3r` | D3R | 3, +1 | 13 | 3 (left side), 6 (front) |
| `draw_square_d3c` | D3C | 3, 0 | 11 | 5 (front) |
| `draw_square_d2l2` | D2L2 | 2, −2 | 9 | wall and field only |
| `draw_square_d2r2` | D2R2 | 2, +2 | 10 | wall and field only |
| `draw_square_d2l` | D2L | 2, −1 | 7 | 7, 9 |
| `draw_square_d2r` | D2R | 2, +1 | 8 | 8, 11 |
| `draw_square_d2c` | D2C | 2, 0 | 6 | 10 |
| `draw_square_d1l` | D1L | 1, −1 | 4 | 12 |
| `draw_square_d1r` | D1R | 1, +1 | 5 | 13 |
| `draw_square_d1c` | D1C | 1, 0 | 3 | 14 (front, clickable) |
| `draw_square_d0l` | D0L | 0, −1 | 1 | none |
| `draw_square_d0r` | D0R | 0, +1 | 2 | none |
| `draw_square_d0c` | D0C | 0, 0 | 0 | none |

7. Restore the wall IDs. Free the temp bitmap.
8. Call `draw_viewport(1)`, then `clear_viewport`. The next frame starts from a black viewport.

The square functions are ReDMCSB `F0116`–`F0127` plus D3L2, D3R2, D2L2, and D2R2. ReDMCSB has 12 square functions for DM 1.x. This version adds the 4 "L2/R2" squares, so there are 16.

#### Square function pattern

Each square function calls `set_square_aspect(aspect, direction, map_x, map_y)` (range 4). Aspect words: `[0]` element type, `[2]` first thing, `[3]` door state / pit invisible / stairs up, `[4]` right wall ornament or door index, `[5]` front wall ornament or floor ornament, `[6]` left wall ornament.

Element types: 0 wall, 1 corridor, 2 pit, 5 teleporter, 0x10 door side, 0x11 door front, 0x12 stairs side, 0x13 stairs front.

| Element | Drawing steps |
|---|---|
| Wall | Wall bitmap (transparent color 10; D3C, D2C, D1C opaque), then wall ornaments. D1C wall: thieves eye hole (graphic 0x29) when the party has the effect. |
| Corridor, teleporter | Floor ornament, then things (cell order `0x3421`), then the field for a teleporter. |
| Pit | Pit bitmap when the pit is visible (aspect `[3] == 0`), then as corridor. |
| Stairs | Stairs bitmaps from `g_stairs_graphics`, then as corridor. |
| Door front | Floor ornament, things behind the door (`0x0218`), door frames, door button, `draw_door`, things in front (`0x0349`). |
| D2/D1/D0 squares | `draw_ceiling_pit` for an open pit on the map above (square type 2, bit 3). |

Cell order words: each nibble is `cell + 1`, low nibble first. Bit 3 of the word marks a door pass: bit 0 then selects pass 1 (back cells) or pass 2 (front cells). The cells are relative to the party direction.

#### `draw_objects_creatures_projectiles_explosions`

ReDMCSB `F0115`. Arguments: `(first_thing, direction, map_x, map_y, view_square, cell_order)`.

- For each cell in `cell_order`, it walks the thing list. It draws objects (thing types 5–10) of that cell with `g_object_aspects`, scaled by depth.
- Then it draws the creature group of the square (thing type 4) with `g_creature_aspects`, the creature coordinate set (`g_view_square_creature_coord_index`), and the creature shift sets.
- Then projectiles (type 14, `g_projectile_aspects`).
- Then explosions (type 15). Fireball, lightning, and rebirth types use `get_explosion_bitmap`. Smoke uses `g_palette_changes_smoke`. An explosion on the party square (view square 0) fills the whole viewport with `blit_box_filled_with_masked_bitmap`. A fluxcage (explosion type 50) is drawn as a field, unless `g_do_not_draw_fluxcages` is set.
- It updates the clickable box of each cell (minimum box 14 × 11) and `g_pile_top_objects`.
- Depth scaling: D3 scale 14/32 with `g_palette_changes_creature_d3`, D2 scale 21/32 with `g_palette_changes_creature_d2`. D1 and D0 use native size, and mirror the bitmap for side views.

View square tables (24 bytes each, index = view square):

| Table | Content |
|---|---|
| `g_view_square_lane` | −3…+3 (left/right offset). |
| `g_view_square_depth` | 0–4. |
| `g_view_square_object_coord_index`, `g_view_square_alcove_object_coord_index` | Object coordinate set index, or −1. |
| `g_view_square_creature_coord_index` | Creature coordinate set index, or −1. |
| `g_view_square_explosion_coord_index` | Explosion coordinate set index, or −1. |
| `g_view_square_field_aspect_index` | Index into `g_field_aspects`. |

View squares: 0 D0C, 1 D0L, 2 D0R, 3 D1C, 4 D1L, 5 D1R, 6 D2C, 7 D2L, 8 D2R, 9 D2L2, 10 D2R2, 11 D3C, 12 D3L, 13 D3R, 14 D3L2, 15 D3R2, 16 D4C, 17 D4L, 18 D4R.

#### Field aspect (8 bytes, `g_field_aspects`)

| Offset | Content |
|---|---|
| 0 | Pattern graphic, relative to graphic 0x4c. |
| 1 | Last unit index (plus a random bit). |
| 2 | Transparent color. Bit 7 = no mask. |
| 3 | Mask graphic, relative to 0x46. 0xff = none. Bit 7 = mirror the mask. |
| 4, 5 | Width and height for the zone. |
| 6 | X offset. |
| 7 | Unit plane words (64). |

#### Helpers

- `draw_wall_set_bitmap`, `_opaque`, `_flipped`: draw a layout bitmap into the viewport at a zone. Transparent color 10, none, or 10 with a horizontal mirror.
- `get_flipped_bitmap` and `get_scaled_flipped_bitmap` return a native bitmap, or a derived bitmap that is scaled (scale/32 per axis, 32 = native), recolored with a palette-change table, and mirrored (flip bit 0 horizontal, bit 1 vertical). They use the derived bitmap cache. Index −1 uses `g_temp_bitmap`.
- `blit_in_zone` draws a bitmap at a zone. A mirrored bitmap moves right by `16 - (width & 15)`, because the mirror works on whole words.
- `draw_wall_ornament` returns nonzero for an alcove. For the D1C front wall it also sets `g_facing_alcove`, `g_facing_vi_altar`, `g_facing_fountain`, draws text inscriptions (graphic 0x102 font, text from the inscription thing), and draws the champion portrait (graphic 0x1a, `g_champion_portrait_ordinal`).
- `draw_door` copies the door graphic to `g_temp_bitmap`, adds the door ornament, mirrors at random for doors with attribute bit 2, and draws the door at a vertical offset that depends on the door state (1–4).
- `copy_layout_bitmap_pixels` (`0x1183de`) has no callers.

### Projectiles and explosions

ReDMCSB `PROJEXPL.C` order.

Projectile record (thing type 14, 8 bytes): `+0` next thing, `+2` slot (the flying object or an explosion thing), `+4` kinetic energy (byte), `+5` attack (byte), `+6` timeline event index.

Explosion record (thing type 15, 4 bytes): `+0` next thing, `+2` attack (byte), `+3` bits 0–6 type, bit 7 centered.

Explosion things are `0xff80 + type`: 0 fireball, 1 slime, 2 lightning bolt, 3 harm non-material, 4 open door, 6 poison bolt, 7 poison cloud, 40 smoke, 50 fluxcage, 100 rebirth step 1, 101 rebirth step 2.

- `projectile_create(thing, x, y, cell, direction, kinetic_energy, attack, step_energy)` gets a free type-14 thing, links it to the square, and adds an event at `game_time + 1`. The event type is 49 when `g_create_launcher_projectile` is set, otherwise 48 (ignore impacts on the first step). Event word at +8: bits 11–15 x, 6–10 y, 4–5 direction, 0–3 step energy. Event word at +6: the projectile thing. When no thing is free, the object drops to the square.
- `explosion_create(thing, attack, x_combo, y_combo, cell)`: a combo value ≥ 0x100 holds `(x + 1) << 8 | target`. It adds event 25 after 1 tick (5 for rebirth step 1). Fireball and lightning damage the party or the creatures on the square at once: `damage = random((attack >> 1) + 1) + (attack >> 1) + 2`, halved for lightning.
- `projectile_process_events_48_to_49` moves a projectile one square per event. Each step subtracts the step energy from kinetic energy and attack. Zero kinetic energy drops the object.
- `has_impact_occurred(impact_type, x, y, cell, thing)`: impact type −2 = party, −1 = creature, ≥ 0 = wall or door. A flask projectile (potion 0x13 or 0x03) becomes a fireball or poison cloud explosion.
- `projectile_get_impact_attack` returns the attack value. It sets `g_projectile_attack_type` (1 fire, 3 sharp, 5 magic, 7 lightning) and `g_projectile_poison_attack`.
- `projectile_get_impact_count` deletes all projectiles in a cell that hit, and returns the count.
- `explosion_process_event_25` applies the explosion effect, then removes it. Poison cloud and smoke repeat with less attack (−3 or −40) until they fade. Rebirth step 1 becomes step 2. Harm non-material damages non-material creatures. Lord Chaos (type 19) gets damage per creature. A fireball destroys a door with `group_is_door_destroyed_by_attack`.

### Group helpers and teleporter rotation

ReDMCSB `GROUP.C` tail and `MOVESENS.C` head.

- `group_is_fluxcage_on_square(x, y)`: true when an explosion of type 50 is on the square.
- `group_get_lord_chaos_on_square(x, y)`: the group thing when creature type 23 (Lord Chaos) is there, else 0.
- `group_fluxcage_action(x, y)` (was unnamed): creates a fluxcage (type 50) and event 24 after 100 ticks, which removes it. When Lord Chaos is on an adjacent square and 3 fluxcages surround him, it sends event −3 to his group. The existing CSV name `group_fluxcage_action` at `0x11cda2` was wrong.
- `group_fuse_action(x, y)` (`0x11cda2`): creates a harm non-material explosion. When Lord Chaos is on the square and 4 fluxcages surround him, it starts the fuse endgame sequence (`0x125132`). Otherwise Lord Chaos moves to a random free adjacent square.
- `get_square_distance`: Manhattan distance.
- `group_is_destination_visible(direction, sx, sy, dx, dy)`: true when the destination is in the 90° cone in that direction.
- `group_get_directions_to(sx, sy, dx, dy)` returns the main direction and writes the second direction to `g_group_secondary_direction`. The port returns both in a `DirectionPair`.
- `order_attack_cells` copies 4 cells from `g_ordered_cells_to_attack[direction * 2 + side]`.
- `group_get_champion_damage(group, champion)`: creature attack on a champion. It uses dexterity, luck, the map difficulty (`map[0x0c] >> 4`), invisibility, and the creature wound probabilities. It adds poison.
- `group_get_melee_action_damage(...)`: champion melee attack on a creature.
- `group_is_door_destroyed_by_attack(x, y, attack, magic, ticks)`: compares the attack with the door defense (`door_info[type].defense`). It sets the square door state to 5 (destroyed) at once, or adds event 2 (door destruction) after `ticks`.
- `move_get_teleporter_rotated_group_result` and `rotate_projectile` rotate a group or projectile by the teleporter rotation (teleporter `+2` bits 2–3 rotation, bit 4 absolute). The projectile direction goes to `0x10316a` (range 8, move result direction).

### Notes on globals of other ranges

- `g_creature_info` is at `0x10192e`, not `0x101930`. The code reads the 26-byte creature record at `0x10192e + 26 * type`. `0x101930` is offset 2 (attributes).
- `0x101d26` is `g_thing_data[15]` (explosion records).
- `0x101ed6` is the party thieves eye counter. `0x101f2a` is the party invisibility counter. Both are in the party record (range 5/6).
- `0x1001d4`–`0x1001d9` are memory and sound options (range 9 names them as memory options).

## Range 8: movement, sensors, timeline, commands, menus

Range 8 covers `0x11d842`–`0x123ae2` (101 functions). The order follows the ReDMCSB files MOVESENS.C, TIMELINE.C, COMMAND.C, and MENU.C. All word parameters are 16-bit. A thing is a 16-bit word: bits 14–15 cell, bits 10–13 type, bits 0–9 index. `0xfffe` ends a thing list. `0xffff` is "no thing" or "the party" (in move functions).

### Common structures

#### Timeline event (10 bytes, `TimelineEvent`)

| Offset | Size | Field |
|---|---|---|
| 0 | long | Bits 24–31: map index. Bits 0–23: game time. |
| 4 | byte | Event type. 0 = free slot. |
| 5 | byte | Priority. Champion events store the champion index here. |
| 6 | word | Data B. Square events: byte 6 = map X, byte 7 = map Y. |
| 8 | word | Data C. Square events: byte 8 = cell, byte 9 = effect. |

A free slot keeps the index of the next free slot in the word at offset 0. The free list ends with `0xffff`.

#### Sensor (thing type 3, 8 bytes)

| Offset | Bits | Field |
|---|---|---|
| 0 | word | Next thing. |
| 2 | 0–6 | Sensor type. 0 = disabled. |
| 2 | 7–15 | Sensor data (object type, creature type, counter, champion index, ...). |
| 4 | bit 3 (byte 4, `0x08`) | Local effect. |
| 4 | 7–10 | Delay in ticks before the effect event. |
| 5 | bit 2 (`0x04`) | Once only. The code clears the type after the first trigger. |
| 5 | bits 3–4 (`0x18`) | Effect: 0 set, 1 clear, 2 toggle, 3 hold (set while active, clear when not). |
| 5 | bit 5 (`0x20`) | Revert effect (invert the trigger condition). |
| 5 | bit 6 (`0x40`) | Audible: play sound 1 on trigger. |
| 6 | 0–3 | Ornament ordinal (not used by range 8). |
| 6 | 4–5 | Target cell. Local sensors: bits 4–15 hold the local effect. |
| 6 | 6–10 | Target map X. |
| 6 | 11–15 | Target map Y. |

Group generators (type 6) reuse word 4 bits 7–10 as creature count (bit 3 of this field set: random count `random16() % (n & 7)`) and word 6 bits 4–7 as health multiplier (0 = map difficulty from map byte 12 bits 4–7). Word 6 bits 8–15 hold the regeneration delay. A delay above 127 means `(delay − 126) × 64` ticks.

#### Square byte (current map)

Bits 5–7 hold the square type (0 wall, 1 corridor, 2 pit, 3 stairs, 4 door, 5 teleporter, 6 fake wall). Bit 4: the square has things. Other bits depend on the type:

- Door: bits 0–2 hold the door state. 0 open, 1–3 partly closed, 4 closed, 5 destroyed.
- Pit: bit 3 open, bit 0 imaginary, bit 2 invisible.
- Teleporter: bit 3 open (active).
- Fake wall: bit 2 open, bit 0 imaginary.
- Stairs: bit 2 up.

### Movement (`move_*`)

`move_get_move_result(thing, src_x, src_y, dst_x, dst_y)` moves a thing between squares. It is the central movement function. `thing = 0xffff` moves the party. `src_x < 0` means "add only". `dst_x < 0` means "remove only". The function returns 1 when the moved group died or left the map. For groups it can also return an active group code (see below).

Algorithm:

1. When a group moves on the party map, `move_is_killed_by_projectile_impact` runs first. When it returns true, the function returns 1.
2. Store the destination. For the party set `g_party_map_x/y`. Choose a teleporter scope mask: 1 creature, 2 party, 3 object.
3. Loop at most 99 times over the destination square:
   - Teleporter (type 5, open bit 3): the teleporter thing is the first thing of the square. Check its scope bits (byte 2 bits 5–6). Read target X (byte 3 bits 0–4), target Y (word 2 bits 5–9), target map (byte 4). Byte 2 bit 7 is audible: play sound 14. Byte 2 bit 4 selects absolute rotation; bits 2–3 hold the rotation. The party turns with `champion_set_party_direction`. Objects rotate their cell. Stop when the teleporter points to itself.
   - Open pit (type 2, bit 3 set, bit 0 clear), when the thing does not levitate: go one level down with `get_location_after_level_change(map, +1, &x, &y)`. The party takes `champion_damage_all(20, 0x30, 2)` and sound 7. When `g_use_rope_to_climb_down_pit` is set, each champion instead loses stamina `load × 25 / max_load + 1`. A falling group takes 20 damage through `group_damage_all_creatures`. Outcome 2 (all dead) ends the loop.
   - Stairs (type 3) for things other than the party and projectiles: go one level up or down (bit 2 clear: down), then step out in the stairs exit direction. The new cell faces away from the stairs.
   - Any other square ends the loop.
4. A group that died, or that is not allowed on the target map (`is_creature_allowed_on_map`), drops its possessions and is deleted.
5. Store the result in `g_move_result_map_x/y`, `g_move_result_map_index`, `g_move_result_cell`, and `g_move_result_direction`. The port returns them in a `MoveDestination`, and takes the move as a `Move` request.
6. Party move to a new square: add a scent. The scent list holds 24 entries. When full, `party_delete_scent(0)` drops the oldest. A scent word holds X (bits 0–4), Y (bits 5–9), and map (bits 10–15). `champion_add_scent_strength` updates strengths with the time since `g_last_party_movement_time`.
7. Remove the thing from the source with `sensor_process_thing_change(..., add=0)`. A levitating thing skips sensors and only unlinks.
8. Add the thing at the destination:
   - Party on a new map: set `g_new_party_map_index`. The game loop changes the map.
   - Party on the same map: a group on the square drops its possessions and is deleted. Then sensors run with `add=1`.
   - Group: when the square is free, play the creature move sound (`move_get_creature_sound`), add or remove the active group, link the group through the sensors, and call `group_start_wandering`. When the square is occupied, remove the group and create event 60/61 (`move_create_event_move_group`) to retry 5 ticks later.

`move_is_levitating(thing)` returns true for a group with the levitation attribute (creature attributes bit 5), and for projectiles (type 14) and explosions (type 15).

`move_is_killed_by_projectile_impact` builds a 4-byte table of occupied cells (party champions or group creatures). For a move to an adjacent square, it also checks the two cells that the mover crosses. It calls `has_impact_occurred` for every projectile on the squares whose event type is not 48 (ignore impacts). The projectile event index is in projectile data offset 6. The function returns true when the group died (`g_creature_damage_outcome == 2`, global `0x103158`, owned by range 7).

`move_get_creature_sound(type)` returns -1 when the party sleeps or the creature has no sound. Otherwise it reads the sound ordinal from creature record byte 1 and maps it through the word table at `0x101bea` (low byte).

Note for range 4: `g_creature_info` (`0x101930`) points to offset 2 of each 26-byte creature record. The record starts at `0x10192e`. Offset 1 holds the sound ordinal, offset 6 the movement ticks, offset 0x10 the properties word (fear resistance in bits 4–7).

### Sensors (`sensor_*`)

`sensor_add_event(type, x, y, cell, effect, time)` creates a square event on the current map. The priority byte depends on the effect: effect 0 → 1, effect 1 → 3, effect 2 → 2, effect 3 → 0.

`sensor_trigger_effect(sensor, effect, x, y, cell)` runs a sensor:

- Once only (byte 5 bit 2): clear the sensor type.
- Remote effect: look up the event type from the target square type with `g_square_type_to_event_type` (`0x100d4a`: wall 6, corridor 5, pit 9, stairs 0, door 10, teleporter 8, fake wall 7). Only wall targets keep the target cell. The event time is `g_game_time + delay`.
- Local effect: call `trigger_local_effect(word6 >> 4, x, y, cell)`.

Local effects: 10 adds 300 experience in skill 8 (to the leader, or to all champions when `cell == -1`). Effects 1 and 2 store a rotation request in `g_sensor_rotation_effect*`. `sensor_process_rotation_effect` runs after each sensor pass. It moves the first sensor of the matching cell to the end of the sensor list of that cell.

`sensor_process_thing_change(x, y, thing, party_on_square, add)` runs floor or wall sensors when a thing enters or leaves a square. It first scans the square: a group present, objects present, objects of the same type, objects of a different type. On a corridor (no wall), text things (type 2) print when the party enters. Floor sensor types:

| Type | Name | Trigger |
|---|---|---|
| 1 | Theron party/creature/object | Anything enters or leaves (no other objects, no group). |
| 2 | Theron party/creature | Party or group, no objects or group left. |
| 3 | Party | Party enters. Non-zero data: only when the party faces direction `data − 1`. |
| 4 | Object | Object of type `data` (no other of that type). |
| 5 | Party on stairs | Party on a stairs square. |
| 7 | Creature | A group. |
| 8 | Party possession | `is_object_possessed(data)`. |
| 9 | Version checker | True when `data < 37`. |

On a wall square the same function handles objects in a cell: wall sensor types 1, 2 (any object), and 3 (specific object).

The trigger value is XORed with "revert" (byte 5 bit 5). Effect 3 (hold) converts to set or clear. Otherwise the sensor triggers only when the value is true.

`sensor_is_wall_triggered(x, y, cell)` handles a click on the front wall. Wall sensor types:

| Type | Name |
|---|---|
| 1 | Ornament click. |
| 2 | Click with any object (revert flag compares with "empty handed"). |
| 3 | Click with specific object. |
| 4 | Click with specific object removed (the object is consumed). |
| 5 | AND/OR gate (event 6 only). |
| 6 | Counter (event 6 only). |
| 7–10, 14, 15 | Projectile launchers (event 6 only). |
| 11 | Single object storage, rotate. |
| 12 | Single object generator, rotate. |
| 13 | Single object exchanger (alcove). |
| 16 | Object exchanger. |
| 17 | Click with specific object removed, the sensor is removed. |
| 18 | End game (event 6 only). |
| 127 | Champion portrait: `champion_add_candidate_to_party(data)`. |

Only sensors for the clicked cell count. Data is the object type. The function returns true when any sensor triggered.

### Timeline (`timeline_*`)

The timeline keeps future events in two arrays:

- `g_timeline.events`: `g_timeline.capacity` slots of 10 bytes.
- `g_timeline.heap`: a binary min-heap of slot indices (uint16). `g_timeline.count` (`0x103180`) holds the heap size.
- `g_timeline.first_unused` (`0x103182`) heads the free list.
- `g_timeline.high_water` (`0x103176`, CSV name `g_event_count`) holds the highest used slot + 1. Linear scans use it as a bound. `timeline_reset` rebuilds it, the free list, and the heap size after a load. The save file stores the heap, the heap size, and the free list head.

Heap order (`is_before`): earlier time (bits 0–23) first. On equal time, the larger word at offset 4 (type × 256 + priority) first. On a full tie, the lower slot address first. `timeline_fix_chronology(i)` sifts the entry at heap position `i` up, and down when it did not move up. `timeline_get_index` finds a slot in the heap with a linear search. It calls `fatal_error(0x46)` when the slot is missing.

`timeline_add_event(event)`:

1. Type 0: return -1.
2. A full timeline calls `fatal_error(0x2d)`.
3. `find_mergeable` can merge the event:
   - Square events 5–10: an event with the same time, map, and X/Y (and the same cell for type 6) gets the new effect byte. Return its index.
   - New type 1 (door animation): an existing type 10 event at the same time and place with toggle effect gets effect `1 − new`. An existing type 1 event gets the new effect.
   - New type 2 (door destruction): delete all type 1 and type 10 events of that square and map.
   - Any other new type: an existing type 1 event with the same time and place is deleted. A toggle effect in the new event becomes `1 − old effect`.
4. Take a slot from the free list, copy 10 bytes, append the slot to the heap, and sift up.

`timeline_extract_first_event` copies the heap root and deletes it. `timeline_is_first_event_expired` compares the root time with `g_game_time`.

`timeline_process` runs every expired event. It sets the current map from the event, dispatches on the type, and restores the party map. Event types and handlers:

| Type | Name | Handler / action |
|---|---|---|
| 1 | Door animation | `timeline_process_event1_door_animation` |
| 2 | Door destruction | `timeline_process_event2_door_destruction` (state 5) |
| 5 | Corridor | `timeline_process_event5_corridor` |
| 6 | Wall | `timeline_process_event6_wall` |
| 7 | Fake wall | `timeline_process_event7_fake_wall` |
| 8 | Teleporter | `timeline_process_event8_teleporter` |
| 9 | Pit | `timeline_process_event9_pit` |
| 10 | Door | `timeline_process_event10_door` (converts to type 1) |
| 11 | Enable champion action | `timeline_process_event11_enable_champion_action`, then `timeline_move_weapon_from_quiver_to_slot(champion, byte6 − 1)` when byte 6 ≠ 0 |
| 12 | Hide damage received | `timeline_process_event12_hide_damage_received` |
| 13 | Vi altar rebirth | `timeline_process_event13_vi_altar_rebirth` |
| 20 | Play sound | `sound_request_play(word8, byte6, byte7, 1)` |
| 24 | Remove fluxcage | Unlink thing `word8` and free it, unless `g_game_won` |
| 25 | Explosion | `explosion_process_event_25` (range 7) |
| 29–41 | Group events | `group_process_events_29_to_41(byte6, byte7, type, byte8)` (range 4) |
| 48, 49 | Move projectile (48 ignores impacts) | `projectile_process_events_48_to_49` (range 7) |
| 53 | Watchdog | `timeline_add_watchdog_event` (re-adds itself every 300 ticks) |
| 60, 61 | Move group (61 audible) | `timeline_process_event60_61_move_group` |
| 65 | Enable group generator | `timeline_process_event65_enable_group_generator` |
| 70 | Light | `timeline_process_event70_light`, then `set_dungeon_view_palette` |
| 71 | Invisibility end | Decrement `g_event71_count_invisibility`. At 0 redraw all champions. |
| 72 | Champion shield end | Subtract word 6 from champion +0x112, redraw. |
| 73 | Thieves eye end | Decrement `g_event73_count_thieves_eye`. |
| 74 | Party shield end | Subtract word 6 from `g_party_shield_defense`. |
| 75 | Poison | Decrement champion +0x2a, `champion_poison(champion, word6)`. |
| 77 | Spell shield end | Subtract word 6 from `g_spell_shield_defense`. |
| 78 | Fire shield end | Subtract word 6 from `g_fire_shield_defense`. |
| 79 | Footprints end | Decrement `g_event79_count_footprints`. |

Other types below 80 do nothing. The jump table is at `0x120a5e` (80 word offsets).

Square event effects: 0 = set (open, activate), 1 = clear (close, deactivate), 2 = toggle. A handler resolves toggle against the current state.

- Door animation (1): the door state moves one step per event: effect 0 decreases (opens), effect 1 increases (closes). Each step plays sound 2 and re-adds the event at time + 1, until state 0 or 4. When the door closes on the party, the party takes `champion_damage_all(5, 4, 2)` and the door goes back one step. A closing door hits a group that is not non-material when the creature height is at least the door state (a vertical door uses creature attributes bits 7–8, else height 1): 5 damage, `group_process_events_29_to_41(x, y, -3, 0)`, sound 4.
- Fake wall (7): effect 0 sets bit 2. Effect 1 clears it. Closing waits (re-adds at time + 1) while the party or a material group is on the square.
- Teleporter (8), pit (9): effect 0 sets bit 3 and calls `timeline_move_teleporter_or_pit_square_things`. Effect 1 clears bit 3. The two handlers have identical code.
- `timeline_move_teleporter_or_pit_square_things(x, y)` re-runs `move_get_move_result(thing, x, y, x, y)` for the party, the group, and each object. Projectiles and explosions get their event data updated from the `g_move_result_*` globals (in the port: from the `MoveDestination`).
- Corridor (5): text things toggle visibility (byte 3 bit 0). The party sees new text as a message. Group generator sensors (type 6) create a group with `group_get_generated`, play sound 14 when audible, and disable themselves. A non-once-only generator re-enables through event 65 after its regeneration delay.
- Wall (6): text things in the event cell toggle. Counter sensors (6) count up (effect 0, max 511) or down, and trigger at 0. AND/OR gates (5) set or clear bit `cell` of the data, and compare bits 4–7 with bits 0–3. Launchers in the event cell call `timeline_trigger_projectile_launcher`. Sensor 18 waits `delay × 60` frames, sets `g_game_won`, and calls `endgame(1)`.
- `timeline_trigger_projectile_launcher(sensor, event)`: types 7, 8, 14 launch one projectile, types 9, 10, 15 launch two. Types 8 and 10 launch explosions (`data − 128`). Types 14 and 15 launch objects from the launcher square (cells in front). Types 7 and 9 create objects with `get_object_for_launcher_or_generator`. Kinetic energy is word 6 bits 4–11. Step energy is word 6 bits 12–15. The attack is 100. A single projectile picks a random side cell. The projectile starts on the adjacent square in direction `cell` with `g_create_launcher_projectile` set.
- Move group (60/61): when the party or a group still blocks the square, retry at time + 5. Lord Chaos (creature type 23) can instead step to a random adjacent free square (1 in 4).
- Enable champion action (11): clear the action event index (+0x2c) and the "action disabled" flag, remove the action defense, redraw the action hand. When the last action was SHOOT (32) and the ready hand is empty, move ammunition from the quiver (slot 12, then 7–9).
- Vi altar rebirth (13): byte 9 is the step. Step 2 creates explosion thing `0xffe4` at the altar, step 1 removes the bones (icon 0x93) of the champion, step 0 resurrects the champion (`0x113a12`, range 5). Each step re-adds the event.
- Light (70): word 6 holds the signed light power. The handler applies one step of `g_light_power_to_light_amount` (`0x100cb4`) to `g_magical_light_amount` and re-adds the rest 4 ticks later.

### Commands (`command_*`, `highlight_*`)

#### Mouse and keyboard input tables

A `MouseInput` entry has 6 words: command, x1, x2, y1, y2, button mask. A command of 0 ends the table. When x1 is negative, x2 holds a layout zone index: -1 screen coordinates, -2 relative to the viewport origin (`0x10293a`, `0x10293c`), -3 relative to the centered viewport. `command_init_mouse_input_table` converts zone entries to boxes at start-up. `command_get_command_from_mouse_input` resolves the remaining zone entries at click time.

A `KeyboardInput` entry has 2 words: command, key code. A command of 0 ends the table.

The active tables are `g_primary_mouse_input`, `g_secondary_mouse_input`, `g_primary_keyboard_input`, and `g_secondary_keyboard_input`. Sleep and freeze swap them (freeze saves the old pointers).

Button codes from `input_event_handler`: 2 left press, 4 left release, 1 right press, 8 right release. Code `0x21` comes from a deferred click (`0x1004f6`).

#### Command queue

`g_command_queue` holds 9 entries of 6 bytes: x, y, command (words). `g_command_queue_first_index` and `g_command_queue_last_index` wrap at 9. `g_command_queue_count` counts entries. `command_process_click` accepts a click only when fewer than 5 entries wait (7 for release events). Left release queues command 254, code `0x21` queues command 129. `command_process_key` queues a key only when fewer than 5 entries wait. `command_discard_all_input` drains the key queue and keeps only queued release commands (129, 254). `g_command_queue_locked` guards the queue against the input interrupt. A click that arrives while the queue is locked is stored in `g_pending_click_*`. `command_process_pending_click` replays it.

`command_process_queue` takes one command. Movement commands wait while `g_disabled_movement_ticks` is non-zero, or while a projectile blocks the move direction. Command numbers:

| Command | Action |
|---|---|
| 1, 2 | Turn left, right (`command_turn_party`). |
| 3–6 | Move forward, right, backward, left (`command_move_party`). |
| 7–10 | Toggle inventory of champion 0–3. 11 closes the inventory. |
| 12–15 | Click in champion status box 0–3. |
| 16–19 | Set leader 0–3 (from status box). |
| 20–65 | Click on slot box `command − 20` (hands, inventory, chest). |
| 70 | Click on mouth. |
| 71 | Click on eye. |
| 80 | Click in dungeon view. |
| 81 | Click in panel (chest or resurrect/reincarnate). |
| 83 | Toggle inventory of the leader. |
| 100 | Click in spell area. |
| 101–106 | Rune symbol 0–5 (from the spell area table). |
| 107 | Delete last symbol. |
| 108 | Cast spell. |
| 109 | Change magic caster. |
| 111 | Click in action area. |
| 112 | Pass (no action). |
| 113–115 | Action 0–2. |
| 116–119 | Select acting champion 0–3. |
| 125–128 | Champion icon click (`set_mouse_pointer_to_champion`). |
| 129 | Right button release (`restore_champion_mouse_pointer`). |
| 140 | Save game. |
| 145 | Sleep. |
| 146 | Wake up. |
| 147 | Freeze game. |
| 148 | Unfreeze game. |
| 165–198 | Character key for name entry (stored in `0x102460`). |
| 200 | Set `g_new_game`. |
| 201 | Set `g_new_game` and `0x104158` (load the second dungeon file). |
| 202 | Clear `g_new_game`. |
| 203 | Calls `0x1270c8` (range 9). |
| 210–213 | Dialog choice 1–4 (stored in `0x10402a`). |
| 215 | Set `0x10415c` (range 9 flag). |
| 216 | `quit_game_thunk`. |
| 254 | Left button release: stop pressing eye, mouth, or the hand in a fake wall. |

#### Party movement

`command_move_party(command)` charges every living champion `load × 3 / max_load + 1` stamina. It highlights the arrow zone `command + 0x43`. It computes the target square with `g_movement_arrow_to_step_forward_count` and `g_movement_arrow_to_step_right_count`. Stairs lead to `command_take_stairs`. Walls, closed doors (state not 0, 1, or 5), and solid fake walls block the move. A blocked move hurts the two front champions (24 damage, wound mask 2) and plays sound 15. A group on the target square blocks the move and gets event −1 (`group_process_events_29_to_41`). A successful move sets `g_disabled_movement_ticks` to the maximum movement ticks of the living champions.

`command_turn_party` turns the party. On stairs it takes the stairs. It removes the party from the square sensors before the turn and adds it after the turn.

#### Dungeon view clicks

`command_process_click_in_dungeon_view(x, y)` converts the click to viewport coordinates. Range 7 globals describe the front square: `0x101d2a` (front element, 0x11 = door), `0x101d2c` (alcove), `0x101d2e` (vi altar), `0x101d30` (fountain), `0x101d36` (6 clickable boxes: 4 object cells, 1 unused, 1 wall ornament), `0x101d5e` (door button box), `0x101d66` (top object of each cell).

- Door button: `sensor_add_event(10, front, cell 0, toggle, time + 1)`.
- Empty hand on an object cell: `command_grab_leader_hand_object`.
- Empty hand on the wall ornament: drink from a fountain (water of the leader = 2048, sound 8), or `command_touch_front_wall`.
- Hand object over zones `0x2e4`–`0x2e7`: `command_drop_leader_hand_object`. Over zones `0x2e9`/`0x2ea` (`0x2ec`/`0x2ed` with a door): throw (`champion_is_leader_hand_object_thrown`).
- Hand object on an alcove: drop into the alcove (cell 4). On a vi altar, bones (icon 0x93) create event 13.
- Hand object on a fountain: fill a flask (icon 8 or 9 sets 3 charges) or a waterskin (icon 0xc3), then touch the wall.

#### Highlight

`highlight_box_enable(box)` copies the box to `g_highlight_box`, XORs it on screen, and waits one frame. `highlight_box_disable` XORs it back. `highlight_zone_enable(zone)` resolves a layout zone first.

### Menus (`menus_*`)

#### Action area

`menus_init` loads graphic 699 (action names: 44 zero-terminated strings) into `g_action_names`.

Action set table `g_action_sets` (`0x103d4c`, 44 × 8 bytes, indexed by object info byte 3): bytes 0–2 action indices (`0xff` = none), bytes 4–5 minimum skill levels of actions 1 and 2 (bit 7 = the action needs charges). Byte 6 is copied to `0x103fea`, no reader is known.

`menus_set_acting_champion` selects the champion, builds the action list with `menus_set_action_list`, and draws the action names. An empty action hand uses action set 2. Actions 1 and 2 need the skill level in `g_action_skill_index`. Charged actions also need `menus_get_action_hand_charge_count > 0`.

Per-action tables (44 bytes each, indexed by action):

| Address | Global | Use |
|---|---|---|
| `0x103eac` | `g_action_disabled_ticks` | `champion_disable_action` duration. |
| `0x103ed8` | `g_action_damage_factor` | Melee damage factor. |
| `0x103f04` | `g_action_hit_probability` | Melee hit probability (bit 15 added for SHOOT-like actions). |
| `0x103f30` | `g_action_stamina` | Stamina cost (plus a random bit). |
| `0x103f5c` | `g_action_defense` | Signed defense modifier while the action runs. |
| `0x103f88` | `g_action_skill_index` | Skill that the action trains. |
| `0x103fb4` | `g_action_experience_gain` | Experience gain. |

`menus_is_action_performed(champion, action)` runs the action against the square in the champion direction. Action indices: 2 CHOP, 4 BLOW HORN, 5 FLIP, 6 PUNCH, 7 KICK, 8 WAR CRY, 9 STAB, 10 CLIMB DOWN, 11 FREEZE LIFE, 13 SWING, 14 STAB, 15 THRUST, 16 JAB, 17 PARRY, 18 HACK, 19 BERZERK, 20 FIREBALL, 21 DISPELL, 22 CONFUSE, 23 LIGHTNING, 24 DISRUPT, 25 MELEE, 27 INVOKE, 28 SLASH, 29 CLEAVE, 30 BASH, 31 STUN, 32 SHOOT, 33 SPELLSHIELD, 34 FIRESHIELD, 35 FLUXCAGE, 36 HEAL, 37 CALM, 38 LIGHT, 39 WINDOW, 40 SPIT, 41 BRANDISH, 42 THROW, 43 FUSE. Melee actions on a closed door bash the door (`0x11d5e0`). Other melee actions call `menus_is_melee_action_performed`. The result goes into `g_action_damage`: −1 "can't reach", −2 "need ammo", otherwise damage. `menus_refresh_action_area` draws it with `menus_draw_action_damage`. After the action the function disables the champion action, decrements stamina, and adds experience.

`menus_is_group_frightened_by_action` (WAR CRY, BLOW HORN, CONFUSE, CALM, BRANDISH) compares `random16() % (skill + bonus)` with the creature fear resistance. A frightened group flees for `(16 − fear) × 4 / movement_ticks`.

`menus_refresh_action_area` runs every tick from the game loop. It also turns idle champions to the direction stored at +0x28 (set by damage) when the flag at +0x29 is set.

#### Spell area

Symbols are characters `0x60 + step × 6 + n`. Champion record +0x21 holds the symbol step (0–3), +0x22 holds up to 4 symbols and a terminating zero.

`menus_add_champion_symbol(n)` costs `g_symbol_base_mana_cost[step][n]` mana. For steps after the first, the cost is `base × g_symbol_mana_cost_multiplier[power] >> 3`. The power symbol is the first symbol.

`g_spells` (`0x103c84`): 25 entries of 8 bytes.

| Offset | Field |
|---|---|
| 0 | Long: symbols, first symbol in bits 24–31. Bits 24–31 = 0: the spell has no power symbol, compare only bits 0–23. |
| 4 | Base required skill level. |
| 5 | Skill index. |
| 6 | Word: bits 0–3 kind (1 potion, 2 projectile, 3 other), bits 4–9 type, bits 10–15 action disabled ticks. |

`menus_get_champion_spell_cast_result`:

1. Find the spell (`menus_get_spell_from_symbols`). No match: message "meaningless spell".
2. Power = first symbol − 0x5f (1–6). Required level = power + base level. Experience = `(random & 7) + 16 × level + 8 × (power − 1) × base + D3²`. The intended last term is `level²` (ReDMCSB). The compiled code squares register D3 before it stores the level in D3, so it uses a stale value from the caller. A port should use `level²`.
3. When the skill level is too low, each missing level gives a failure chance against the current wisdom (champion offset `0x50`, `min(wisdom + 15, 115)`). A failure adds reduced experience and prints "needs more practice".
4. Potion: needs an empty flask (icon 0xc3) in a hand, else message "need flask" and return 3. The flask becomes potion `type` with power `power × 40 + random(16)`.
5. Projectile: turn the champion to the party direction and shoot explosion `type − 128` with kinetic energy `bounded(21, (power + 2) × (4 + 2 × skill), 255)`. Type 4 doubles the skill.
6. Other: 0 light, 1 darkness, 2 thieves eye (event 73), 3 invisibility (event 71), 4 party shield (event 74), 5 magic torch, 6 footprints (event 79), 7 zokathra (creates junk 10), 8 fire shield.

`menus_print_spell_failure_message`: type 0 = more practice (strings 0x33/0x34 for skill 3, else 0x35/0x36), type 1 = meaningless (0x37/0x38), type 10 = need flask (0x39/0x3a).

### Uncertain items

- `command_touch_front_square_empty_handed` (`0x12159a`): plays sound 4 on a normal front square. On an imaginary fake wall it hides the mouse pointer while the right button is down. The ReDMCSB name is not confirmed.
- `g_timeline.high_water` replaces the CSV name `g_event_count` (`0x103176`). The CSV name is misleading: the real event count is `g_timeline.count` (`0x103180`).
- `0x103fea` (action set byte 6) has no known reader.
- The setter of `g_pending_click_present` is not visible in the Ghidra output.

## Range 9: OS stubs, dialogs, endgame, save and load, start

### OS stubs (`0x123ba6`–`0x1244d2`)

Each stub saves D3–D7/A2–A6, loads a library base into A6, moves the stack arguments into registers, and calls the LVO. The stubs are in alphabetical order of the original `amiga.lib` names. All 70 LVOs match the names. A C port replaces every stub with a host call or removes it.

| Stub | Library (global) | LVO | Amiga call and registers |
|---|---|---|---|
| `os_add_int_server` | exec (`g_sys_base`) | −168 | `AddIntServer(D0 intNum, A1 interrupt)` |
| `os_add_port` | exec | −354 | `AddPort(A1 port)` |
| `os_alert` | exec | −108 | `Alert(D7 alertNum, A5 parameters)`. A5 is overwritten. |
| `os_alloc_mem` | exec | −198 | `AllocMem(D0 byteSize, D1 requirements)` |
| `os_alloc_signal` | exec | −330 | `AllocSignal(D0 signalNum)` |
| `os_avail_mem` | exec | −216 | `AvailMem(D1 requirements)` |
| `os_change_sprite` | graphics (`g_gfx_base`) | −420 | `ChangeSprite(A0 vp, A1 sprite, A2 newData)` |
| `os_check_io` | exec | −468 | `CheckIO(A1 ioRequest)` |
| `os_close` | dos (`g_dos_base`) | −36 | `Close(D1 file)` |
| `os_close_device` | exec | −450 | `CloseDevice(A1 ioRequest)` |
| `os_close_library` | exec | −414 | `CloseLibrary(A1 library)` |
| `os_cd_input_handler` | console device (`g_console_device`) | −42 | `CDInputHandler(A0 events, A1 consoleDevice)` |
| `os_raw_key_convert` | console device | −48 | `RawKeyConvert(A0 event, A1 buffer, D1 length, A2 keyMap)` |
| `os_date_stamp` | dos | −192 | `DateStamp(D1 date)` |
| `os_delay` | dos | −198 | `Delay(D1 ticks)` |
| `os_delete_file` | dos | −72 | `DeleteFile(D1 name)` |
| `os_device_proc` | dos | −174 | `DeviceProc(D1 name)` |
| `os_disable` | exec | −120 | `Disable()` |
| `os_disown_blitter` | graphics | −462 | `DisownBlitter()` |
| `os_display_alert` | intuition (`g_intuition_base`) | −90 | `DisplayAlert(D0 alertNumber, A0 string, D1 height)` |
| `os_do_io` | exec | −456 | `DoIO(A1 ioRequest)` |
| `os_enable` | exec | −126 | `Enable()` |
| `os_examine` | dos | −102 | `Examine(D1 lock, D2 fileInfoBlock)` |
| `os_new_list` | none | — | `NewList(list)` (`amiga.lib`, inline code) |
| `os_create_ext_io` | none | — | `CreateExtIO(port, size)` (`amiga.lib`): `AllocMem(size, MEMF_PUBLIC|MEMF_CLEAR)`, type 5, reply port at +14, length at +18 |
| `os_delete_ext_io` | none | — | `DeleteExtIO(io)` (`amiga.lib`): poisons type, port and device fields, `FreeMem` |
| `os_create_std_io` | none | — | `CreateStdIO(port)` = `CreateExtIO(port, 48)` |
| `os_delete_std_io` | none | — | `DeleteStdIO(io)` = `DeleteExtIO(io)` |
| `os_find_port` | exec | −390 | `FindPort(A1 name)` |
| `os_find_task` | exec | −294 | `FindTask(A1 name)` |
| `os_forbid` | exec | −132 | `Forbid()` |
| `os_free_mem` | exec | −210 | `FreeMem(A1 memoryBlock, D0 byteSize)` |
| `os_free_signal` | exec | −336 | `FreeSignal(D0 signalNum)` |
| `os_free_sprite` | graphics | −414 | `FreeSprite(D0 pick)` |
| `os_get_msg` | exec | −372 | `GetMsg(A0 port)` |
| `os_get_sprite` | graphics | −408 | `GetSprite(A0 sprite, D0 pick)` |
| `os_init_bitmap` | graphics | −390 | `InitBitMap(A0 bitMap, D0 depth, D1 width, D2 height)` |
| `os_init_rastport` | graphics | −198 | `InitRastPort(A1 rp)` |
| `os_load_view` | graphics | −222 | `LoadView(A1 view)` |
| `os_lock` | dos | −84 | `Lock(D1 name, D2 accessMode)` |
| `os_make_screen` | intuition | −378 | `MakeScreen(A0 screen)` |
| `os_move_sprite` | graphics | −426 | `MoveSprite(A0 vp, A1 sprite, D0 x, D1 y)` |
| `os_open` | dos | −30 | `Open(D1 name, D2 accessMode)` |
| `os_open_device` | exec | −444 | `OpenDevice(A0 devName, D0 unit, A1 ioRequest, D1 flags)` |
| `os_init_sys_base` | none | — | Copies the long at address 4 (ExecBase) into `g_sys_base`. |
| `os_reboot` | exec | −150 | `SuperState()`, then SR = `0x2700`, writes `RESET; JMP` at address 0 and jumps there. This reboots the machine. Called from two places in the system-init range. |
| `os_open_library` | exec | −552 | `OpenLibrary(A1 libName, D0 version)` |
| `os_own_blitter` | graphics | −456 | `OwnBlitter()` |
| `os_permit` | exec | −138 | `Permit()` |
| `os_create_port` | none | — | `CreatePort(name, pri)` (`amiga.lib`): `AllocSignal(-1)`, `AllocMem(34, MEMF_PUBLIC|MEMF_CLEAR)`, `AddPort` when the name is not null. |
| `os_delete_port` | none | — | `DeletePort(port)` (`amiga.lib`): `RemPort`, poisons fields, `FreeSignal`, `FreeMem(port, 34)`. |
| `os_begin_io` | device in `io_Device` (+20) | −30 | `BeginIO(A1 ioRequest)` |
| `os_put_msg` | exec | −366 | `PutMsg(A0 port, A1 message)` |
| `os_read` | dos | −42 | `Read(D1 file, D2 buffer, D3 length)` |
| `os_rect_fill` | graphics | −306 | `RectFill(A1 rp, D0 xMin, D1 yMin, D2 xMax, D3 yMax)` |
| `os_rem_int_server` | exec | −174 | `RemIntServer(D0 intNum, A1 interrupt)` |
| `os_rem_port` | exec | −360 | `RemPort(A1 port)` |
| `os_rename` | dos | −78 | `Rename(D1 oldName, D2 newName)` |
| `os_rethink_display` | intuition | −390 | `RethinkDisplay()` |
| `os_seek` | dos | −66 | `Seek(D1 file, D2 position, D3 mode)` |
| `os_set_a_pen` | graphics | −342 | `SetAPen(A1 rp, D0 pen)` |
| `os_set_dr_md` | graphics | −354 | `SetDrMd(A1 rp, D0 drawMode)` |
| `os_set_function` | exec | −420 | `SetFunction(A1 library, A0 funcOffset, D0 newFunction)` |
| `os_set_rgb4` | graphics | −288 | `SetRGB4(A0 vp, D0 index, D1 r, D2 g, D3 b)` |
| `os_unlock` | dos | −90 | `UnLock(D1 lock)` |
| `os_wait_blit` | graphics | −228 | `WaitBlit()` |
| `os_wait_io` | exec | −474 | `WaitIO(A1 ioRequest)` |
| `os_wait_port` | exec | −384 | `WaitPort(A0 port)` |
| `os_wait_tof` | graphics | −270 | `WaitTOF()` |
| `os_write` | dos | −48 | `Write(D1 file, D2 buffer, D3 length)` |

The game uses `os_delay(175)` (3.5 s) after it closes a saved game file.

### Dialog

The dialog system shows a message box with one to four choices. ReDMCSB calls it `DIALOG.C`.

#### Globals

- `g_game_screen_inactive` (`0x104024`) starts at 1. `game_loop` sets it to 0. `endgame` sets it to 1. `start_game` sets it to 0 for a new game. When it is 1, the dialog uses the full screen: `dialog_draw` fades the palette to black, clears the screen, and `dialog_draw_viewport` blits the viewport bitmap to screen zone 5. When it is 0, `dialog_draw_viewport` calls `draw_viewport(0)`. `game_freeze` and `game_unfreeze` do nothing when it is 1.
- `g_dialog_choice` (`0x10402a`). The command handler writes the clicked choice number here. The value 99 means "no choice yet".
- `g_dialog_message_colors` (`0x104026`): text colors of message 1 (11) and message 2 (9).
- `g_dialog_version_text` (`0x10402c`): the string `V3.6`. The dialog prints it in zone `0x1c2`.
- `g_dialog_mouse_inputs` (`0x103bd4`): 8 pointers to mouse input tables. Index = `g_game_screen_inactive * 4 + choice_count − 1`.

#### `dialog_draw(message1, message2, choice1, choice2, choice3, choice4)`

1. Expand the dialog graphic (pointer at `0x100222`) into `g_viewport_bitmap`. Print the version text.
2. Count the choices: 1 to 4 non-null choice pointers, in order.
3. Patch the box. 1 choice: layout `-0x36`, graphic zone `0x1c3`. 2 choices: layout `-0x37`, zone `0x1c4`. 4 choices: layout `-0x38`, zone `0x1c5`. 3 choices: no patch. The patch uses a temporary bitmap (a derived-cache slot when the memory option at `0x10253a` is set).
4. Print the choice texts in color 9 on background 5. Zones: 1 choice `0x1ce`; 2 choices `0x1cf`, `0x1ce`; 3 choices `0x1cf`, `0x1d2`, `0x1d3`; 4 choices `0x1d0`–`0x1d3`.
5. Print the two messages centered in the message zone (`0x1d5` for 1 choice, else `0x1d7`). A message that is wider than 7/8 of the zone width is split into two lines with `text_get_next_word_line`. The block of lines is centered vertically.
6. Show the result with `dialog_draw_viewport`. In full-screen mode fade in to the palette at `0x100170`.

#### `dialog_get_choice(choice_count, disk_change_drive, disk_change_choice)`

1. Save the four mouse/keyboard input table pointers (`0x103c04`–`0x103c10`). Install the dialog table as primary mouse input. Clear the other three.
2. Show the mouse pointer (undo all hide requests). Set `g_dialog_choice` to 99.
3. Loop: `command_process_queue`, `wait_vertical_blank`. With one choice, the Return key (`0x4400`) selects choice 1. When `has_disk_changed(disk_change_drive)` is true, the choice is `disk_change_choice`.
4. Hide the pointer again (restore the hide count).
5. Animate the pressed button: draw a frame around the button box in colors 5, 0, 13, wait 5 frames, draw the pressed frame in colors 5, 12, 1. A mouse input entry is 12 bytes: command word, then `x1, x2, y1, y2`, then a button mask. When `x1` is negative, `x2` holds a zone index and the box comes from `get_zone_box`.
6. Restore the input tables. Return the choice number (1-based).

### Start and end

#### Game start

- `start_game` clears pending mouse/click state, the sleep flag, `g_game_frozen`, and the 32-byte block at `0x102490`. It installs the four main input tables (`0x1031f8`, `0x1032e8`, `0x103948`, `0x103964`) and calls `process_new_party_map(g_party_map_index)`.
- Loaded game: clear screen, show "GAME LOADED, READY TO PLAY." with OK, then `ensure_master_disk(0)` (it closes `Graphics.DAT` for the time of the check). New game: clear `g_game_screen_inactive` and fill screen zones `0x191`, `0x193`, `0x195` with color 0.
- Both paths set the top palette `0x100aa6` and bottom palette `0x100170`, wait until the vertical-blank handler applies them, draw the interface (`0x1235fe`, `0x113abe`), and set `g_game_time_ticking`.
- Ghidra shows an endless loop at the end of `start_game`. This is wrong. The palette wait loop confuses it. Read the disassembly.

#### `initialize_game` order

`set_language_file_names`, `system_init`, `detect_floppy_drives`, `install_input_handler`, `graphics_dat_read_header`, `init_layout`, `load_string_table`, `init_graphic_data`, `text_init`, `sound_init`, `0x1175c6(0)`, `0x1176aa(0)`, `menus_init`, `load_object_names`, `command_init_mouse_zones`, `init_mouse_pointer`, `process_entrance`. Then loop `load_game` until it returns 1; after each failure call `graphics_dat_open` and `process_entrance` again. Then `graphics_dat_open`, `heap_set_reserve(g_memory_option_list)`, `sound_disable_unloaded`, `start_game`, `graphics_dat_close`. For a new game: `move_get_move_result(0xffff, -1, 0, x, y)` puts the party on the start square. Last: `mouse_show_pointer`, `command_discard_all_input`.

Ghidra drops the pointer argument of `heap_set_reserve`. The real argument is `0x104134`.

#### Memory options

`heap_set_reserve` (range 6) tries option sets from `g_memory_option_list` (`0x104134`) until the game fits in memory. The list is bytes:

- `0..5`: set flag `g_memory_options[n]` (`0x1001d4`) to 1.
- `0xfd n`: reserve `n << 10` bytes.
- `0xfe`: end of one set.
- `0xff`: end of the list.

The list is `{0,3,4,5,2,1, reserve 120K}`, `{0,3,4,5,1}`, `{3,4,5,1}`, `{3,4,1}`, `{3,4}`, `{3}`, `{}`. `parse_memory_option_set` clears the six flags, reads one set, and returns true when more sets follow. `g_memory_options[1]` (`0x1001d5`) enables a second set of wall bitmaps for the mirrored view (`init_flipped_wall_bitmaps`; `draw_dungeon_view` reads the same flag). The meaning of the other flags is not traced.

`check_memory_for_all_maps(is_last_set, reserve)` sets the flag at `0x10254e` (memory-check mode), calls `process_new_party_map` for every map, and takes the minimum of `0x102550 − reserve`. It keeps the best value in `g_best_free_memory` (`0x104154`, initial −10000000). When the last set still does not fit, it shows "OUT OF MEMORY. ~K NEEDED." (string `0x87`, value `(0x3ff − best) >> 10`) and calls `quit_game`. It returns true when the minimum is not negative.

A C port with enough memory can use the first set and skip this search.

#### Graphic data initialization

`init_graphic_data` (ReDMCSB `F0460_START_InitializeGraphicData`) runs once:

1. Allocate layout bitmaps for wall, floor, ceiling, and door graphics (`layout_alloc_graphic_bitmap(layout, graphic)`, graphics `0x4e`–`0x6b`). Some layouts share a bitmap (`layout_share_bitmap`).
2. Move the current `g_viewport_bitmap` to layout `-0x35` and allocate a new viewport bitmap.
3. Compute the sizes of the floor and ceiling parts of the viewport and the largest of three scratch bitmaps.
4. Fill the derived-bitmap byte-count table (`0x102540`) with the scaled sizes of all wall ornaments, door ornaments, floor ornaments, projectiles, explosions, and creatures. `0x11751c(width, height, scale)` computes one scaled size. Scales: 14 and 21 (in 32nds) for creatures, a 5-entry table at `0x102e24` for ornaments, a 6-entry table at `0x102e2a` for door ornaments.

`graphic_get_bitmap_byte_count(g)` returns `((width + 15) & ~15) / 2 * height` from `g_graphic_dimensions`.

#### Freeze

- `game_freeze`: only when not frozen and the game screen is active. Set `g_game_frozen`. Restore the champion mouse pointer, disable the highlight box, clear zone 9 (or close the open chest when an inventory is open), clear zones 13 and 11, and set the default mouse pointer.
- `game_unfreeze`: only when frozen and the game screen is active. When the party sleeps, redraw the sleep screen (`0x1210e8`, `draw_viewport(0)`) and stay frozen. Otherwise clear the flag, redraw the magic caster and action area, then the viewport or the open inventory, then refresh the mouse pointer.

#### Entrance

`process_entrance` (ReDMCSB `F0441_STARTEND_ProcessEntrance`):

1. Load graphic 4 (entrance) and graphic 5 (credits) compressed into memory. Load sound samples 3 (door) and 1 (button) when they are not loaded.
2. Allocate two bitmaps with the size of zone `0x1ad`: the background (`g_entrance_background_bitmap`) and the work bitmap (`g_entrance_work_bitmap`). Load the left and right door bitmaps (graphics 2 and 3) and the button graphic `0xb`.
3. Install the entrance input tables (`0x10318c`, `0x10393c`). Loop: `entrance_draw`, show the pointer, set `g_new_game` to 99, process keys and commands until `g_new_game` changes. Repeat while it is `0xcb` (credits).
4. Commands from the mouse table: 200 = enter (new game), `0xc9` = enter and use `DungeonB~.dat` (`g_use_dungeon_b`), `0xca` = resume (load), `0xcb` = credits.
5. Draw the pressed button (zone `0x1b3` for enter, `0x1b4` for resume), play sample 1, wait 20 frames. For a new game call `entrance_open_doors`.
6. Free the bitmaps and the loaded graphics.

`entrance_draw`: fade to black, expand graphic 4 into screen zone 2, draw the dungeon behind the doors (`entrance_draw_dungeon_view`), copy the entrance into the background bitmap at the work box, draw both doors closed, fade in to `g_entrance_palette` (`0x100a66`).

`entrance_draw_dungeon_view` builds a temporary 5×5 map on the stack. All squares are walls except column 2 and square (1, 2), which are corridors. It sets `g_party_map_index` to 255 and calls `draw_dungeon_view(2, 2, 0)` (facing south).

`entrance_open_doors`: per step, the left door moves left and the right door moves right by the value at `0x102986` pixels. Each step: copy the background into the work bitmap, blit the viewport, blit both door bitmaps with their remaining width, wait until 2 frames passed, and blit the work bitmap to the screen (`entrance_blit_work_bitmap`). Play sample 3 every 40 frames. Stop when both doors are fully open.

`entrance_show_credits`: expand graphic 5 to the screen with `g_credits_palette` (`0x100a46`). Wait 1800 frames (36 s) or until input. Set `g_new_game` to `0xcb`.

#### Endgame

`endgame(party_dead)` (ReDMCSB `F0444_STARTEND_Endgame`). `kaos_main` calls it with `g_party_dead` after `game_loop`. `save_game` calls it with 0 (quit). `fuse_sequence` calls it with 1 and `g_game_won` set.

1. Set the default pointer and hide it. Clear the input tables.
2. When the party died and the game is not won: play sound 7 and wait 240 frames.
3. `ensure_master_disk(0)`. Load "THE END" (graphic 6) when `party_dead`, the champion frame (graphic `0x15a`) when the game is won, and the credits (graphic 5). Set `g_game_screen_inactive`.
4. Game won: on a color-12 screen, for each champion draw the frame (zone `0x19c + i`), the portrait (zone `0x1a0 + i`), the name at (87, 48i + 14), the title after the name (one extra space unless the title starts with `,`, `;`, or `-`), and for each of the 4 base skills with level > 1 the line "<level name> <skill name>" at x = 105. Wait for input.
5. Loop: draw "THE END" in zone 5 with a palette whose color 15 is white. The first time wait 300 frames. When `g_saved_game_exists`, draw two buttons with `endgame_draw_button`: "RESTART THIS GAME" (zone `0x1b5`) and "QUIT" (zone `0x1b6`). Wait up to 900 frames for a click. Command `0xd7` sets `g_restart_requested`; command `0xd8` calls `quit_game`. On restart: free the graphics, set `g_new_game` to 0, call `load_game`. When it succeeds, call `start_game` and return to the main loop.
6. Show the credits until input. Quit when no saved game exists or when `party_dead` is 0. Otherwise repeat step 5 without the 300-frame wait.

`endgame_print_text` maps `A`–`Z` to codes 1–26 (the endgame font) and prints on color 12. `endgame_draw_button` draws a box of color 1 with a color-0 inner box, prints the text, and stores the box in the 12-byte mouse input entry.

#### Fuse sequence

`fuse_sequence` (ReDMCSB `F0446_STARTEND_FuseSequence`) runs when the party fuses Lord Chaos. `fuse_sequence_update` runs one tick: `timeline_process`, `draw_dungeon_view`, `sound_play_pending`, `g_game_time++`.

1. Set `g_game_won`. Close the inventory. Set magical light to 200 and the three party shields (`0x101ed4`, `0x101ed8`–`0x101edc`) to 100. Redraw all champions.
2. The group in front of the party gets health 10000, all cells, and faces the party.
3. Remove fluxcage explosions (type 50) on the party square and the Lord Chaos square.
4. Create 6 fireball explosions (`0xff80`, power 55 to 255 step 40) and 6 harm-non-material explosions (`0xff83`) on the Lord Chaos square. Play sound 14. Flash the group between creature types 23 (Lord Chaos) and 25 (Lord Order) with sound 14. Then set type 26 (Grey Lord). Set the "do not draw fluxcages" flag (`0x103110`).
5. Delete all other groups on the map.
6. Collect the text things on square (0, 0) of the map. Print them in the message area in the order of their first letter (`A`, `B`, ...), 780 frames each.
7. Final explosions, wait 600 frames, clear `g_saved_game_exists`, `endgame(1)`.

#### Quit

`quit_game` (`0x12546a`) is the real entry. It hides the mouse pointer until the hide count is at least 1, closes `Graphics.DAT` twice, calls `system_shutdown`, and calls `rt_longjmp(0x104032, 1)`. `0x125470` is only the loop head inside `quit_game`. Ghidra made a second function there. No code calls it.

### Load and save

#### Files

| Global | File |
|---|---|
| `g_dungeon_file_name` (`0x101d8c`) | `DungeonMaster:Dungeon~.dat` |
| `g_dungeon_ftl_file_name` (`0x101dac`) | `DungeonMaster:Dungeon~.FTL` |
| `g_dungeon_b_file_name` (`0x101dcc`) | `DungeonMaster:DungeonB~.dat` |
| `g_save_file_name` (`0x101de4`) | `DF0:DMGAME~.DAT` |
| `g_save_backup_file_name` (`0x101df8`) | `DF0:DMGAME~.BAK` |

`Dungeon~.FTL` is not a module. `load_game` opens it as a new-game dungeon when the save disk has no `DMGAME~.DAT` and no `DMGAME~.BAK` (and no restart is in progress). It is read like `Dungeon.DAT`.

#### Read and write layer

- `save_file_read(buffer, n)` reads from `g_load_file`. When `g_load_from_memory` is set, it copies from `g_load_memory_pointer` and fails when fewer than `n` bytes remain (`g_load_memory_remaining`). `n = 0` returns true.
- `save_file_write(buffer, n)` writes to `g_load_file`.
- `save_read_summed_block` and `save_write_summed_block` read or write a block and add the unsigned sum of its bytes to a 16-bit sum.

#### Block cipher

`savefile_xor_block(words, key, count)` encrypts and decrypts in place (XOR is symmetric). `save_block_checksum` computes the same sum without changes.

```c
sum = key;
for (i = count; i != 0; i--) { old = *w; *w ^= key; sum += *w + old; key += i; w++; }
```

The checksum is the same for the plain and the encrypted data, because it adds both. `save_write_checked_block` computes the checksum, encrypts, writes, and decrypts again. `save_read_checked_block` reads, decrypts, and compares the checksum.

#### Save header (512 bytes)

`save_write_header` and `save_read_header` handle it. Offsets are bytes.

| Offset | Size | Content |
|---|---|---|
| `0x000` | 256 | 32 groups of 4 random words `w0..w3`. The chain `acc = w3 ^ ((w1 ^ (w0 + acc)) − w2)`, start 0, gives the sum of the 128 plain words at `0x100`. The writer picks `w3` of the last group to make this true. |
| `0x100` | 256 | Payload, encrypted with `savefile_xor_block`, key = word at `0x014`, 128 words. |
| `0x12a` | 1 | `g_save_header_byte_12a` (default 1). |
| `0x12b` | 1 | Platform. Written as 5. A load fails when it is 1. Stored in `g_save_platform`. |
| `0x130` | 2 | 1 (the "save" choice). Not read. |
| `0x132` | 4 | `g_game_id`. A random value at new game. A restart fails with "THAT'S NOT THE SAME GAME" when it differs. |
| `0x136` | 32 | 16 random keys. Keys 0–4 encrypt the five blocks. |
| `0x156` | 10 | 5 checksums of the five blocks. |
| `0x176` | 2 | `g_save_format`. Must be 3. |
| `0x178` | 2 | `g_dungeon_id` (10 for the DM dungeon; from the compressed dungeon header). |
| `0x17a` | 134 | `g_save_header_extra` (`0x10406a`). Copied unchanged. Use not traced. |

#### Save file layout

1. Header (512 bytes).
2. Five encrypted blocks with keys and checksums from the header:
   1. Global state, 128 bytes: game time (long, +0), random seed (long, +4), leader hand object (+8), champion count (+10), party X (+12), Y (+14), direction (+16), map index (+18), leader index (+20), magic caster index (+22), `0x103180` (+24), `0x103182` (+26), event max count (+28), active group count (+30), `0x10314e` (long, +32), `0x103152` (long, +36), disabled movement ticks (+40), projectile disabled movement ticks (+42), last projectile direction (+44), max active group count (+46). The rest is 0.
   2. Active groups: `max_active_group_count × 16` bytes.
   3. Champions and party: `0x580` bytes = 4 × `0x140` champion records + 128 bytes from `0x101ed4`.
   4. Timeline events: `event_max_count × 10` bytes.
   5. Timeline index: `event_max_count × 2` bytes.
3. Four champion portrait bitmaps, not encrypted. Size per bitmap = `((w + 15) & ~15) / 2 × h` with `w`, `h` at `0x102958`/`0x10295a`. The pointers are at champion offset `0x114`.
4. The dungeon in `Dungeon.DAT` format without compression: header (44), maps (count × 16), column first-thing indices, square first things, text data, 16 thing arrays, raw map data. Then a word: the byte sum of all dungeon parts.

`save_game` removes the weight of the leader hand object from the leader load (champion offset `0x110`) for the time of the save. It calls `0x112f5c` before the first write and `0x112ffe` at the end (champion range).

#### `save_game`

1. `game_freeze`, `timeline_reset`. Allocate a 512-byte header.
2. Show "PUT THE GAME SAVE DISK IN ~". Choices: when `g_save_dialog_format_choice` (`0x104060`, value 1) is set: SAVE, QUIT, FORMAT FLOPPY, CANCEL. Else: CANCEL, SAVE, QUIT.
3. QUIT: when the last save and the last load (`g_last_save_game_time`, `g_last_load_game_time`) are both more than 100 ticks old, ask "THE GAME IS NOT SAVED, IS IT OK TO QUIT?". Then clear `g_saved_game_exists` and call `endgame(0)`.
4. FORMAT FLOPPY: `format_save_disk_dialog`.
5. SAVE: check the disk with `is_save_disk_inserted(0)` and show the error ("CAN'T MODIFY DUNGEON MASTER DISK!", "THAT DISK IS UNREADABLE", "THAT DISK IS WRITE-PROTECTED!", "THERE IS NO DISK IN ~!"). Show "SAVING GAME . . .". Delete `DMGAME~.BAK`, rename `DMGAME~.DAT` to `.BAK`, write a new `DMGAME~.DAT`. On error show "UNABLE TO SAVE GAME!". On success set `g_saved_game_exists` and `g_last_save_game_time`, and show the dialog again.
6. At the end: `ensure_master_disk(0)`, `game_unfreeze`.

#### `load_game`

Returns 1 on success, −1 on cancel (restart path).

1. Set defaults: platform 5, byte `0x12a` = 1, format 3, dungeon ID 10. Allocate 4 portrait bitmaps (or reuse them on restart).
2. Loaded game: show "PUT THE GAME SAVE DISK IN ~" with LOAD SAVED GAME and CANCEL, unless the save disk is already in the drive. Open `DMGAME~.DAT`, then `DMGAME~.BAK`, then `Dungeon~.FTL` (new game). Show "CAN'T FIND SAVED GAME!" when none opens. Read and check the header. Read the five blocks in order. Raise the max active group count to at least 110. Read the portraits. Then `load_dungeon`. When the backup was used, rename it back to `DMGAME~.DAT`. Show "LOADING GAME . . .".
3. New game: `ensure_master_disk(0)`. Open `DungeonB~.dat` when `g_use_dungeon_b`, else `Dungeon~.dat`. When this fails: `fatal_error(0x32)`, wait 180 frames, `quit_game`. Clear the champion count and the hand object. Set `g_game_id` from two random words. Then `load_dungeon`.
4. On error show "THE GAME IS DAMAGED!" and quit (or return −1 on restart).

#### `load_dungeon` and decompression

1. New game only: read 8 bytes. When the first word is `0x8104`, the file is compressed. Offset 2 is the long decompressed size. Offset 6 is the dungeon ID. Read the rest of the file and call `decompress_dungeon(data, buffer, size)`. Set `g_load_from_memory`. Else seek back to 0.
2. Read the parts with `save_read_summed_block`. Allocate them only when no restart is in progress (`g_restart_requested` = 0). The parts, the new-game extras (300 free square-first-thing entries, extra thing records per type from the table at `0x101058`, capped at 1024, 768 for type 15), and the event max count (100 + counts of type 4 and types 14–15) are as in the module map.
3. Read the checksum word. Accept the dungeon when the word is missing or equal to the byte sum.
4. Build the column pointer table `g_dungeon_map_data`: one pointer per map, then one pointer per column. Column `c` of map `m` starts at `raw + map.offset + c × height`.
5. New game: `timeline_init`, `group_init_active_groups`.

`decompress_dungeon` decodes a bit stream (MSB first, read in big-endian words). The first 20 bytes of the data after the 8-byte header are two tables: `t4` (4 bytes) and `t16` (16 bytes). The stream starts at byte 20. Codes:

- `0` + 2 bits `i`: output `t4[i]`.
- `10` + 4 bits `i`: output `t16[i]`.
- `11` + 8 bits: output the literal byte.

Stop after `size` output bytes. `decompress_get_bits` is the internal bit reader (register interface, not a C function).

#### Disk helpers

- `build_save_prompt(dest, text, kind)` replaces `~` in the text with a drive name. It uses the name at `0x100106` when the flag at `0x100104` is set or `kind` is 1, else the name at `0x10010a`.
- `ensure_master_disk(force)` returns 1 at once when the master disk is present and `force` is not 2. Otherwise it freezes the game and shows "PUT THE DUNGEON MASTER DISK IN ~" with OK until the disk is present (next prompts add "THAT'S NOT THE MASTER DISK!"). It returns 0 when it showed a prompt. The old name `check_save_disk` was wrong.
- `format_save_disk_dialog`: "PUT A BLANK DISK IN ~" with FORMAT FLOPPY and CANCEL. A save disk asks "FORMAT DISK ANYWAY?". Shows "FORMATTING DISK . . ." and calls `format_save_disk`. On error "UNABLE TO FORMAT DISK!".

#### Other globals

- `g_saved_game_exists` (`0x10415a`): 1 after a save or a load. The endgame offers a restart only when it is set.
- `g_restart_requested` (`0x10415c`): set by command `0xd7`. While it is set, `load_game` and `load_dungeon` reuse the existing buffers.
- `g_game_frozen` (`0x10415e`).
- `g_game_won` (`0x1001e2`).
