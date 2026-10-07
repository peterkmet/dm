# KAOS ranges 4–6: symbols and subsystem notes

This page covers the KAOS functions in partition ranges 4, 5, and 6 of the [module map](../kaos-map.md): `0x10a432`–`0x116a42`. The symbol list is [range-4-6.csv](range-4-6.csv). It has all 246 functions of the ranges and 143 globals that these functions own.

Names follow the ReDMCSB function order where the code matches. All offsets are in bytes. All words are big-endian. "Word" means 16 bits, "long" means 32 bits.

## Summary

| Item | Count |
|---|---|
| Functions in ranges 4–6 | 246 |
| Existing names confirmed | 211 |
| Existing names corrected | 7 |
| Functions named for the first time | 28 |
| Globals named | 143 (3 of them are tables in the code section) |

Corrected names:

| Address | Old name | New name | Reason |
|---|---|---|---|
| `0x10af2e` | `get_unused_thing` | `get_discarded_thing` | Scans maps and deletes a thing to free a slot (ReDMCSB F0165). |
| `0x10b1a6` | `get_discarded_thing` | `get_unused_thing` | Returns a free slot and calls `get_discarded_thing` when none is free (F0166). The two names were swapped. |
| `0x1143f8` | `heap_find_pool` | `heap_claim_largest_region` | Returns the largest unclaimed region and marks it as claimed. |
| `0x114b0c` | `heap_set_reserve` | `memory_select_feature_set` | Tries feature sets until the graphics of every map fit. |
| `0x115728` | `derived_cache_reset_usage` | `cache_reset_usage` | Works on all cached bitmaps, not only derived ones. |
| `0x115768` | `derived_cache_add` | `cache_lru_insert` | Inserts a block into the LRU list. |
| `0x115858` | `derived_cache_lookup` | `cache_touch` | Increments the usage count and moves the block in the LRU list. |

Global correction: `g_creature_info` starts at `0x10192e`, not `0x101930`. The table entry is `0x1a` bytes and the attributes word is at offset 2. The code reads `0x10192e + type * 0x1a` (for example in `group_process_events_29_to_41`). Remove the `0x101930` entry when you merge.

## Method

- Read every function in `decomp/KAOS.c` for the three ranges.
- Read the disassembly (capstone) for jump-table calls, register helpers, and the clipping tail of `get_zone`.
- Built a call graph from the disassembly to find callers.
- Checked `expand_graphic` in the emulator (`src/dmtools/emu.py`) against a Python model on every bitmap in `Graphics.DAT`. Result: 687 of 688 bitmap-sized items decode to identical bytes. The other item (graphic 672) is not a bitmap. Its "dimensions" are a size field.

## Dungeon data access (range 4, DUNGEON.C)

### Thing word

A thing is a word:

| Bits | Meaning |
|---|---|
| 0–9 | Index into the record array of the type. |
| 10–13 | Thing type. |
| 14–15 | Cell (0–3) on the square. |

`0xfffe` ends a list. `0xffff` means "no thing". `dungeon_get_thing_data(thing)` returns `g_thing_data[type] + g_thing_data_byte_count[type] * index`.

Thing types and record sizes:

| Type | Name | Bytes |
|---|---|---|
| 0 | Door | 4 |
| 1 | Teleporter | 6 |
| 2 | Text string | 4 |
| 3 | Sensor | 8 |
| 4 | Group | 16 |
| 5 | Weapon | 4 |
| 6 | Armour | 4 |
| 7 | Scroll | 4 |
| 8 | Potion | 4 |
| 9 | Container | 8 |
| 10 | Junk | 4 |
| 14 | Projectile | 8 |
| 15 | Explosion | 4 |

Every record starts with the next-thing word at offset 0. A free record has `0xffff` at offset 0.

Record fields that the code in these ranges uses (word at offset 2 unless stated):

| Type | Fields |
|---|---|
| Door | Bit 0: door type (index into `g_current_map_door_info`). Bit 5: vertical door. |
| Text string | Bit 0: visible. Bits 3–15: word offset into `g_dungeon_text_data`. |
| Sensor | Bits 0–6: sensor type. Bits 7–15: data. Byte 4, bits 4–7: ornament ordinal. Type 127 shows a champion portrait (data + 1 goes to `g_champion_portrait_ordinal`). |
| Group | See [Group record](#group-record). |
| Weapon, armour | Bits 0–6: type. Bit 7: important (never discarded). Bit 8: cursed. Weapon bits 10–13: charges or torch light power. Weapon bit 15: lit torch. |
| Scroll | Bits 0–9: text-string index. |
| Potion | Bits 8–14: type (20 is the empty flask). Bit 15: important. Byte 3: power. |
| Container | Offset 2: first thing of the contents. Byte 5, bits 1–2: container type. |
| Junk | Bits 0–6: type. Bit 7: important. Bits 14–15: charges. |

### Square byte

`g_current_map_data[x][y]` is one byte:

| Bits | Meaning |
|---|---|
| 5–7 | Square type: 0 wall, 1 corridor, 2 pit, 3 stairs, 4 door, 5 teleporter, 6 fake wall. |
| 4 | The square has a thing list. |
| 0–3 | Type-specific flags. Wall: random ornament allowed for each side. Pit: bit 3 open, bit 0 invisible. Stairs: bit 3 orientation, bit 2 up. Door: bits 0–2 state (0 open … 4 closed, 5 destroyed), bit 3 orientation. Teleporter: bit 3 open, bit 2 visible. Fake wall: bit 2 open, bit 0 imaginary, bit 3 random ornament allowed. |

`get_square(x, y)` returns the byte for a square on the map. For a square outside the map, it returns a wall byte. The wall gets an "ornament allowed" bit when the adjacent square on the map is a corridor or a pit: `2` above row 0, `8` below the last row, `4` left of column 0, `1` right of the last column.

### Map descriptor

`g_dungeon_maps` has 16 bytes per map:

| Offset | Size | Meaning |
|---|---|---|
| 0 | long | Raw map data offset. |
| 6 | byte | X offset of the map in the dungeon. |
| 7 | byte | Y offset of the map. |
| 8 | word | Bits 0–5: level. Bits 6–10: width − 1. Bits 11–15: height − 1. |
| 10 | byte | Bits 0–3: floor ornament count. Bits 4–7: random floor ornament count. |
| 11 | byte | Bits 0–3: wall ornament count. Bits 4–7: random wall ornament count. |
| 12 | byte | Bits 4–7: difficulty. 0 means a lit map. The value multiplies skill experience. |
| 13 | byte | Bits 0–3: door ornament count. Bits 4–7: creature type count. |
| 14 | byte | Bits 0–3: door type 0. Bits 4–7: door type 1 (index into `g_door_info`). |

The bytes after the last column of squares are the map metadata (`g_current_map_metadata`): the creature types, then the wall ornament indices, the floor ornament indices, and the door ornament indices. `set_current_map_and_party_map` copies the three ornament lists into `g_current_map_wall_ornament_indices`, `g_current_map_floor_ornament_indices`, and `g_current_map_door_ornament_indices`. It writes 0 after the wall list. That slot is the inscription ornament (`g_current_map_inscription_ornament_index` = wall ornament count).

`is_creature_allowed_on_map(group_thing, map)` searches the creature type list of the map for the creature type of the group.

### First-thing index

The square thing lists start in `g_square_first_things` (one word for each square that has bit 4 set, in column order). `g_dungeon_column_first_thing_index` has one word for each column of each map. `g_current_map_column_first_thing_index` points to the first column of the current map.

`get_square_first_thing_index(x, y)` starts at the column entry and adds 1 for each square above `y` in the column that has bit 4 set.

`link_thing_to_list(thing, list, x, y)`:

1. Set the next word of `thing` to `0xfffe`.
2. If `x < 0`, append `thing` to the list that starts at `list`.
3. If the square has no list, set bit 4. Add 1 to the first-thing index of every later column of the dungeon (`g_dungeon_column_count` columns). Insert the thing into `g_square_first_things` at the square index. The last entry of `g_square_first_things` must be `0xffff`, or the game calls `fatal_error(0x48)`.
4. Otherwise append `thing` to the end of the square list.

`unlink_thing_from_list` is the inverse. When the list becomes empty, it clears bit 4, removes the entry, writes `0xffff` to the last entry, and subtracts 1 from the later column indices.

### Free records

`get_unused_thing(type)` scans the record array of the type for a free record (`0xffff`). Type 10 (junk) keeps 3 records in reserve. The value `0x800a` asks for junk without the reserve. The function clears the record and sets the next word to `0xfffe`. When no record is free, it calls `get_discarded_thing(type)`.

`get_discarded_thing(type)` starts at the map in `g_last_discarded_thing_map_index[type]` and visits every map (the party map last). It skips squares within 5 squares of the party. It takes the first thing of the type that is not important. A group must have no possessions and must not be marked bit 10 of its word at offset 14. It deletes the thing (groups drop their possessions first) and returns the freed index.

### Text decoding

`decode_text(dst, text_thing, mode)` decodes 5-bit codes. Each word of `g_dungeon_text_data` holds three codes: bits 10–14, 5–9, 0–4.

- Mode 0: inscription. Mode 1: message (output starts with a newline). Mode 2: scroll. Bit 15 of the mode decodes an invisible text.
- Codes 0–25 are letters `A`–`Z` (inscriptions keep the raw code). 26 is space and 27 is `.` (not for inscriptions).
- Code 28 is the line separator: `0x80` for inscriptions, a space for messages, a newline for scrolls.
- Code 29 takes the next code as an index into `g_escape_replacement_chars` (2 bytes each).
- Code 30 takes the next code as an index into `g_inscription_escape_strings` (inscriptions) or `g_message_escape_strings` (8 bytes each).
- Code 31 ends the text. Inscriptions end with `0x81`, other texts with NUL.

### Random ornaments

```c
index = ((header_word0 + ((seed1 * 31417) >> 1) + seed2 * 11) >> 2) % modulo;  /* 16-bit math */
ordinal = allowed && index < count ? index + 1 : 0;
/* seed1 = y + x * 32 + 2000; seed2 = map_height + map_width + map_index * 64 + 3000 */
```

`header_word0` is the first word of `g_dungeon_header`. For a wall side, `y` is multiplied by `(side + 1) * (y + 1)` before the call. See `set_square_aspect_random_wall_ornaments`.

### Square aspect

`set_square_aspect(aspect, direction, x, y)` fills 7 words for the dungeon view: `[0]` element type, `[1]` square byte, `[2]` first thing that is not a door, teleporter, text, or sensor, `[3]`–`[6]` the four wall ornament ordinals (or type-specific values), `[5]` also the floor ornament ordinal. Bit 15 of `[5]` marks footprints (the party scent on the square is between `g_party.first_scent_index` and `last_scent_index`). Element types 16/17 are a door seen from the front or the side. 18/19 are stairs seen from the front or the side.

## Creature groups (range 4, GROUP.C)

### Group record

| Offset | Size | Meaning |
|---|---|---|
| 0 | word | Next thing. |
| 2 | word | First possession thing. |
| 4 | byte | Creature type. |
| 5 | byte | Cells (2 bits for each creature, `0xff` for a centered creature). On the party map: index into `g_active_groups`. |
| 6 | 4 words | Health of each creature. |
| 14 | word | Bits 0–3: behavior (0 wander, 2/3 react, 5 flee, 6 attack, 7 approach). Bits 5–6: creature count − 1. Bits 8–9: direction. Bit 10: do not discard. |

`group_get_cells` and `group_get_directions` read the active group when the map is the party map. Otherwise they read the record. A group off the party map has one direction for all creatures. `g_group_directions_packed[d]` expands it to 2 bits for each creature.

### Active group

`g_active_groups` has `g_max_active_group_count` entries of 16 bytes (110 for a new game):

| Offset | Size | Meaning |
|---|---|---|
| 0 | word | Group record index, or `-1` for a free entry. |
| 2 | byte | Directions (2 bits for each creature). |
| 3 | byte | Saved cells of the group record. |
| 4 | byte | Low byte of the game time at the last move. |
| 5 | byte | Delay before the group stops fleeing. |
| 6, 7 | bytes | Target X, Y (usually the last known party position). |
| 8, 9 | bytes | Prior X, Y. |
| 10, 11 | bytes | Home X, Y. |
| 12 | 4 bytes | Aspect of each creature: bit 7 attacking, bit 6 flipped, bits 3–5 vertical offset, bits 0–2 horizontal offset. |

### Creature info

`g_creature_info` (`0x10192e`) has `0x1a` bytes for each creature type:

| Offset | Size | Meaning |
|---|---|---|
| 0 | byte | Aspect index. |
| 1 | byte | Attack sound ordinal. |
| 2 | word | Attributes: bits 0–1 size (1 means two creatures share a half square), bit 2 side attack, bit 3 prefer back row, bit 4 attack any champion, bits 7–8 height, bit 9 drops fixed possessions, bit 11 sees invisible, bit 12 night vision, bit 13 archenemy. |
| 4 | word | Graphic info: bit 2 flip when not attacking, bit 9 flip when attacking, bit 10 flip during attack, bits 12–13 horizontal offset range, bits 14–15 vertical offset range. |
| 6 | byte | Movement ticks. |
| 7 | byte | Attack ticks. |
| 8 | byte | Defense (255 is invulnerable). |
| 9 | byte | Base health. |
| 10 | byte | Attack. |
| 11 | byte | Poison attack. |
| 12 | byte | Dexterity. |
| 14 | word | Ranges: bits 0–3 sight, bits 8–11 smell, bits 12–15 attack. |
| 16 | word | Properties: bits 4–7 fear resistance, bits 8–11 experience, bits 12–15 wariness. |
| 18 | word | Resistances: bits 8–11 poison. |
| 20 | word | Animation ticks: bits 4–7 non-attack, bits 8–11 attack. |

### Group AI

`group_process_events_29_to_41` follows ReDMCSB F0209. A port should use the ReDMCSB structure and these KAOS details:

- Off the party map, a group only handles event types 32, 33, 37, and 38. It moves in a random direction and adds a new event 37 later by `max(16 * |map − party map|, 2 * movement ticks)`.
- The function stores the current group in `g_current_group_map_x`, `g_current_group_map_y`, `g_current_group_thing`, `g_current_group_distance_to_party`, and the two directions to the party. The movement test results go to `g_group_movement_blocked_by_*`, `g_flux_cages`, and `g_flux_cage_count`.
- `group_get_distance_between_unblocked_squares` walks a line from the group to the party with fixed point slope (`* 64`). It calls `group_is_view_party_blocked` or `group_is_smell_party_blocked` for each square.
- `group_is_creature_attacking` has a jump table at `0x10d75a` for creature types below 25 that cast spells. Ghidra cannot recover it. Read the disassembly when you port that switch.

Several callees are in range 7 and have no names yet: `0x11cefe` (distance), `0x11cf42` (creature sees party in a direction), `0x11cfba` (primary direction to a square), `0x11d10a` (creature attack damage), `0x11c4e8` (fluxcage/move test).

## String table (range 4)

`load_string_table` reads graphic 700 raw. It is a block of NUL-terminated strings. The function counts the strings and builds `g_strings` (a pointer array). `get_string(i)` returns `g_strings[i]`, or `g_empty_string` for an invalid index. `graphics_get_language_index` replaces graphic 700 with 737 (French) or 747 (German).

## Fill helpers and inventory panel (range 5)

- `fill_viewport_zone(zone)` always uses color 12.
- `fill_screen_zone_pattern` fills with a pattern (used to dim the screen behind the inventory and over an invisible champion portrait).
- `flip_bitmap_horizontal_raw` reverses the byte order of each row and maps each byte through `g_byte_bit_reverse`.
- `flip_bitmap_vertical_raw` (`0x113b6e`) swaps rows plane by plane.
- `set_dungeon_view_palette`: if the map difficulty is 0, the palette index is 0. Otherwise it collects the light power of torches in both hands of all champions, sorts the 8 values, and adds `g_light_power_to_light_amount[power] >> (6 − i)` for the 5 strongest (the shift decreases for each torch). It adds `g_party.magical_light_amount`. The index is the first entry in `g_palette_light_thresholds` that is not greater than the sum, or 5 when the sum is below 1.

`g_panel_content` values: 1 food/water/poison, 2 skills and statistics, 3 object, 5 scroll, 6 chest, 7 resurrect/reincarnate, 8 rename.

## Champion (ranges 5 and 6, CHAMPION.C)

### Champion record

`g_champions` holds 4 records of `0x140` bytes:

| Offset | Size | Meaning |
|---|---|---|
| `0x00` | 8 bytes | Name (NUL-terminated). |
| `0x08` | 20 bytes | Title. |
| `0x1c` | byte | Direction. |
| `0x1d` | byte | Cell in the party. |
| `0x20` | byte | Action index (255 is none). |
| `0x21` | byte | Spell symbol step. |
| `0x22` | 5 bytes | Spell symbols (string). |
| `0x28` | byte | Direction of the maximum damage received. |
| `0x29` | byte | Maximum damage received. |
| `0x2a` | byte | Poison event count. |
| `0x2c` | word | Timeline index of the enable-action event (event 11), or −1. |
| `0x2e` | word | Timeline index of the hide-damage event (event 12), or −1. |
| `0x30` | word | Attributes (redraw flags, see below). |
| `0x32` | word | Wounds: bit 0 ready hand, 1 action hand, 2 head, 3 torso, 4 legs, 5 feet. |
| `0x34` | word | Current health. |
| `0x36` | word | Maximum health. |
| `0x38` | word | Current stamina (shown divided by 10). |
| `0x3a` | word | Maximum stamina. |
| `0x3c` | word | Current mana. |
| `0x3e` | word | Maximum mana. |
| `0x40` | word | Action defense. |
| `0x42` | word | Food (−1024 to 2048). |
| `0x44` | word | Water (−1024 to 2048). |
| `0x46` | 7 × 3 bytes | Statistics: luck, strength, dexterity, wisdom, vitality, anti-magic, anti-fire. Each has maximum, current, minimum. |
| `0x5c` | 20 × 6 bytes | Skills: word temporary experience, long experience. 0–3 are the base skills (fighter, ninja, priest, wizard). 4–19 are the hidden skills. |
| `0xd4` | 30 words | Slots (ReDMCSB order, consistent with `g_slot_masks`): 0 ready hand, 1 action hand, 2 head, 3 torso, 4 legs, 5 feet, 6 pouch 2, 7 quiver line 2, 8–9 quiver line 1, 10 neck, 11 pouch 1, 12 quiver line 1 slot 1, 13–29 backpack. |
| `0x110` | word | Load (weight in 1/10 kg). |
| `0x112` | word | Shield defense (from the potion). |
| `0x114` | long | Pointer to the portrait bitmap (32 × 29). |
| `0x1e`, `0x1f`, `0x23`–`0x27`, `0x118`–`0x13f` | — | No code reference found. |

Attribute flags at `0x30`: `0x0008` disable action, `0x0010` male, `0x0080` name and title, `0x0100` statistics, `0x0200` load, `0x0400` icon, `0x0800` panel, `0x1000` status box, `0x2000` wounds, `0x4000` viewport, `0x8000` action hand. `ui_draw_champion_state` draws the parts that have a flag and then keeps only bits 0–6.

### Party record

`g_party` (`0x101ed4`) is the ReDMCSB PARTY structure:

| Offset | Size | Meaning |
|---|---|---|
| `0x00` | word | Magical light amount. |
| `0x02` | byte | Thieves eye event count. |
| `0x03` | byte | Footprints event count. |
| `0x04` | word | Shield defense. |
| `0x06` | word | Fire shield defense. |
| `0x08` | word | Spell shield defense. |
| `0x0a` | byte | Scent count. |
| `0x0b` | byte | Freeze life ticks. |
| `0x0c` | byte | First scent index (footprints). |
| `0x0d` | byte | Last scent index. |
| `0x0e` | 24 words | Scents: X bits 11–15, Y bits 6–10, map bits 0–5. |
| `0x3e` | 24 bytes | Scent strengths. |
| `0x56` | byte | Invisibility event count. |

### Rules a port must reproduce

- Skill level (`champion_get_skill_level(champion, skill | flags)`): returns 1 while the party sleeps. Experience is the long of the skill plus the temporary experience (flag `0x8000` ignores the temporary part). For a hidden skill, add the base skill `(skill − 4) / 4` and halve. Level starts at 1 and adds 1 for each halving while experience ≥ 500. Without flag `0x4000`, add object bonuses (action hand icons 27 and 28, neck icons 120, 124, 121, 122 for specific skills).
- Experience (`champion_add_skill_experience`): halve for skills 4–11 when the last creature attack is more than 150 ticks ago. Multiply by the map difficulty when it is not 0. Double for hidden skills when the last creature attack is less than 25 ticks ago. On a new level, raise statistics, maximum health, stamina, and mana, set `g_champion_skill_level_up`, and print the message.
- Maximum load: `stamina_adjusted(current_strength * 8 + 100)`. Subtract 1/8 when the champion has a wound (1/4 with a leg wound). Add 1/16 with icon 119 on the neck. Round up to a multiple of 10.
- Luck (`game_is_lucky`) changes the current luck by ±2 within its minimum and maximum each time.
- Damage pipeline: `champion_add_pending_damage_and_wounds` adds to `g_champion_pending_damage` and `g_champion_pending_wounds`. The main loop calls `champion_apply_and_draw_pending_damage_and_wounds`, which subtracts the damage, kills the champion at 0 health, draws the damage, and schedules event 12 after 5 ticks.
- `champion_apply_time_effects` runs every 64 ticks (16 while sleeping). It ages scents, regenerates mana, decrements temporary experience, consumes food and water, decrements stamina, regenerates health, and moves current statistics toward their maximum every 256 ticks (64 while sleeping).
- Candidate data: `champion_add_candidate_to_party` decodes the text on the portrait square. Line 1 is the name, lines 2–3 the title, then `M`/`F`, then 4-character values for health, stamina, and mana, then 2-character statistic values, then one character for each hidden skill. `champion_get_decoded_value` reads each character as `c − 'A'` (4 bits). A hidden skill gets `125 << (c − 'A')` experience. Each base skill is the sum of its 4 hidden skills.
- Portraits are stored interleaved in a saved game. `champion_portraits_to_interleaved` runs before `save_game` writes. `champion_portraits_from_interleaved` runs after the write and after `load_game` reads. Interleaved means: for each row, for each 16-pixel column, the 4 plane words. Both functions assume a portrait 2 words wide.

## Video driver interface (VDEO, `FTL_DRIA_Video`)

KAOS does not call code in the VDEO module. `find_video_driver` calls `FindPort("FTL_DRIA_Video")`. The result is a `0x19e`-byte block that VDEO allocates. KAOS reads and writes its fields and passes its embedded OS structures to OS calls.

Layout of the block (`g_video`):

| Offset | Content |
|---|---|
| `0x00` | `struct MsgPort` (name at `0x22`). |
| `0x32` | `struct View` for the game display. |
| `0x44` | `struct ViewPort` (320 × 200). |
| `0x6c` | `struct RasInfo`. |
| `0x78` | `struct BitMap`, 4 planes, 320 × 200. Plane pointers at `0x80`–`0x8c`. |
| `0xa0` | `struct RastPort` on that bitmap. `0xa4` is `RastPort.BitMap`, `0xb8` is `RastPort.Mask`. |
| `0x104` | Saved `GfxBase->ActiView` to restore on exit. |
| `0x108` | `struct Screen *` (Intuition screen for colors). |
| `0x110` | Display buffer 0 (16 bytes; `+0x0c` is a `struct cprlist *` for 220 copper instructions). |
| `0x120` | Display buffer 1 (copper list for 200 instructions). |
| `0x130` | Pointer to the palette fade step table (16 bytes for each step). |
| `0x134` | `0x7e40` bytes of chip memory (the bitplanes). |
| `0x138` | Pointer to a chip scratch buffer. |

Every KAOS access:

| KAOS function | Range | Access | Meaning |
|---|---|---|---|
| `find_video_driver` | 6 | `FindPort`; reads `+0xa4`; sets `g_display_buffer = +0x110` | Connects to VDEO. `g_screen_bitmap = RastPort.BitMap->Planes[0]`: the screen planes, 40 bytes for each row, 8000 bytes for each plane. |
| `select_display_buffer(n)` | 6 | Writes `View.LOFCprList` (`+0x36`) from buffer `n`; `LoadView(+0x32)` | Switches between the two copper lists. No caller exists. |
| `input_event_handler` | 1 | Compares `GfxBase->ActiView` with `+0x32`; `LoadView(+0x32)` | Accepts mouse input only while the game view is active. Restores the game view. |
| `load_colormap` | 2 | `Screen->ViewPort.ColorMap` (`+0x108`, then `+0x30`) | Copies colors into the color table. |
| `restore_display` | 2 | `SetRGB4(&Screen->ViewPort, 17..19, …)`; `MakeScreen(Screen)` | Sets the pointer sprite colors. |
| `palette_fade` | 2 | Reads the table at `*(+0x130)` | Step sizes for each color component. |
| `fill_screen_box`, `complement_screen_box` | 2 | `SetDrMd`, `SetAPen`, `Mask = 15`, `RectFill` on `+0xa0` | Fills or inverts a screen box. |
| `clear_screen(color)` | 2 | Plane pointers at `+0x80` | Clears the 4 planes with the color bits. |
| `update_mouse_pointer_sprites`, `free_mouse_pointer` | 3 | `ChangeSprite`, `MoveSprite` with `+0x44` | Mouse pointer (sprites 4–7). |
| `draw_viewport` | 7 | Waits while `GfxBase->ActiView == +0x32` | Uncertain. The loop waits for a change of the active view. |
| `blit_raw` | 7 | `*(+0x138)` into `BLTBPT`/`BLTDPT` | Scratch buffer for blits. |

A port replaces all of this with one 320 × 200 indexed framebuffer, a 16-color palette with a split (top and bottom palettes), and a sprite or software pointer.

## Graphic decompression (`expand_graphic`)

`expand_graphic(packed, bitmap)` decodes one graphic into a planar bitmap. A Python model of this description matched the original code in the emulator on every bitmap (see [Method](#method)).

### Input

1. Skip any leading words with the value `0xff81`. No graphic in the v3.60 file has one.
2. Read the width `w` and the height `h` (words).
3. The command stream follows.

### Output

- When `bitmap` is NULL, the target is the screen (`g_screen_bitmap`, 40 bytes for each row, plane size 8000), at X = 0, Y = 0.
- Otherwise the target is a bitmap: `row_bytes = ((w + 15) & ~15) / 8`, `plane_size = row_bytes * h`. Plane `k` starts at `bitmap + k * plane_size`. Pixel `x` is bit `7 − (x & 7)` of byte `x / 8` of the row. Color bit `k` goes to plane `k`.
- The function does not write the 4-byte header. `decode_cached_graphic` writes it from `g_graphic_dimensions`.

### Commands

The decoder keeps the position (`x`, `y`). Each pixel advances `x`. At `x == w`, `x` becomes 0 and `y` increases. Padding pixels after `w` are not written. Decoding stops when `y == h` at a command boundary. Let `c = cmd & 0x0f`.

| Command byte | Meaning |
|---|---|
| `0nnn cccc` | Write `nnn + 1` pixels (1–8) of color `c`. |
| `1L00 cccc`, count | Write `count + 1` pixels of color `c`. The count is 1 byte, or 2 bytes (big-endian) when `L` is 1. |
| `1L10 cccc` | Skip pixels (leave the target unchanged). `n = c + (L ? 16 : 0)`. `n` 0–28: skip `n + 1`. `n` 29: skip `next_byte + 1`. `n` 30: skip `256 + next_byte + 1`. `n` 31: skip `next_word + 1`. A skip can cross several rows. |
| `1L01 cccc`, count | Raw pixels. Count as above, plus 1. When the count is odd, the first pixel has color `c`. Then `count / 2` bytes follow, each with 2 pixels (high nibble first). The count must be at least 2. |
| `1L11 cccc`, count | Copy `count + 1` pixels from the row above (same `x`), then write 1 pixel of color `c`. |

Rows wrap inside every run. A run that passes the last row writes past the end of plane 0 in the original. A port must clip.

The four register helpers (`expand_fill_run`, `expand_copy_run_from_previous_row`, `expand_put_pixel`, `expand_next_row_if_full`) use word masks from `g_expand_left_masks` and the following right masks. `g_expand_color_planes[c]` holds the 4 plane words of color `c` in the order plane 1, plane 0, plane 3, plane 2. They do not use the C calling convention. A port does not need them.

## Graphics.DAT (range 6)

### Header and offsets

`graphics_dat_read_header` reads the header described in the [module map](../kaos-map.md#graphicsdat). The v3.60 file is format 1 with 749 items. Data starts at `4 + count * 8` (format 1) or `2 + count * 4` (format 0). `graphics_dat_get_graphic_offset(i)` adds the compressed sizes of the items before `i`. Items are stored as they are used in memory. The "compressed" and "decompressed" sizes are equal for every item in the v3.60 file.

`graphics_dat_read_graphic(i, dst)`:

1. Replace `i` with the language version (`graphics_get_language_index`). `g_language_graphics` has 10 rows of English, French, and German indices (for example 700, 737, 747).
2. Read the item through a 1 KB buffer (`g_graphics_dat_buffer`) aligned to 1 KB file blocks. The buffer keeps the last block.
3. A seek or read error calls `fatal_error(0x2e)`.

### Graphic index flags

| Flag | Where | Meaning |
|---|---|---|
| `0x8000` | `get_native_bitmap_or_graphic` | Return the raw loaded item (`g_graphic_cache[i & 0x7fff]`), not a decoded bitmap. |
| `0x8000` | `load_graphic_into_bitmap` | Read the raw item into `dst`. Do not decode. |
| `0x4000` | `load_graphic_into_bitmap` | Do not write the bitmap header. |
| `0x4000` | `graphics_dat_load_graphics` list | The item must be in chip memory (sound samples). |
| `0x2000` | `graphics_dat_load_graphics` | Internal: the item goes to the chip area. |

`load_graphic_into_bitmap` without `0x8000` reads the packed item into `g_viewport_bitmap` as scratch space, then decodes it into `dst`. It sets the global at `0x1001ca` so that the inventory redraws the viewport.

### Loading the graphics of a map

`graphics_dat_load_graphics(list, count)` loads the packed items that a map needs:

1. Sort the list by `index & 0x1fff` (`sort_words`, heapsort) and remove duplicates.
2. Measure the items that are already loaded. Keep them where they are when possible.
3. Put new items into the fast area (`g_fast_gfx_area`) until it is full, then into the chip area (`g_chip_gfx_area`). Items with flag `0x4000` always go to the chip area.
4. Free items that the map does not need (`gfx_area_keep_or_evict`). Compact both areas (`gfx_area_compact`).
5. In dry-run mode (`g_graphics_load_dry_run`), stop after the measurement. `g_graphics_spare_memory` then holds the free memory. `memory_select_feature_set` uses this mode to test every map.
6. Read the new items from the file. Each item in an area ends with a word: the graphic index, or `−size` when the space is free.

`g_graphic_cache[i]` (`0x102566`) points to the loaded packed item. Despite the name, it is not the bitmap cache.

### Bitmap cache

Decoded bitmaps live in the chip arena between `g_cache_start` and `g_cache_end`.

Block layout (`CacheBlock`):

| Offset | Size | Meaning |
|---|---|---|
| 0 | long | Size. Negative when used, positive when free. The same value is in the last long of the block. |
| 4 | word | Usage count in the current game tick. |
| 6 | word | Previous block index in the LRU list (`0xffff` for none). |
| 8 | word | Next block index. |
| 10 | word | Owner: graphic index, or derived index with bit 15 set. |
| 12 | word | Width. |
| 14 | word | Height. |
| 16 | — | Bitmap pixels (the bitmap pointer). |

A free block uses offset 4 and 8 as previous and next pointers of `g_cache_free_list` (sorted by size, largest first).

- `g_cache_blocks` maps a block index (0–`0x605`) to the block. `g_graphic_cache_block` and `g_derived_cache_block` map an owner to a block index.
- `cache_alloc_block(size)` evicts from `g_cache_lru_first` until `g_cache_free_bytes ≥ size` (`cache_make_room`). It takes space at `g_cache_top`, or an exact or larger free block (it splits a block when the rest is at least `0x1c` bytes), or compacts the cache.
- `cache_free_graphic(owner)` releases a block and merges it with free neighbors (boundary tags).
- `cache_touch(index)` counts a use in the current tick and keeps the LRU list ordered by usage count. `cache_reset_usage` clears the counts when `g_game_time` changes.
- `get_native_bitmap_or_graphic(i)` returns a cached bitmap. On a miss, it allocates `rowbytes * 4 * h + 0x14` bytes and decodes the packed item.
- Derived bitmaps (scaled or recolored copies, 997 indices) use `is_derived_bitmap_in_cache` (allocates on a miss and returns 0, so the caller draws it), `get_derived_bitmap`, and `add_derived_bitmap_to_cache`. `g_derived_bitmap_byte_counts` gives the size of each derived bitmap.

A port can replace this with a plain LRU cache. Keep the behavior that a derived bitmap is drawn by the caller when it is missing.

## Heap (range 6)

`system_init` passes memory regions to `heap_add_region(start, size, flags)`. Chip regions have flags `0x7f8`. The fast region has flags `0x700`. A request with flags `f` can use a region when `(region_flags & f) == f`, so any of the bits `0x0f8` requires chip memory.

Region header (`HeapRegion`, `0x1c` bytes, before the region memory):

| Offset | Size | Meaning |
|---|---|---|
| 0 | long | Next region. |
| 4 | long | Size after the header. |
| 8 | long | Top pointer (allocation moves down). |
| 12 | long | Free bytes. |
| 16 | word | Flags. |
| 18 | word | Claimed (used as an arena). |
| 20 | long | Saved top. |
| 24 | long | Saved free bytes. |

`heap_add_region(NULL, 0, 0)` finishes the setup. It claims the largest chip region as the main arena and the largest `0x700` region as the fast arena. It allocates the 1 KB file buffer, the cache block table, and the derived-bitmap tables.

`mem_alloc(size, mode, flags)` rounds the size up to even. Then:

| Mode | Source | Release |
|---|---|---|
| 1 (permanent) | Best-fit unclaimed region (`heap_find_best_fit`, smallest sufficient free space). Else the fast arena when `(flags & 0x700) == flags`. Else the top of the main arena. | Never. |
| 0 (stack) | Top of the main arena (`g_main_arena_top` moves down). | `mem_free_top(size)`, last in first out. |
| 2 (temporary) | Bottom of the free space of the main arena (`g_main_arena_top − g_main_arena_free`). | `mem_free_temp(size)`. |

The main arena calls `fatal_error(0x28)` when it is full. `alloc_bitmap(w, h, mode)` allocates `rowbytes * 4 * h + 4` bytes with flags 8 and writes the header. `free_bitmap` assumes mode 0.

`memory_select_feature_set(sets)` reads `g_memory_feature_sets`: lists of feature numbers (0–5) separated by `0xfe` and ended by `0xff`. `0xfd n` reserves `n` KB. For each set, it restores the heap marks, enables the features (globals `0x1001d4`–`0x1001d9`), places the bitmap cache and the graphics areas, and runs the loader in dry-run mode for every map. It stops at the first set where every map fits. The features appear in the memory debug text as `G W F S A M`.

`memory_debug_key` prints memory statistics after the player types `_memdbg`.

## Layout zones (range 6)

`init_layout` loads graphic 696 raw and parses it with `parse_layout`. The data format:

1. Word `0xfc0d`.
2. Word `range_count`.
3. `range_count` pairs of words: first and last zone number.
4. For each range, `last − first + 1` zone records of 4 words: `type`, `parent`, `a`, `b`.

`parse_layout` builds a linked list of chunks (`LayoutChunk`: next, first, last, records). `layout_find_zone(list, n)` returns the record of zone `n`, or NULL. Zone 0 does not exist.

Zone types:

| Type | Meaning |
|---|---|
| 0–8 | A point (`a`, `b`) relative to the parent, with an alignment. |
| 1 as a parent | A translation: add (`a`, `b`) to the position and to the clip rectangle. |
| 9 | A size record: `a` is the width, `b` is the height. |
| 10–18 | A point inside another zone: the parent is a point record whose parent is a size record. Alignment `type − 10` selects the reference point of that rectangle. (`a`, `b`) is an offset. |

Alignment codes for a box of size (`w`, `h`) at point (`x`, `y`): 0 center (`x − (w+1)/2`, `y − (h+1)/2`), 1 top left, 2 top right (`x + 1 − w`), 3 bottom right, 4 bottom left (`y + 1 − h`), 5 top center, 6 right center, 7 bottom center, 8 left center. The reference points for types 10–18 use the same codes on the rectangle (for example 12 is (`x + w − 1`, `y`)).

`get_zone(bitmap, box, zone, &w, &h)`:

1. Start with the zone record and a clip rectangle (0, 0, 20000, 20000).
2. Walk up the parent chain. Add each point and translation to the position. Intersect the clip rectangle with every size rectangle on the way.
3. Take the item size from `*w` and `*h`, or from the bitmap header when they are 0. With bit 15 of `zone`, add `*w` and `*h` to the position and take the size from the bitmap.
4. Place the box at the position with the alignment of the zone.
5. Clip the box to the clip rectangle. Write the clipped amounts to `*w` and `*h` (the source offset). Return NULL when the box is empty.

`get_zone_box(zone, box)` uses the size record of the zone's parent as the box size. `get_zone_box_clipped(zone, box, wp, hp)` scales that size by `wp / 10000` and `hp / 10000` (10000 means full size). `get_zone_size(zone, &x, &y)` returns the position of a 1 × 1 box.

Negative bitmap indices refer to `g_layout_bitmaps[−1 − i]` (14 bytes each): byte 0 kind (1 graphic index, 2 bitmap pointer), words at 2 and 4 (X, Y), words at 6 and 8 (width and height, 0 means "from the bitmap"), long at 10 (graphic index or bitmap pointer). `set_layout_bitmap` and `set_layout_graphic` change the long.

`temp_box` returns one of 8 rotating static boxes (`g_temp_boxes`). A caller must not keep the pointer.

## Globals owned by other ranges

These globals appear in ranges 4–6 but belong to other source files. They are not in the CSV. Suggested names:

| Address | Suggested name | Evidence |
|---|---|---|
| `0x1001ca` | `g_viewport_overwritten` | Set by `load_graphic_into_bitmap`; the inventory redraws when set. |
| `0x1001d4`–`0x1001d9` | `g_memory_features[6]` | Set by `memory_select_feature_set`. |
| `0x1001e2` | `g_game_won` | No damage and no archenemy action when set. |
| `0x10020e` | `g_pressing_eye` | Eye click handling. |
| `0x100210` | `g_pressing_mouth` | Mouth click handling. |
| `0x100212` | Unknown pressing flag | Cleared in `champion_kill` for the leader. |
| `0x100220` | `g_refresh_dungeon_view_palette` | Set by `set_dungeon_view_palette`. |
| `0x100266` | `g_object_names` | Pointer to the name pointer array. |
| `0x100640` | `g_ignore_mouse_movements` | Set while pressing eye or mouth. |
| `0x100646` | `g_champion_icon_ordinal_as_pointer` | Champion icon dragged by the mouse. |
| `0x10314e` | `g_last_creature_attack_time` | Set in `group_is_creature_attacking`. |
| `0x103152` | `g_last_party_movement_time` | Read in `champion_apply_time_effects`. |
| `0x103156` | `g_secondary_direction_to_or_from_party` | Group movement. |
| `0x103162`, `0x103164` | `g_move_result_map_x`, `g_move_result_map_y` | Read after `move_get_move_result`. |
| `0x103c04`–`0x103c10` | Primary/secondary mouse and keyboard input tables | Swapped by inventory, sleep, and rename. |

## Uncertain items

- `ui_draw_champion_damage` (`0x11281c`), `champion_clear_icon`, `champion_build_icon_bitmap`, `champion_resurrect`: names come from behavior. ReDMCSB names were not confirmed.
- `is_square_open` (`0x10bce6`): true for corridor, pit, door, and teleporter. Callers are the fluxcage code and the timeline.
- `select_display_buffer` has no caller. The difference between the two copper lists (220 and 200 instructions) is not traced.
- Behavior codes of the group record are from ReDMCSB and only partly checked.
- `g_panel_content` (`0x101e6e`) and `g_panel_content_drawn` (`0x101e70`): two globals where ReDMCSB has one. The split is not fully traced.
- `0x102962`, `0x102964`, `0x102968`, `0x102980` are text or layout metrics. Their meaning is not clear.
- The jump table in `group_is_creature_attacking` (`0x10d75a`) and the one in `blit_shrink_with_palette_changes` (`0x10e9b4`) need disassembly work.
