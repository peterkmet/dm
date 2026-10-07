# Static game tables

This page describes the static rule tables of `KAOS.FTL` and the C accessors in `port/src/data/tables.h`. The port does not copy table values into the source. `tables_init` reads the tables from the loaded KAOS image at run time.

All addresses are original KAOS addresses (see [KAOS module map](kaos-map.md)). All multi-byte values are big-endian in the image. The C structs hold native values.

## Usage

1. Load the module with `kaos_load(path)`.
2. Call `tables_init()`. It returns 0, or -1 when no KAOS image is loaded.
3. Call an accessor, for example `tables_creature_info(type)`.

Each accessor returns a pointer to a constant record. It returns `NULL` for an index out of range and before `tables_init`.

## Table list

| Accessor | Address | Record size | Count | Index |
|---|---|---|---|---|
| `tables_creature_info` | `0x10192e` | 26 | 27 | Creature type |
| `tables_creature_sound` | `0x101bec` | 2 | 18 | Sound ordinal − 1 |
| `tables_object_info` | `0x101068` | 6 | 180 | Object info index |
| `tables_weapon_info` | `0x1014a0` | 6 | 46 | Weapon type |
| `tables_armour_info` | `0x1015b4` | 4 | 58 | Armour type |
| `tables_junk_weight` | `0x10169c` | 1 | 53 | Junk type |
| `tables_food_value` | `0x1016d2` | 2 | 8 | Icon − `0xa8` |
| `tables_door_info` | `0x1016e2` | 2 | 4 | Door type |
| `tables_spell` | `0x103c84` | 8 | 25 | Spell |
| `tables_symbol_base_mana_cost` | `0x103c66` | 1 | 4 × 6 | Symbol step, symbol |
| `tables_symbol_mana_cost_multiplier` | `0x103c7e` | 1 | 6 | Power symbol |
| `tables_action_set` | `0x103d4c` | 8 | 44 | Object info action set index |
| `tables_action_info` | `0x103eac`–`0x103fb4` | 7 × 1 | 44 | Action |
| `tables_light_power_to_light_amount` | `0x100cb4` | 2 | 16 | Light power |
| `tables_palette_light_threshold` | `0x100cd4` | 2 | 6 | Palette index |
| `tables_slot_mask` | `0x100c68` | 2 | 38 | Champion slot (30), then chest slot (8) |
| `tables_wound_defense_factor` | `0x100ce4` | 1 | 6 | Body slot |
| `tables_fixed_possession` | `0x101c10` | 2 | 40 | Word in the list block |

Each table ends where the next named table starts. The tests check this.

## Creature info

`CreatureInfo`, 26 bytes per creature type. The code reads `0x10192e + type * 0x1a`.

| Offset | Field | Meaning |
|---|---|---|
| 0 | `aspect_index` | Index into `g_creature_aspects`. |
| 1 | `attack_sound_ordinal` | 0 = no sound. Otherwise index + 1 into the creature sound table. |
| 2 | `attributes` | Bits 0–1 size, bit 2 side attack, bit 3 prefer back row, bit 4 attack any champion, bits 7–8 height, bit 9 drops fixed possessions, bit 11 sees invisible, bit 12 night vision, bit 13 archenemy. |
| 4 | `graphic_info` | Bit 2 flip when not attacking, bit 9 flip when attacking, bit 10 flip during attack, bits 12–13 horizontal offset range, bits 14–15 vertical offset range. |
| 6 | `movement_ticks` | Ticks between moves. |
| 7 | `attack_ticks` | Ticks between attacks. |
| 8 | `defense` | 255 = invulnerable. |
| 9 | `base_health` | Base health. |
| 10 | `attack` | Attack strength. |
| 11 | `poison_attack` | Poison attack strength. |
| 12 | `dexterity` | Dexterity. |
| 13 | — | Padding. |
| 14 | `ranges` | Bits 0–3 sight, bits 8–11 smell, bits 12–15 attack. |
| 16 | `properties` | Bits 4–7 fear resistance, bits 8–11 experience, bits 12–15 wariness. |
| 18 | `resistances` | Bits 8–11 poison. |
| 20 | `animation_ticks` | Bits 4–7 non-attack, bits 8–11 attack. |
| 22 | `wound_probabilities` | Four 4-bit thresholds. The creature attack code compares them with a random nibble to select an entry of `g_wound_probability_index_to_mask`. |
| 24 | `attack_type` | Attack type argument of `champion_add_pending_damage_and_wounds`. |
| 25 | — | Padding. |

`group_get_champion_damage` (`0x11d10a`) copies the whole 26-byte record to the stack. That code confirms the offsets of `wound_probabilities` and `attack_type`.

## Creature sounds

`CreatureSound`, 2 signed bytes per sound ordinal. Byte 0 `attack` is the attack sound index. Byte 1 `movement` is the movement sound index. −1 = no sound. The count (18) is the highest `attack_sound_ordinal` in the creature table.

## Object info

`ObjectInfo`, 6 bytes per object info index. `get_object_info_index` maps a thing to the index: scroll 0, container 1, potion 2 + type, weapon `0x17` + type, armour `0x45` + type, junk `0x7f` + type.

| Offset | Field | Meaning |
|---|---|---|
| 0 | `type` | Object name index and base icon index. |
| 2 | `aspect_index` | Index into `g_object_aspects`. |
| 3 | `action_set_index` | Index into the action set table. |
| 4 | `allowed_slots` | Bit mask. A slot accepts the object when `allowed_slots & slot_mask` is not 0. |

## Weapon info

`WeaponInfo`, 6 bytes per weapon type.

| Offset | Field | Meaning |
|---|---|---|
| 0 | `weight` | Weight in tenths of a kilogram. |
| 1 | `weapon_class` | Below 16: melee and thrown classes. 16–31: arrows (need launcher class 10). 32–47: sling stones (need launcher class 11). |
| 2 | `strength` | Weapon strength. |
| 3 | `kinetic_energy` | Kinetic energy of a thrown or shot weapon. |
| 4 | `attributes` | Bits 8–12 projectile aspect ordinal (0 = use the object aspect). Bits 0–7 shoot attack (ReDMCSB name, not traced). |

## Armour info

`ArmourInfo`, 4 bytes per armour type. Byte 3 is padding.

| Offset | Field | Meaning |
|---|---|---|
| 0 | `weight` | Weight in tenths of a kilogram. |
| 1 | `defense` | Defense. |
| 2 | `attributes` | Bits 0–2 sharp resistance (`get_armour_defense`). Bit 7 shield. |

## Junk weights and food values

- Junk weight: 1 byte per junk type. Junk type 1 adds 2 for each charge (`get_object_weight`). Potions, scrolls, and containers have no weight table. `get_object_weight` uses fixed values for them.
- Food value: 1 signed word per food icon `0xa8`–`0xaf`. Eating adds the value to the champion food, with a maximum of 2048.

## Door info

`DoorInfo`, 2 bytes per door type. Byte 0 `attributes`, byte 1 `defense`. `group_is_door_destroyed_by_attack` compares an attack with `defense`. Each map selects 2 door types (map record byte 14).

## Spells and symbols

`Spell`, 8 bytes per spell.

| Offset | Field | Meaning |
|---|---|---|
| 0 | `symbols` | Long. Bits 24–31 power symbol (0 = the spell has no power symbol, compare only bits 0–23). Symbols are characters `0x60 + step × 6 + n`. |
| 4 | `base_required_skill_level` | Required level before the power is added. |
| 5 | `skill_index` | Skill that casts and trains the spell. |
| 6 | `kind` | Bits 0–3 of the word at offset 6: 1 potion, 2 projectile, 3 other. |
| 6 | `type` | Bits 4–9: potion type, explosion type + 128, or other spell number. |
| 6 | `disabled_ticks` | Bits 10–15: action disabled ticks after the cast. |

`tables_symbol_base_mana_cost(step, n)` is the mana cost of symbol `n` at symbol step `step`. For steps after the first, the cost is `base × multiplier[power] >> 3`, where `multiplier` is `tables_symbol_mana_cost_multiplier`.

## Actions

`ActionSet`, 8 bytes per action set. The object info `action_set_index` selects it. An empty hand uses set 2.

| Offset | Field | Meaning |
|---|---|---|
| 0 | `action_indices[3]` | Action of each menu row. `0xff` = none. |
| 3 | — | Padding. |
| 4 | `minimum_skill_levels[2]` | Minimum skill level of actions 1 and 2. Bit 7 = the action needs charges. |
| 6 | `byte_6` | Copied to `0x103fea`. No reader is known. |
| 7 | — | Padding. |

`ActionInfo` joins seven byte tables of 44 entries each, indexed by action:

| Address | Field | Meaning |
|---|---|---|
| `0x103eac` | `disabled_ticks` | Duration of `magic_disable_action`. |
| `0x103ed8` | `damage_factor` | Melee damage factor. |
| `0x103f04` | `hit_probability` | Melee hit probability. |
| `0x103f30` | `stamina` | Stamina cost. |
| `0x103f5c` | `defense` | Signed defense modifier while the action runs. |
| `0x103f88` | `skill_index` | Skill that the action needs and trains. |
| `0x103fb4` | `experience_gain` | Experience gain. |

The action names come from graphic 699, not from KAOS.

## Light, slots, and wounds

- Light power to light amount: 16 signed words. `set_dungeon_view_palette` and the light event use it.
- Palette light thresholds: 6 signed words. The palette index is the first entry that is not greater than the light sum.
- Slot masks: 38 words. Index 0–29 are champion slots, 30–37 are chest slots. Compare with `allowed_slots` of the object info.
- Wound defense factor: 6 bytes, indexed by body slot. The champion defense code multiplies the shield defense with it.

## Fixed possessions

40 words at `0x101c10`. The block holds several lists, each ended by a 0 word. Each word is an object info index. Bit 15 marks a possession that the creature drops at random. The creature drop code selects the list start for each creature type.

## Not in this module

These tables are in the data region, but other modules own them:

- Graphics and view tables: object, projectile, and creature aspects, ornament coordinate sets, palette changes.
- Input tables: mouse and keyboard input, movement arrows.
- Dungeon format tables: thing data byte counts, new-game extra thing counts, direction steps.
