# Champion rules

This page describes the champion rules of the C port: recruiting, skills, inventory, eating and drinking, damage, death, and the time effects. The code is in `src/engine/champion.c` and `src/engine/inventory.c`. The rules follow the decompiled `KAOS.FTL` code (CHAMPION.C, the inventory part of range 5, and `process_click_on_mouth`). The original addresses are in [kaos-symbols.csv](../reference/kaos-symbols.csv). The champion record is in [game-state.md](game-state.md#champions).

The port reads all game tables from the KAOS image at run time ([tables.md](tables.md)). Object record fields go through the accessors in `src/engine/record.h`. Icon and text numbers are in `src/data/ids.h`.

## Constants

`champion.h` names the slots, skills, wounds, and attack types.

| Name | Values |
|---|---|
| Slots | `SLOT_HAND_READY` 0, `SLOT_HAND_ACTION` 1, `SLOT_HEAD` 2, `SLOT_TORSO` 3, `SLOT_LEGS` 4, `SLOT_FEET` 5, `SLOT_POUCH_2` 6, quiver 7–9 and 12, `SLOT_NECK` 10, `SLOT_POUCH_1` 11, `SLOT_BACKPACK_FIRST` 13 to 29. `SLOT_COUNT` is 30. Slots 30–37 (`SLOT_CHEST_FIRST`) are the open chest. |
| Skills | Base skills `SKILL_FIGHTER` 0, `SKILL_NINJA` 1, `SKILL_PRIEST` 2, `SKILL_WIZARD` 3. Hidden skills 4–19. Hidden skill `s` belongs to base skill `(s − 4) / 4`. |
| Skill flags | `champion_get_skill_level` takes `ignore_modifiers` (original flag `0x4000`) and `ignore_temporary` (original flag `0x8000`). |
| Wounds | Bit `n` is a wound of body slot `n` (0–5). |
| Attack types | `ATTACK_RAW` 0, `ATTACK_FIRE` 1, `ATTACK_SELF` 2, `ATTACK_BLUNT` 3, `ATTACK_SHARP` 4, `ATTACK_MAGIC` 5, `ATTACK_PSYCHIC` 6, `ATTACK_LIGHTNING` 7. |

## Recruiting

`champion_add_candidate_to_party(portrait)` runs when the party clicks a mirror (sensor type 127). It does nothing when the leader holds an object or the party has 4 champions.

1. The new champion record is cleared. Action is `ACTION_NONE`. The two event indices are −1. Direction is the party direction.
2. The cell is the first free cell, starting at the party direction and going clockwise.
3. Food and water are `1500 + random16() & 255` each (two draws).
4. All slots are empty.
5. The text on the party square gives the name, title, sex, health, stamina, mana, statistics, and experience (`recruit_parse`, [recruit.h](../../src/engine/recruit.h)).
6. The champion becomes the candidate (`g_session.candidate_champion_index`: index, −1 for none). The first champion becomes leader and magic caster.
7. The objects in the cell of the wall square that faces the party become the possessions:
   - Armour goes to the first body slot (head to feet) that accepts it, else to the neck.
   - A weapon goes to the action hand.
   - A scroll or potion goes to pouch 1, then pouch 2.
   - Other objects, and objects whose slot is full, go to the neck (when the object fits there and the neck is empty) or to the next backpack slot.
8. The inventory of the candidate opens.

The possessions stay in the wall square list until the player accepts the candidate.

`champion_click_rebirth_panel(command)`:

| Command | Effect |
|---|---|
| `PANEL_CANCEL` | Removes the candidate. When it was the only champion, the party has no leader and no caster. |
| `PANEL_RESURRECT` | Unlinks the possessions from the wall square. Disables the mirror sensor (`rec_sensor_set_type` to `SENSOR_DISABLED`). Prints "<name> RESURRECTED." |
| `PANEL_REINCARNATE` | As resurrect. Also clears all experience and adds 1 to the current and maximum value of a random statistic 12 times. Prints "<name> REINCARNATED." The original also asks for a new name. The port keeps the name. |

With the first champion, the time of the last party move becomes the game time.

`champion_resurrect(index)` is the Vi altar rebirth. The champion takes a free cell when its cell is used, loses all objects, and gets `max(25, max_health − max_health / 64 − 1)` maximum health and half of it as current health.

## Skill level

`champion_get_skill_level(index, skill, ignore_modifiers, ignore_temporary)`:

1. A sleeping party gives level 1.
2. Experience is the skill experience plus the temporary experience (unless `ignore_temporary`). A hidden skill adds the same of its base skill and halves the sum.
3. Level is 1. It increases by 1 for each halving while the experience is 500 or more.
4. Unless `ignore_modifiers`: The Firestaff in the action hand adds 1 (complete: 2) to all skills. Neck objects add 1: Gem of Ages (heal), Pendant Feral (wizard), Ekkhard Cross (defend), Moonstone (influence). The Sceptre of Lyf in the action hand adds 1 to heal.

## Experience

`champion_add_skill_experience(index, skill, experience)`:

1. Skills 4–11 get half the experience when the last creature attack is more than 150 ticks ago.
2. The map difficulty (descriptor byte 12, bits 4–7) multiplies the experience when it is not 0.
3. Hidden skills get double experience when the last creature attack is less than 25 ticks ago.
4. The skill experience increases. The temporary experience increases by `bounded(1, experience / 8, 100)` while it is below 32000. A hidden skill also adds the experience to its base skill.
5. When the base skill level (without objects and temporary experience) increases, the champion gains a level:

| Base skill | Strength max | Dexterity max | Wisdom max | Mana max | Health gain `h` | Stamina gain `s` |
|---|---|---|---|---|---|---|
| Fighter | + r2 | + r1 | — | — | 3 × level | max stamina / 16 |
| Ninja | + r1 | + r2 | — | — | 2 × level | max stamina / 21 |
| Priest | — | — | + r1 | + level + extra | level + (level + 1) / 2 | max stamina / 25 |
| Wizard | — | — | + r2 | + level × 1.5 + extra | level | max stamina / 32 |

- `r1 = random_bit()`, `r2 = random_bit() + 1`, then a third bit for vitality: vitality maximum increases by that bit (and by `bit & level` except for the priest). Anti-fire maximum increases by `~level & random_bit()`.
- Priest and wizard: extra mana is `min(random_2bits(), level − 1)`. Maximum mana stops at 900. Anti-magic maximum increases by `random_2bits()`.
- Maximum health increases by `h + random16() % (h / 2 + 1)`, up to 999. Maximum stamina increases by `s + random16() % (s / 2 + 1)`, up to 9999.
- `runtime.skill_level_up[base]` of the champion becomes true. The message area shows "<name> JUST GAINED A <SKILL> LEVEL!" in the champion color.

## Load, movement, dexterity, strength

| Function | Rule |
|---|---|
| `champion_get_maximum_load` | `stamina_adjusted(strength × 8 + 100)`. A wound removes 1/8 (1/4 with a leg wound). Elven Boots on the feet add 1/16. The result rounds up to a multiple of 10 (tenths of a kilogram). |
| `champion_get_movement_ticks` | Below the maximum load: 2, or 3 when the load is above 5/8 of the maximum. Else `(load − max) × 4 / max + 4`. A feet wound adds 1 (2 when overloaded). Boots of Speed subtract 1. |
| `champion_get_dexterity` | Dexterity + `random16() & 7`, minus `load × (dexterity / 2) / max_load`, at least 2, halved while sleeping, then `bounded(1 + (r & 7), value / 2, 100 − (r & 7))`. |
| `champion_get_strength(index, hand)` | Strength + `random16() & 15`, adjusted by the weight of the hand object. A weapon adds its strength and 2 × the levels of swing (swing classes), throw (classes 1–15), and shoot (classes 16–111). Then stamina adjustment, halved by a wound of the hand, and `bounded(0, value / 2, 100)`. |

## Wound defense

`champion_get_wound_defense(index, slot, sharp)`:

1. Each shield in a hand adds `(strength + armour defense) × wound_defense_factor[slot] >> 4`. The shift is 5 when the shield is not in the slot itself.
2. Add `random16() % (vitality / 8 + 1)` (halved for sharp attacks), the action defense, the champion shield (Ya potion), and the party shield.
3. Armour in a body slot (2–5) adds its defense. A sharp attack uses `defense × (sharp resistance + 4) / 8`.
4. A wound of the slot subtracts `random_2bits() + 8`. Sleeping halves the value.
5. The result is `bounded(0, value / 2, 100)`.

## Damage

`champion_add_pending_damage(index, attack, allowed_wounds, type)` adds damage that the main loop applies at the end of the tick. It returns the damage.

1. No damage for no champion, the candidate, a dead champion, or an attack of 0 or less.
2. `ATTACK_RAW` adds the attack without defense and without wounds.
3. Other types use the mean wound defense of the allowed body slots:
   - Fire: anti-fire reduces the attack, then the party fire shield.
   - Self: defense is half the defense plus the ninja level.
   - Magic: anti-magic reduces the attack, then the party spell shield. Armour does not count.
   - Psychic: attack × (115 − wisdom) / 64. Armour does not count.
   - All except magic and psychic: attack × (130 − defense) / 64.
4. Wounds: a threshold is the vitality-adjusted value of `(random16() & 127) + 10`. While the threshold is below the attack, a random allowed body slot gets a wound and the threshold doubles.
5. Damage wakes up a sleeping party.

Sleep keeps the selected runes of the magic caster, in the original (command 145 and `champion_wake_up`) and in the port (`go_to_sleep`). The input of a sleeping party only wakes it up, so the runes do not change during sleep. Mana regeneration does not depend on the selected runes.

`champion_damage_all(attack, wounds, type)` gives each champion `max(1, attack − spread + random16() % (2 × spread))` with `spread = attack / 8 + 1`. It returns the number of champions hurt.

`champion_apply_pending_damage()` adds the pending wounds, subtracts the pending damage, and kills a champion at 0 health. A hurt champion gets event 12 (hide the damage) at time + 5, or the existing event moves to that time. A champion that survives keeps the damage in `damage_to_show`; after the call the game loop takes it with `champion_take_damage_to_show(index)` and draws the splat with the number on the status box (`status_draw_damage`).

`champion_decrement_stamina(index, amount)`: stamina below 1 becomes 0 and gives raw damage of half the deficit. Stamina stops at the maximum.

`champion_poison(index, attack)` gives raw damage `max(1, attack / 64)` and, when `attack − 1` is not 0, adds event 75 at time + 36 with `attack − 1`. `champion_unpoison` deletes the poison events of the champion.

`champion_fall_damage(use_rope)` is the pit fall: `champion_damage_all(20, legs | feet, ATTACK_SELF)` and sound 7. With a rope, each living champion loses `load × 25 / max_load + 1` stamina instead.

## Death

`champion_kill(index)`:

1. Health becomes 0. An open inventory of the champion closes.
2. All objects drop on the party square in the champion cell, in the order of `g_slot_drop_order` (`move_get_move_result`).
3. A junk record (no reserve) becomes the bones (junk type 5, important, champion index in the charges) and drops in the same cell.
4. The spell symbols clear. Poison ends.
5. When no champion lives, `g_session.party_dead` becomes 1. Else the first living champion becomes leader or caster when the dead champion had that role.

## Inventory

| Function | Rule |
|---|---|
| `inventory_get_object_weight` | Weapon and armour weight from the tables. Scroll 1. Potion 3, empty flask 1. Chest 50 plus its contents. Junk weight from the table; the waterskin adds 2 for each charge. |
| `object_get_icon_index` | The object type, plus the state for types 0–30: compass direction, lit torch charge, waterskin and worn jewel charges, weapon charges, closed scroll. |
| `inventory_add_object_in_slot` | Adds the weight to the load and applies the object modifiers. A torch in a hand burns. A scroll in the action hand opens. A jewel on the neck is worn; the Illumulet adds light. |
| `inventory_take_slot_object` | The reverse. A torch in a hand goes out. A scroll leaving the action hand closes. |
| `inventory_put_hand_object`, `inventory_take_hand_object` | The hand object counts in the leader load. |
| `inventory_click_on_slot_box` | Swaps the hand object and the slot object when the slot mask accepts the hand object. |
| `champion_set_leader` | Moves the hand object weight to the new leader and turns the leader to the party direction. |

Object modifiers (removed with the opposite sign):

| Slot | Object | Effect |
|---|---|---|
| 0–12 | Cursed weapon or armour | Luck −3 |
| any | Rabbit's Foot | Luck +10 |
| Action hand | Mace of Order | Strength +5 |
| Action hand | Staff of Claws, staffs and wands, Delta, Vorpal Blade, The Inquisitor | Maximum mana +1 to +16 |
| Legs | Powertowers | Strength +10 |
| Head | Crown of Nerra, Dexhelm | Wisdom +10, dexterity +10 |
| Torso | Flamebain, Cloak of Night | Anti-fire +12, dexterity +8 |
| Neck | Jewel Symal, Cloak of Night, Moonstone | Anti-magic +15, dexterity +8, maximum mana +3 |

A statistic modifier changes the maximum, current, and minimum value.

## Eating and drinking

`inventory_process_click_on_mouth()` uses the leader hand object on the inventory champion. The object must have the mouth bit (bit 0) in its allowed slots.

- Waterskin: with charges, water +800 (maximum 2048) and one charge less.
- Food: the object record becomes free. Food increases by the food value of the icon (maximum 2048).
- Potion: the potion becomes an empty flask. With power `p`, `amount = p / 25 + 8` and `d = ((511 − p) / ((p + 1) / 8 + 32)) / 2`:

| Potion | Effect |
|---|---|
| Ros | Dexterity + amount (`champion_adjust_statistic`) |
| Ku | Strength + `p / 35 + 5` |
| Dane | Wisdom + amount |
| Neta | Vitality + amount |
| Antivenin | Ends the poison |
| Mon | Stamina + `max_stamina / d` |
| Ya | Champion shield + `amount × 1.5` (a quarter of it when the shield is above 50). Event 72 removes it after `amount²` ticks. |
| Ee | Mana + `2 × amount − 8`, at most 900. Above the maximum, half of the excess is lost. |
| Vi | Health + `max_health / d`. Heals wounds at random. |
| Water | Water +1600 |

Health and stamina stop at their maximum. Sound 8 plays.

## Time effects

The main loop calls `champion_apply_time_effects()` every 64 ticks (16 while the party sleeps).

1. Scents that are not on the party square lose 1 strength. A first scent at strength 0 is deleted.
2. A chance value is made from bits 6–8 of the game time (0–112).
3. For each living champion that is not the candidate:
   - Mana: when the wizard and priest levels plus wisdom are above the chance, mana increases by `max_mana / 40 + 1` (doubled while sleeping) and stamina decreases by `max(7, 16 − levels)` times that gain. Mana above the maximum decreases by 1.
   - Temporary experience decreases by 1 in each skill.
   - Food and water decrease. Fed champions regain stamina; starving champions (below −512) lose stamina. Food and water stop at −1024.
   - Health: when stamina is at least a quarter of the maximum and vitality + 12 is above the chance, health increases by `max_health / 128 + 1` (doubled while sleeping, +50 % + 1 with the Ekkhard Cross).
   - Every 256 ticks (64 while sleeping), each current statistic moves toward its maximum.
   - When the party is awake and no creature attacked for 60 ticks, the champion turns to the party direction.

## Actions

The code is in `src/engine/action.c`. The rules follow `action_is_action_performed` and its helpers in KAOS range 8 (MENU.C). The per-action values come from the action tables ([tables.md](tables.md#actions)).

`action_did_click_trigger_action(list_index)` runs commands 113–115. It adds the action defense to the champion, runs the action, stores the action in `action_index`, and closes the action menu.

`action_is_action_performed(champion, action)` runs one action against the square in front of the champion:

1. The target group is the group on that square (`target_group` of `action_is_action_performed`).
2. Stamina cost is `random_bit() + stamina`. For skills 16–19 and the wizard skill, the mana cost of a spell action is `7 − min(6, skill level)`.
3. The action runs (table below).
4. The action hand is disabled for the action ticks (event 11, `magic_disable_action`). Stamina decreases. The action skill gets the experience. A value of 0 skips each step.

| Action | Effect |
|---|---|
| Chop, Kick, Swing, Hack, Berzerk, Bash on a closed door | Bashes the door with the action-hand strength (`event_is_door_destroyed`, destruction after 2 ticks). Disabled ticks become 6. |
| Melee actions (the above without a door, Punch, Stab, Thrust, Jab, Parry, Disrupt, Melee, Slash, Cleave, Stun) | `is_melee_performed`. A miss of the target (no group, no creature in reach) halves the experience and the disabled ticks. |
| War Cry, Blow Horn, Calm, Brandish, Confuse | `is_group_frightened`. Confuse uses a charge. War Cry plays sound 17, Blow Horn sound 18. |
| Flip | Prints heads or tails (text 0x45 or 0x46) by a random bit. |
| Climb Down | With the rope, the party goes into the pit in front (no group there) without fall damage. Else no disabled ticks. |
| Freeze Life | A blue magical box adds 30 freeze ticks, a green box 125; the box is used up. Other objects add 70 and use a charge. The maximum is 200. |
| Fireball (150), Dispell (harm non-material, 150), Lightning (180), Spit (fireball, 250), Invoke | Shoots the explosion with the attack as kinetic energy. Invoke: attack `100 + random(128)`, spell by `random % 6`: 0 poison bolt, 1 poison cloud, 2 harm non-material, else fireball. With less mana than the cost, attack becomes `max(2, mana × attack / cost)` and all mana is used. A failed cast halves the experience. Each uses a charge. |
| Shoot | The bow (class 16–31) or sling (class 32–47) in the action hand shoots the arrow (class 10) or stone (class 11) of the ready hand. Kinetic energy is the sum of both weapons. Attack is `(shoot level + launcher attributes low byte) × 2`. Step energy is the launcher class minus 16 or 32. Without ammunition: `g_session.action_damage = −2`, no experience. |
| Spellshield, Fireshield | `magic_cast_party_spell` for 280 ticks with mana. Success uses a charge. Failure quarters the experience and halves the disabled ticks. |
| Fluxcage | `projectile_fluxcage_action` on the square in front. |
| Heal | While mana remains: health + `min(10, heal level)`, mana − 2, experience + 2 (start 2). |
| Light | Light power 2 now, event 70 takes it back after 2500 ticks. Uses a charge. |
| Window | Thieves eye (event 73) for `random % (level + 8) + 5` ticks. Uses a charge. |
| Throw | `is_object_thrown` from the action hand. The event 11 gets byte 6 = 2: when it runs, the next weapon of the quiver goes to the action hand. |
| Fuse | `projectile_fuse_action` on the square in front of the party. |

Block, Hit, and the other actions only add the action defense, the disabled ticks, the stamina cost, and the experience. The champion turns to the party direction for the spell, shoot, throw, fluxcage, and fuse actions.

### Melee

`is_melee_performed` plays sound 13 and needs a group with a creature in reach of the champion cell (`group_get_melee_target`: the creature index, −1 for none). A champion in a back cell cannot reach when a living champion stands in the front cell before it: `g_session.action_damage = −1` ("can't reach"). Disrupt hits only non-material creatures. Disrupt and the weapon with icon 40 add bit 15 to the hit probability, so the hit can reach non-material creatures. `group_get_melee_action_damage` ([rules-creatures.md](creatures.md)) gives the damage, which goes into `g_session.action_damage` for the action area.

### Fear

The influence roll is `random16() % (influence level + bonus)`. The bonus and the experience are: Confuse 12 and 45, War Cry 3 and 12, Blow Horn 6 and 20, Brandish 6 and 30, Calm 7 and 35. When the roll is below the fear resistance, or the creature is fearless (15), the group stays and the experience is halved. Else an attacking group stops its attack, the group flees, and the flee delay is `(16 − fear) × 4 / movement ticks`.

### Throwing

`is_object_thrown(champion, slot, side)` throws the object of the slot. With slot < 0 it throws the leader hand object (`action_is_hand_object_thrown`, a click in the throw zones of the dungeon view); the strength then counts the object as if it were in the action hand.

1. Stamina cost: half the weight, bounded 1–10, plus half of each 10 above (`throwing_stamina_cost`). The action hand is disabled for 4 ticks.
2. Experience to the throw skill: 8 for objects, 12 for weapons, plus kinetic energy / 4 for weapons of class 0–12.
3. Kinetic energy (a byte): `level + e / 2 + random(16) + e` with `e` = strength + the weapon kinetic energy (class 0–12) or 1.
4. Attack: `bounded(40, random(32) + 8 × level, 200)`. Step energy: `max(5, 11 − level)`.
5. The projectile starts in the front cell of the side and flies in the party direction. The party cannot move in that direction for 4 ticks.

### Charges

`decrement_charges` removes one charge from the action-hand object: weapon bits 10–13, armour bits 9–12, junk bits 14–15 of word 1.

## Champion events

| Event | Handler | Effect |
|---|---|---|
| 11 | `enable_champion_action` | The action hand works again. The action defense of `action_index` ends. After Shoot with an empty ready hand, the first matching ammunition of the quiver (slot 12, then 7–9) goes to the ready hand. Byte 6 ≠ 0 then moves the next weapon of the quiver to slot `byte 6 − 1` (`action_refill_from_quiver`). |
| 12 | `hide_damage_received` | Hides the damage number of the champion. |
| 13 | `process_vi_altar_rebirth` | Cell byte = step. Step 2: rebirth explosion, next step after 5 ticks. Step 1: the bones of the champion (junk icon 147, champion index in the charges, in the event cell) disappear, next step after 1 tick; without bones the rebirth stops. Step 0: `champion_resurrect`. |
| 75 | `champion_poison` | One poison step: the poison event count decreases, then `champion_poison(champion, word 6)`. |

## Save files

`champion_pack` and `champion_unpack` convert between the `Champion` struct and the original 0x140-byte big-endian record. The portrait pointer (offset 0x114) is not in the struct.

## Not in this module

- Drawing: status boxes, damage numbers, the inventory panel, and portraits (`status.c`, `panel.c`).
- Spells and creature attacks on champions (`magic.c`, `group.c`).
- Scent creation on party moves (`move.c`).
- `rec_find_unused_thing` does not free discarded things yet (`record.c`).
- Chest closing when the open chest leaves the action hand, and the light palette update after a torch or Illumulet change.

## Tests

`tests/differential/test_champion.py` runs these checks with `build/libdm.dylib`. The emulator tests patch the drawing and message functions of the original to `rts`.

- `champion_pack` writes the original offsets and `champion_unpack` reverses it.
- Object weight and icon, skill level, maximum load, movement ticks, dexterity, strength, wound defense, pending damage, damage to all, stamina decrease, experience gain with level-ups, and time effects give the same champion records, random seed, and globals as the original functions in `dmtools.emu`.
- Scenario tests in `Dungeon.DAT`: recruit the first mirror champion and accept it, eat food, and take damage until the party dies (objects and bones on the party square).

`tests/port/test_actions.py` compares `action_is_action_performed` (stamina, experience, disabled ticks, heal, action damage, random seed) for all actions without a target, and `group_get_melee_action_damage` (hit and damage), with the original functions in `dmtools.emu`. Scenario tests: a sword attack hurts a skeleton and trains the skill, a back-row champion cannot reach, a throw creates a projectile and event 11 refills the hand from the quiver, event 11 ends the action defense, poison steps, event 12, and the vi altar rebirth.

The tests skip when `game/KAOS.FTL` or `game/Dungeon.DAT` is missing.
