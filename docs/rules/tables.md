# Static game tables

This page describes the static rule tables of `KAOS.FTL` and the C accessors in `src/data/tables.h`. The port does not copy table values into the source. `tables_init` reads the tables from the loaded KAOS image at run time.

All addresses are original KAOS addresses (see [KAOS module map](../reference/kaos-map.md)). All multi-byte values are big-endian in the image. The C structs hold native values.

## Usage

1. Load the module with `kaos_load(path)`.
2. Call `tables_init()`. It returns 0, or -1 when no KAOS image is loaded.
3. Call an accessor, for example `tables_get_creature_info(type)`.

`tables_load(path)` does steps 1 and 2. It returns 0 on success, or the error of `kaos_load`.

Each accessor of the table list returns a pointer to a constant record. It returns `NULL` for an index out of range and before `tables_init`. Load the module again (CSB or DM), then call `tables_init` again.

The image accessors read the loaded image at call time. They need `kaos_load`, not `tables_init`. They do not check the index. All addresses are in `src/data/kaos_addresses.h`.

| Image accessor | Address | Element | Index |
|---|---|---|---|
| `tables_get_step_east`, `tables_get_step_north` | `0x101038`, `0x101040` | int16 × 4 | Direction |
| `tables_get_arrow_steps_forward`, `tables_get_arrow_steps_right` | `0x103c56`, `0x103c5e` | int16 × 4 | Movement arrow |
| `tables_get_ordered_cell_to_attack` | `0x100b66` | uint8 × 8 × 4 | Row (attacker side + 2 × direction), cell order |
| `tables_get_wound_probability_mask` | `0x100b86` | uint8 × 4 | Wound probability index |
| `tables_get_slot_to_steal_from` | `0x100b8a` | uint8 × 8 | Random index |
| `tables_get_torch_icon_offset` | `0x100ba0` | uint8 × 16 | Torch charges |
| `tables_get_slot_drop_order` | `0x100d0e` | uint16 × 30 | Drop order |
| `tables_get_champion_color` | `0x100ce0` | uint8 × 4 | Champion |
| `tables_get_square_type_event` | `0x100d4a` | uint8 × 8 | Square type |
| `tables_get_sound_graphic`, `tables_get_sound_rate`, `tables_get_sound_priority` | `0x100d52` (+0, +3, +4) | 12-byte record × 35 | Sound |
| `tables_get_sound_volume` | `0x100696` | uint8 × 25 × 25 | Squares ahead, squares to the side (−12 to 12) |
| `tables_get_new_game_extra_things` | `0x101058` | uint8 × 16 | Thing type |
| `tables_get_door_attributes` | `0x1016e2` | uint16 × 4 (attributes << 8, defense) | Door set |
| `tables_get_message_escape`, `tables_get_inscription_escape` | `0x1016ea`, `0x10182a` | char[8] × 32 | Escape code |
| `tables_get_escape_replacement` | `0x1017ea` | char[2] × 32 | Escape code |
| `tables_get_portrait_width`, `tables_get_portrait_height` | `0x102958`, `0x10295a` | int16 | — |
| `tables_get_replacement_color` | `0x102efa` | 14-byte record × 13: uint16 × 6 | Replacement set, light level |
| `tables_get_replacement_change` | `0x102efa` (+12, +13) | uint8 | Replacement set, depth 2 or 3 |
| `tables_get_creature_aspect` | `0x102e58` | 6-byte record × 27 (pointer) | Creature type |
| `tables_get_object_aspect` | `0x102c68` | 4-byte record × 85 (pointer) | Object aspect |
| `tables_get_projectile_aspect` | `0x102db8` | 4-byte record (pointer) | Projectile aspect 1-14 |
| `tables_get_field_aspect` | `0x102b26` | 8-byte record × 12 (pointer) | Field aspect |
| `tables_get_creature_shift` | `0x102fd0` | int8 × 3 × 8 | Distance, shift index |
| `tables_get_pile_shift` | `0x102e38` | uint8 × 16 × 2 | Pile pair, axis (x, y) |
| `tables_get_object_scale`, `tables_get_projectile_scale`, `tables_get_explosion_scale` | `0x102e24`, `0x102e2a`, `0x102e32` | uint8 × 5, × 7, × 5 | View row, view row, depth |
| `tables_get_rebirth_scale` | `0x102fe8` (step 2), `0x102ff0` (step 1) | uint8 × 7 | Rebirth step, coordinate set |
| `tables_get_group_directions` | `0x10192a` | uint8 × 4 | Direction |
| `tables_get_view_square` | `0x102a7e` + 24·n (7 tables) | int8 × 23 each | View square |
| `tables_get_wall_set_layout_id` | `0x102a22` | int16 × 46 | Wall set entry |
| `tables_get_wall_ornament_depth` | `0x102ba6` | uint8 × 14 | Wall ornament view |
| `tables_get_plaque_height` | `0x102c57` (one byte before the table) | uint8 × 3 × 5 | Line count 1-3, depth |
| `tables_get_floor_ornament_view_offset` | `0x102bb4` | uint8 × 11 | Floor ornament view |
| `tables_get_alcove_ornament`, `tables_get_fountain_ornament` | `0x102bc0`, `0x102bc3` | uint8 × 3, × 1 | Alcove index, — |
| `tables_get_wall_ornament_coord_set`, `tables_get_floor_ornament_coord_set`, `tables_get_door_ornament_coord_set`, `tables_get_door_button_coord_set` | `0x102bc4`, `0x102c00`, `0x102c0a`, `0x102c16` | uint8 | Ornament graphic, — |
| `tables_get_glyph_width`, `tables_get_glyph_height` | `0x10296e`, `0x102970` | int16 | — |
| `tables_get_viewport_x`, `tables_get_viewport_y`, `tables_get_viewport_width`, `tables_get_viewport_height` | `0x10293a`, `0x10293c`, `0x102948`, `0x10294a` | int16 | — |
| `tables_get_layout_bitmap` | `0x1025f2` | 14-byte record (x, y, w, h at +2 … +8) | Layout bitmap slot |
| `tables_get_palette`, `tables_get_palette_color` | `0x100170`, `0x100190`, `0x100a46`, `0x100a66`, `0x100aa6` | uint16 × 16 | `TABLES_PALETTE_*` (dungeon + light level), color |
| `tables_get_palette_changes` | `0x102c18` … `0x102fc0`, `0x10247a`, `0x103ffa`, `0x1005fe` | uint8 × 16 (pointer) | `TABLES_CHANGES_*` |
| `tables_get_icon_sheet_first` | `0x100b92` | int16 × 7 | Icon sheet |
| `tables_get_slot_box_zone` | `0x100bb0` | 4-byte record × 46 (zone word) | Slot box |
| `tables_get_slot_box_border`, `tables_get_bar_shadow` | `0x102956`, `0x102980` | int16 | — |
| `tables_get_champion_icon_width`, `tables_get_champion_icon_height` | `0x10295c`, `0x10295e` | int16 | — |
| `tables_get_damage_splat_width`, `tables_get_damage_splat_height` | `0x10297c` (small), `0x102978` (large) | int16 × 2 | Large |
| `tables_get_skill_level_text`, `tables_get_base_skill_text`, `tables_get_statistic_text`, `tables_get_direction_text` | `0x101e74`, `0x102472`, `0x101e9a`, `0x101e92` | int16 text index | Level 2-16, base skill, statistic, direction |
| `tables_get_text` | `0x101ebe`, `0x101ec6`, `0x101eca`, `0x101ec8`, `0x10402c` | C string (pointer) | `TABLES_TEXT_*` |
| `tables_get_message_line_pad`, `tables_get_message_color` | `0x102966`, `0x104026` | int16, int16 × 2 (low byte) | —, message |
| `tables_get_button_border`, `tables_get_button_inner_border` | `0x102988`, `0x10298c` | int16 × 2 | `TABLES_AXIS_X`, `TABLES_AXIS_Y` |
| `tables_get_csb_restart_box` | `0x104248` (CSB image) | 8-byte box × 2 | Inner |
| `tables_get_pointer_zone` | `0x1004f8` | 12-byte record × 16 | Zone |
| `tables_get_pointer_hotspot_x`, `tables_get_pointer_hotspot_y` | `0x1005c8` | 8-byte record × 4 | Pointer shape |
| `tables_get_pointer_shadow_dx`, `tables_get_pointer_shadow_dy` | `0x102952`, `0x102954` | int16 | — |
| `tables_get_pointer_planes` | `0x1002b0` (arrow), `0x1003d4` (hand) | 4 bit planes (pointer), size words before | Hand |
| `tables_get_mouse_input`, `tables_set_mouse_input_box` | `0x10318c` … `0x10378c` (14 tables), dialog tables through the pointers at `0x103bd4` | 12-byte entry (command, x1, x2, y1, y2, buttons); command 0 ends a table | `UI_INPUT_*` table, entry |
| `tables_get_key_command` | `0x10393c`, `0x103948`, `0x103964`, `0x103980`, `0x10398c` | 4-byte entry (command, key); command 0 ends a table | `TABLES_KEYS_*`, key |

## Table list

| Accessor | Address | Record size | Count | Index |
|---|---|---|---|---|
| `tables_get_creature_info` | `0x10192e` | 26 | 27 | Creature type |
| `tables_get_creature_sound` | `0x101bec` | 2 | 18 | Sound ordinal − 1 |
| `tables_get_object_info` | `0x101068` | 6 | 180 | Object info index |
| `tables_get_weapon_info` | `0x1014a0` | 6 | 46 | Weapon type |
| `tables_get_armour_info` | `0x1015b4` | 4 | 58 | Armour type |
| `tables_get_junk_weight` | `0x10169c` | 1 | 53 | Junk type |
| `tables_get_food_value` | `0x1016d2` | 2 | 8 | Icon − `0xa8` |
| `tables_get_door_info` | `0x1016e2` | 2 | 4 | Door type |
| `tables_get_spell` | `0x103c84` | 8 | 25 | Spell |
| `tables_get_symbol_base_mana_cost` | `0x103c66` | 1 | 4 × 6 | Symbol step, symbol |
| `tables_get_symbol_mana_cost_multiplier` | `0x103c7e` | 1 | 6 | Power symbol |
| `tables_get_action_set` | `0x103d4c` | 8 | 44 | Object info action set index |
| `tables_get_action_info` | `0x103eac`–`0x103fb4` | 7 × 1 | 44 | Action |
| `tables_get_light_power_to_light_amount` | `0x100cb4` | 2 | 16 | Light power |
| `tables_get_palette_light_threshold` | `0x100cd4` | 2 | 6 | Palette index |
| `tables_get_slot_mask` | `0x100c68` | 2 | 38 | Champion slot (30), then chest slot (8) |
| `tables_get_wound_defense_factor` | `0x100ce4` | 1 | 6 | Body slot |
| `tables_get_fixed_possession` | `0x101c10` | 2 | 40 | Word in the list block |

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

`ObjectInfo`, 6 bytes per object info index. `inventory_get_object_info_index` maps a thing to the index: scroll 0, container 1, potion 2 + type, weapon `0x17` + type, armour `0x45` + type, junk `0x7f` + type.

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
| 2 | `attributes` | Bits 0–2 sharp resistance (`inventory_get_armour_defense`). Bit 7 shield. |

## Junk weights and food values

- Junk weight: 1 byte per junk type. Junk type 1 adds 2 for each charge (`inventory_get_object_weight`). Potions, scrolls, and containers have no weight table. `inventory_get_object_weight` uses fixed values for them.
- Food value: 1 signed word per food icon `0xa8`–`0xaf`. Eating adds the value to the champion food, with a maximum of 2048.

## Door info

`DoorInfo`, 2 bytes per door type. Byte 0 `attributes`, byte 1 `defense`. `event_is_door_destroyed_by_attack` compares an attack with `defense`. Each map selects 2 door types (map record byte 14).

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

`tables_get_symbol_base_mana_cost(step, n)` is the mana cost of symbol `n` at symbol step `step`. For steps after the first, the cost is `base × multiplier[power] >> 3`, where `multiplier` is `tables_get_symbol_mana_cost_multiplier`.

## Actions

`ActionSet`, 8 bytes per action set. The object info `action_set_index` selects it. An empty hand uses set 2.

| Offset | Field | Meaning |
|---|---|---|
| 0 | `action_indices[3]` | Action of each menu row. `0xff` = none. |
| 3 | — | Padding. |
| 4 | `minimum_skill_levels[2]` | Minimum skill level of actions 1 and 2. Bit 7 = the action needs charges. |
| 6 | `flags` | Copied to `0x103fea`. No reader is known. |
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
