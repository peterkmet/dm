# Game state

This page describes the game state of the C port: the structs and globals in `src/engine/game.h` and `src/engine/random.h`. It gives the original KAOS address of each item. The names come from [kaos-symbols.csv](../reference/kaos-symbols.csv). The field meanings come from [range-4-6.md](../reference/symbols/range-4-6.md) and [range-7-9.md](../reference/symbols/range-7-9.md).

The port uses native C types for this state. It does not keep the big-endian byte layout. The dungeon data from `dungeon.c` keeps the big-endian layout, see [Dungeon.DAT format](../formats/dungeon.md). A save file reader or writer must convert between the two layouts.

## Rules for types

- A word in the original is `int16_t` or `uint16_t`. A long is `int32_t` or `uint32_t`.
- The code keeps the 16-bit overflow and the truncating division of the original.
- Field names and order follow the original record. Fields with no known code reference are not in the C structs.

## Random number generator

| C name | Original | Notes |
|---|---|---|
| `g_random_seed` | `0x100ef8` (long) | Seed. |
| `random_init()` | `system_init` | Sets the seed to the microseconds of the current time. The original reads `IntuitionBase->Micros` (offset `0x4c`). |
| `random16()` | `0x1076a0` | `seed = seed * 0xBB40E62D + 11`. Returns `(seed >> 8) & 0xFFFF`. |
| `random_bit()` | `0x1076bc` | Same step. Returns bit 0 of `seed >> 8`. |
| `random_2bits()` | `0x1076da` | Same step. Returns bits 0–1 of `seed >> 8`. |

KAOS has no other random function. Callers compute `random16() % n` inline.

`game_start_new` does not call `random_init`. The program calls `random_init` one time at start, as `system_init` does.

## Party

`Party g_party` is the record at `0x101ed4` (`0x58` bytes in the original).

| Offset | C field | Type |
|---|---|---|
| `0x00` | `magical_light_amount` | `int16_t` |
| `0x02` | `thieves_eye_count` | `uint8_t` |
| `0x03` | `footprints_count` | `uint8_t` |
| `0x04` | `shield_defense` | `int16_t` |
| `0x06` | `fire_shield_defense` | `int16_t` |
| `0x08` | `spell_shield_defense` | `int16_t` |
| `0x0a` | `scent_count` | `uint8_t` |
| `0x0b` | `freeze_life_ticks` | `uint8_t` |
| `0x0c` | `first_scent_index` | `uint8_t` |
| `0x0d` | `last_scent_index` | `uint8_t` |
| `0x0e` | `scents[24]` | `uint16_t`: X bits 11–15, Y bits 6–10, map bits 0–5 |
| `0x3e` | `scent_strengths[24]` | `uint8_t` |
| `0x56` | `invisibility_count` | `uint8_t` |

The party position is in `dungeon.h`: `g_party_map_x` (`0x1001ec`), `g_party_map_y` (`0x1001ee`), `g_party_direction` (`0x1001ea`), `g_party_map_index` (`0x1001f0`).

## Champions

`Champion g_champions[4]` is the array at `0x101f54`. Each original record has `0x140` bytes.

| Offset | C field | Type |
|---|---|---|
| `0x00` | `name[8]` | `char` |
| `0x08` | `title[20]` | `char` |
| `0x1c` | `direction` | `uint8_t` |
| `0x1d` | `cell` | `uint8_t` |
| `0x20` | `action_index` | `uint8_t`, 255 is none |
| `0x21` | `symbol_step` | `uint8_t` |
| `0x22` | `symbols[5]` | `char` |
| `0x28` | `direction_maximum_damage_received` | `uint8_t` |
| `0x29` | `maximum_damage_received` | `uint8_t` |
| `0x2a` | `poison_event_count` | `uint8_t` |
| `0x2c` | `enable_action_event_index` | `int16_t`, event 11 or −1 |
| `0x2e` | `hide_damage_received_event_index` | `int16_t`, event 12 or −1 |
| `0x30` | `attributes` | `uint16_t`, redraw flags |
| `0x32` | `wounds` | `uint16_t` |
| `0x34`–`0x3e` | `current_health`, `maximum_health`, `current_stamina`, `maximum_stamina`, `current_mana`, `maximum_mana` | `int16_t` |
| `0x40` | `action_defense` | `int16_t` |
| `0x42` | `food` | `int16_t` |
| `0x44` | `water` | `int16_t` |
| `0x46` | `statistics[7][3]` | `uint8_t` |
| `0x5c` | `skills[20]` | `Skill`: `temporary_experience` (`int16_t`), `experience` (`int32_t`) |
| `0xd4` | `slots[30]` | `uint16_t` things |
| `0x110` | `load` | `uint16_t` |
| `0x112` | `shield_defense` | `int16_t` |

The first index of `statistics` is `STAT_LUCK`, `STAT_STRENGTH`, `STAT_DEXTERITY`, `STAT_WISDOM`, `STAT_VITALITY`, `STAT_ANTIMAGIC`, or `STAT_ANTIFIRE`. The second index is `STAT_MAXIMUM`, `STAT_CURRENT`, or `STAT_MINIMUM`.

The portrait pointer (`0x114`) is not in the struct. Champion recruiting adds it in a later step.

## Other globals

| C name | Original | Type |
|---|---|---|
| `g_party_champion_count` | `0x1001e8` | `uint16_t` |
| `g_leader_index` | `0x101ed2` | `int16_t`, −1 is no leader |
| `g_magic_caster_champion_index` | `0x103fe0` | `int16_t`, −1 is none |
| `g_leader_hand_object` | `0x102454` | `uint16_t` thing, `0xffff` is empty |
| `g_party_is_sleeping` | `0x1001de` | `int16_t` |
| `g_party_dead` | `0x1001e4` | `int16_t` |
| `g_game_time` | `0x1001f8` | `uint32_t` |
| `g_game_id` | `0x1040f2` | `uint32_t` |
| `g_last_creature_attack_time` | `0x10314e` | `int32_t` |
| `g_last_party_movement_time` | `0x103152` | `int32_t` |
| `g_timeline.watchdog_time` | `0x103184` | `int32_t` |
| `g_disabled_movement_ticks` | `0x1001f2` | `int16_t` |
| `g_projectile_disabled_movement_ticks` | `0x1001f4` | `int16_t` |
| `g_last_projectile_disabled_movement_direction` | `0x1001f6` | `int16_t` |

These globals are the values that the save file header stores.

## Timeline

| C name | Original | Type |
|---|---|---|
| `g_timeline.events` | `0x103178` | `TimelineEvent *`, `g_timeline.capacity` slots |
| `g_timeline.heap` | `0x10317c` | `uint16_t *`, binary min-heap of slot indices |
| `g_timeline.count` | `0x103180` | `uint16_t`, heap size |
| `g_timeline.first_unused` | `0x103182` | `int16_t`, head of the free list |
| `g_timeline.high_water` | `0x103176` | `uint16_t`, highest used slot + 1 |
| `g_timeline.capacity` | `0x103174` | `int16_t` |

`TimelineEvent` replaces the 10-byte original event:

| Offset | C field | Meaning |
|---|---|---|
| 0 | `map_time` (`uint32_t`) | Map index in bits 24–31, game time in bits 0–23. |
| 4 | `type` (`uint8_t`) | Event type. 0 is a free slot. |
| 5 | `priority` (`uint8_t`) | Priority. Champion events keep the champion index here. |
| 6 | `b` (`uint16_t`) | Square events: `x << 8 | y` (byte 6 is X, byte 7 is Y). |
| 8 | `c` (`uint16_t`) | Square events: `cell << 8 | effect` (byte 8 is the cell, byte 9 is the effect). |

A free slot keeps the index of the next free slot in `map_time`. The value `0xffff` ends the list. The original keeps this index in the high word of the long. No code reads the other bits of a free slot.

### Order

`timeline_is_event_a_before_b(a, b)` (`0x11f23c`) returns 1 when:

1. The time of `a` (bits 0–23) is lower, or
2. The times are equal and `type << 8 | priority` of `a` is higher, or
3. Both are equal and `a` has the lower slot address.

### Functions

| C function | Original | Work |
|---|---|---|
| `timeline_init` | `0x11f16a` | New game only. Allocates the arrays, builds the free list, and adds the watchdog event. |
| `timeline_get_index` | `0x11f26c` | Finds a slot in the heap. A missing slot is fatal error `0x46`. |
| `timeline_fix_chronology` | `0x11f296` | Moves a heap entry up. When it does not move up, moves it down. |
| `timeline_delete_event` | `0x11f3d0` | Frees a slot and removes it from the heap. |
| `timeline_find_mergeable_event` | `0x11f43c` | See [Merge rules](#merge-rules). |
| `timeline_add_event` | `0x11f59c` | Type 0 returns −1. A full timeline is fatal error `0x2d`. Merges or takes a free slot. Returns the slot index. |
| `timeline_extract_first_event` | `0x11f634` | Copies the heap root and deletes it. |
| `timeline_is_first_event_expired` | `0x11f662` | 1 when the root time is not greater than `g_game_time`. |
| `timeline_add_watchdog_event` | `0x120662` | Adds event 53 at `g_game_time + 300`. The original leaves priority, `b`, and `c` as stack data. The port sets them to 0. |

### Merge rules

`timeline_find_mergeable_event` scans the slots below `g_timeline.high_water`. "Same place" means the same `map_time` and the same `b`.

- New type 5–10 (square event):
  - An existing type 5–10 event at the same place gets the new effect. For type 6, the cell must also be equal. The function returns its index.
  - An existing type 1 event at the same place is deleted. A new effect 2 (toggle) becomes `1 − old effect`. The function returns −1, so the new event gets a slot.
- New type 1 (door animation): an existing type 10 event at the same place returns its index. When its effect is 2, it gets `1 − new effect`. An existing type 1 event at the same place gets the new effect and returns its index.
- New type 2 (door destruction): every type 1 and type 10 event with the same `b` and the same map is deleted. The function returns −1.
- Other new types do not merge.

Note: [range-7-9.md](../reference/symbols/range-7-9.md) says that "any other new type" deletes a type 1 event. The code does this only for new types 5–10.

## Active creature groups

| C name | Original | Type |
|---|---|---|
| `g_active_groups` | `0x101e22` | `ActiveGroup *` |
| `g_active_group_count` | `0x101e18` | `uint16_t` |
| `g_max_active_group_count` | `0x101e26` | `uint16_t`, 110 for a new game |

`ActiveGroup` has the 16-byte layout of the original: `group_index` (`int16_t`, −1 is a free entry), `directions`, `cells`, `last_move_time`, `delay_fleeing`, `target_x`, `target_y`, `prior_x`, `prior_y`, `home_x`, `home_y`, and `aspect[4]`. All fields after `group_index` are `uint8_t`.

`game_init_active_groups` (`0x10cb76`) allocates 110 entries and sets `group_index` to −1 in each entry. The port has only the new-game path.

## Projectiles and explosions

Projectiles (thing type 14) and explosions (thing type 15) are dungeon things. Their records stay in `g_thing_data[14]` and `g_thing_data[15]` in big-endian layout. Their movement uses timeline events 25, 48, and 49. The port has no separate state for them.

## New game

`game_start_new()` sets the state of a new game. Call it after `dungeon_load`. It follows `load_game` (new-game path) and the end of `load_dungeon`:

1. Clear `g_party` and `g_champions`. Set the champion count to 0, the leader and the magic caster to −1, and the leader hand object to `0xffff`.
2. Set `g_game_time`, the sleep and death flags, the movement counters, and `g_active_group_count` to 0.
3. Set `g_game_id` to the product of two `random16()` values.
4. Call `timeline_init` and `game_init_active_groups`.

`dungeon_load` sets the party position and direction from the dungeon header, and `g_party_map_index` to 0.

## Helpers

| C function | Original | Work |
|---|---|---|
| `min16`, `max16` | `0x107652`, `0x107668` | Minimum and maximum of two words. |
| `bounded_value(min, value, max)` | `0x10767e` | Returns `min` when `value <= min`, else the lower of `value` and `max`. |
| `scale_value(a, shift, b)` | `0x1076f8` | `(a * b) >> shift`: unsigned 16 × 16 multiply, arithmetic shift of the 32-bit result. Callers use the low word. |
| `game_update_map_coordinates_after_relative_movement` | `0x10a85c` | Adds forward and right steps. Reads the step tables through `tables_get_step_east` and `tables_get_step_north` (KAOS `0x101038`: 0, 1, 0, −1 and `0x101040`: −1, 0, 1, 0). |
| `NORMALIZE(direction)` | — | `direction & 3`. |
| `game_get_stamina_adjusted_value` | `0x110f4c` | When current stamina is below half of the maximum: `value / 2 + value * stamina / (maximum / 2)`. |
| `game_get_statistic_adjusted_attack` | `0x110f88` | `d = 170 − current statistic`. Returns `attack >> 3` when `d < 16`, else `scale_value(attack, 7, d)`. |
| `game_adjust_statistic_current_value` | `0x10f860` | Adds a delta to a current statistic. A negative delta stops at the minimum. Above 120 the delta is halved, above 150 quartered (plus 1). The result stops at 170. |
| `game_is_lucky` | `0x110fc8` | See below. |

`game_is_lucky(champion, percentage)`:

1. When `random_bit()` is 1 and `percentage < random16() % 100`, return 1. Luck does not change.
2. Lucky is true when the current luck is above 0 and `percentage < random16() % (2 * luck)`.
3. Add −2 (lucky) or +2 to the current luck, bounded by the minimum and maximum. Return lucky.

The original reads the three luck bytes as signed bytes in this function. A current luck of 128 or more counts as not above 0. The port keeps this behavior.

KAOS tables must be loaded (`tables_load`) before `game_update_map_coordinates_after_relative_movement` runs.

## Tests

`tests/differential/test_game.py` runs these checks with `build/libdm.dylib`:

- The random functions, the math helpers, the relative movement, and the four champion helpers give the same results as the original functions in `dmtools.emu`.
- A sequence of 1500 timeline additions and extractions gives the same heap, slots, free list, and return values as the original `timeline_add_event` and `timeline_extract_first_event` in the emulator.
- `game_start_new` after `dungeon_load` gives the party position from the `dmcsb` header, one watchdog event, and 110 free active groups.

The emulator tests skip when `game/KAOS.FTL` is missing.
