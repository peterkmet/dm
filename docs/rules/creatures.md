# Creature, projectile, and magic rules

The creature rules of the port: creature groups and their AI, creature attacks, damage and death, group generators, projectiles, explosions, fluxcages, the fuse action, and spell effects. The code is in `src/engine/group.c`, `src/engine/projectile.c`, and `src/engine/magic.c`. The rules follow KAOS range 4 (GROUP.C), range 7 (PROJEXPL.C), and range 8 (MENU.C). See [KAOS ranges 4–6: symbols and subsystem notes](../reference/symbols/range-4-6.md) and [KAOS ranges 7–9: symbol notes](../reference/symbols/range-7-9.md) for the record layouts and the original addresses. See [Movement, sensors, and timeline rules](movement.md) for the game loop and the timeline dispatch.

## Records

| Thing type | Size | Accessors | Content |
|---|---|---|---|
| 4 group | 16 bytes | `record.h` | Next thing, possessions, creature type, cells byte, 4 health words, flags in word 14 (direction, last creature index, behavior). |
| 14 projectile | 8 bytes | `record.h` | Next thing, flying thing with its cell, kinetic energy, attack, timeline event index. |
| 15 explosion | 4 bytes | `record.h` | Next thing, attack, type in bits 0–6, centered flag in bit 7. |

The flying thing of a spell projectile is an explosion thing `0xff80 + type` (`EXPLOSION_THING(type)`), not a record.

Creature types come from the creature info table (`tables_get_creature_info`).

## Active groups

The party map has active groups. Other maps do not.

- `group_add_all_active_groups()` runs when the party enters a map. Each group on the map gets an `ActiveGroup` entry, loses its behavior events, and starts to wander (event 37 after 1 tick).
- `group_remove_all_active_groups()` runs when the party leaves a map. Each record gets back its cells byte and one direction. A group that flees or attacks wanders again.
- On the party map, the cells byte of the group record holds the index of the active group. `group_get_active(group)` returns the entry; a cells byte beyond the table gives a free entry. The fields of the entry are in [Game state](game-state.md#active-creature-groups).
- A group that enters the party map from another map becomes active. A group that leaves the party map becomes inactive.
- More than `g_max_active_group_count` active groups is system error 71 (`0x47`).

## Group events

The timeline sends events 29–41 to `group_process_event(event)`. Other rules send a reaction with a negative type through `group_process_reaction_event(at, reaction)`. The reaction becomes event 32 + reaction after a short delay.

| Event | Reaction | Meaning |
|---|---|---|
| 29 | -3 `REACTION_DANGER_ON_SQUARE` | Danger on the square: a closing door, a poison cloud, a fluxcage around Lord Chaos. |
| 30 | -2 `REACTION_HIT_BY_PROJECTILE` | A projectile hit a creature of the group. |
| 31 | -1 `REACTION_PARTY_IS_ADJACENT` | The party is next to the group, or a champion hit the group. |
| 32 | — | Aspect update of the whole group. |
| 33–36 | — | Aspect update of creature 0–3. |
| 37 | — | Behavior update of the whole group. |
| 38–41 | — | Behavior update of creature 0–3 (attacking group). |
| 60, 61 | — | A group waits off the map until its square is free (61 plays the teleporter sound). |

The type of an aspect event is the type of its behavior event minus 5. When the next aspect update comes before the next behavior update, the group adds the aspect event. Byte 8 of the event keeps the delay to the behavior update.

Off the party map a group only handles events 32, 33, 37, and 38. It moves one square in a random direction when it can. It adds the next event 37 after `max(16 × |map - party map|, 2 × movement ticks)` ticks.

While Freeze Life is active, a group (not Lord Chaos) adds its event again 4 ticks later. Lord Chaos does nothing after the game is won.

## Group behavior

Bits 0–3 of group word 14 hold the behavior.

| Value | Behavior | Rule on event 37 |
|---|---|---|
| 0 | Wander | Sees the party: attacks when the party is in attack range and in line, otherwise approaches. Does not see the party: moves toward the smell of the party, otherwise moves randomly or stays. |
| 2 | Wander, no approach | Like wander, but does not follow the smell and does not approach. |
| 3 | Wander, no smell | Like wander, but does not follow the smell. |
| 5 | Flee | Moves away from the party. Without sight of the party, it sometimes moves home. The flee ends after the flee delay. |
| 6 | Attack | Each creature gets its own events 38–41. |
| 7 | Approach | Moves toward the party at double speed. Without sight of the party, it moves to the target square, then wanders. |

### Sight

`visible_party_distance` returns the square distance to the party, or 0.

1. A creature without side attack sees only into the 90° cone in front of it (`group_is_destination_visible`).
2. The sight range comes from the creature info. Darkness reduces it by `palette index / 2`, unless the creature has night vision. A creature that does not see invisible cannot see an invisible party.
3. Outside the sight range the creature can still notice the party by a random roll. Next to the party, the roll also adds the hearing and smell ranges.
4. A wall, a solid fake wall, or a closed door that is not see-through on the line from the party to the creature blocks the view.

A creature smells the party directly within half the smell range when no wall and no solid fake wall blocks the line. Otherwise it follows the scent trail of the party (`g_party.scents`).

### Movement

- The creature moves when half its movement ticks have passed since the last move. Otherwise the group waits.
- `group_get_directions_to` returns a `DirectionPair`: a primary direction and a second direction. Random numbers select between equal choices.
- `test_movement` blocks walls, stairs, open pits (except for levitating creatures), closed fake walls, the party square, other groups, and closed doors that are too low for the creature. A wary creature avoids a teleporter to a map that does not allow its type. Lord Chaos does not enter a fluxcage.
- A creature never turns around immediately. It turns left or right first.
- Lord Chaos sometimes jumps two squares over a free square.
- `move_get_move_result` moves a group through the square chain of every mover (the rule table in [Movement, sensors, and timeline rules](movement.md)): teleporters with the creatures scope, pits, and stairs. The group hooks are `group_turn_by_teleporter`, `group_is_killed_by_fall`, `group_remove_after_move`, and `group_enter_square`. When the destination is occupied, the group waits with event 60 or 61 and tries again every 5 ticks.

### Attack

An attacking creature (events 38–41) turns to the party, moves to the front cells of its square, then attacks. After an attack it waits its attack ticks plus a random part.

`is_creature_attacking` makes one attack:

- Attack range 1, or (half of the time) a party next to the group: a melee hit on the champion in the cells that the creature reaches first. A creature with "attack any champion" picks a random living champion.
- A Giggler steals objects from random slots of the champion while the champion fails a dexterity and luck check. Then the Giggler often flees.
- Longer range: the creature shoots a spell projectile with step energy 8. The creature type selects the spell: slime for a Swamp Slime, lightning or open door for a Wizard Eye, a random spell for a Vexirk or Lord Chaos, poison cloud or fireball for a Materializer, otherwise a fireball.

`group_get_champion_damage(group, champion)` gives the melee damage:

1. A sleeping party wakes up. The champion gets parry experience.
2. The champion dodges by dexterity against `random(32) + creature dexterity + 2 × map difficulty + dodge bonus - 16`. Darkness and invisibility increase the dodge bonus. Luck (60 %) can also avoid the hit.
3. The damage starts from `random(16) + creature attack + 2 × map difficulty - 2 × parry skill`, then gets a random spread.
4. `champion_add_pending_damage` applies armour by the attack type of the creature. A hit plays the damage sound of the champion.
5. A poison attack poisons the champion half of the time.


## Damage and death

`group_damage_creature(group, creature, at, damage)` hurts one creature. It returns `OUTCOME_NO_CREATURE_KILLED` (0), `OUTCOME_SOME_CREATURES_KILLED` (1), or `OUTCOME_ALL_CREATURES_KILLED` (2).

- An invulnerable creature takes no damage.
- A creature that dies leaves smoke (explosion type 40). The smoke attack depends on the creature size.
- When the last creature dies, the group drops its possessions and the record becomes free.
- When one creature of several dies, the other creatures move down one index. The dead creature can drop its fixed possessions. An attacking group can start to flee, by its fear resistance.

`group_damage_all_creatures(group, x, y, attack)` hurts each creature with `attack ± attack / 8`.

### Melee hits

`group_get_melee_action_damage(melee)` resolves a melee hit of a champion on one creature. It returns the damage, or 0 for a miss.

1. A dead champion does nothing. The champion never hits a creature with dexterity 255 (`CREATURE_NEVER_HITS`). The champion hits a non-material creature only when the hit probability has bit 15 (`HIT_PROBABILITY_ALWAYS_HITS`).
2. The defense is `(random16() & 31) + creature dexterity + 2 × map difficulty + 2 × g_party_light_level - 16`. The champion hits when the defense is below `champion_get_dexterity`, when `random_2bits()` is 0, or when `champion_is_lucky(75 - hit probability)` is true.
3. The base damage is the strength of the action hand plus `random(strength / 2 + 1)`, times the damage factor / 32, plus `random16() & 31`, minus the armour. The armour is `(random16() & 31) + creature defense + 2 × map difficulty`. The Diamond Edge removes a quarter of the armour, the Hardcleave an eighth. A champion without strength has base damage 0.
4. A base damage of 1 or less uses the weak hit rule (`weak_hit_damage`), which can give 0. A random spread then sets the final damage (`melee_hit_damage`). The Vorpal Blade halves it against a material creature.
5. When `random16() & 63` is below the skill level, the damage becomes `2 × damage + 10`.
6. On a hit, the creature takes the damage (`group_damage_creature`), the skill gets `creature experience × damage / 16 + 3` experience, and the stamina decreases by `4 + random_2bits()`. On a miss, the stamina decreases by `2 + random_bit()`.
7. When a creature of the group survives, the group gets the reaction party-is-adjacent.

### Possessions

- Fixed possessions: Skeleton, Stone Golem, Trolin, Animated Armour, Rock Pile, Pain Rat, Screamer, Magenta Worm, and Red Dragon drop fixed objects from a KAOS list. An entry with bit 15 drops half of the time. The objects of an Animated Armour are cursed.
- Carried possessions: objects that the group picked up (for example a caught dagger) fall on random cells of the square.

### Other damage

- A closing door hits the creatures on its square with 5 damage. The survivors react to danger.

## Group generators

Event 5 (corridor) on a group generator sensor calls `group_generate_from_sensor(sensor, x, y)`:

- Sensor word 4 bits 7–10: the creature count. With bit 3 of word 4 set, the count is random.
- Sensor word 6 bits 4–7: the health multiplier. 0 means the map difficulty.
- Each creature gets `base health × multiplier + random(base health / 4 + 1)` health.
- The group enters the square with a random direction and plays a buzz sound.

`group_get_generated` returns `THING_NONE` when the group becomes active on the party map, like the original. The group is on the square in that case; `map_find_thing((Location){x, y}, THING_GROUP)` finds it.

Event 65 enables a generator again after its delay (see [Movement, sensors, and timeline rules](movement.md)).

## Projectiles

`projectile_create(launch)` puts a projectile in a cell of a square. The `ProjectileLaunch` struct holds the thing, the square, the cell, the direction, the kinetic energy, the attack, the step energy, and the launcher flag. It adds event 48 after 1 tick, or event 49 for a launcher projectile. Event 48 ignores impacts on the first step. Without a free projectile record, an object drops to the square.

Event 48 or 49 (`projectile_process_move_event`) moves a projectile one cell forward:

1. Event 49 tests the party and the group on the square. A hit ends the projectile.
2. Subtract the step energy from the kinetic energy and from the attack. A projectile without kinetic energy drops to the square.
3. A projectile in a front cell leaves the square. Stairs, a wall, or a closed fake wall in front stop it with an impact.
4. In a door square, a closed door stops it with an impact.
5. The next event comes after 1 tick on the party map, 3 ticks on other maps.

`has_impact_occurred(site, projectile, outcome)` runs one impact. `site` holds the impact type, `at`, `target`, and the cell. `outcome` receives the damage outcome. The projectile stops at the square `at`. The creature or door hit is on the square `target`. `target` is `at`, except when the party or a group steps onto the projectile square. Then `target` is the square they leave. The impact types:

- Party: the champion in the cell takes `projectile_get_impact_attack` damage on the head or torso. Poison can follow.
- Creature: damage `impact attack × 64 / creature defense` plus poison. A non-material creature takes damage only from Harm Non-Material. A Black Flame gains health from a fireball. Some creatures catch a thrown dagger, arrow, slayer, poison dart, or throwing star.
- Door: an Open Door spell toggles a door with a button. A door grate can let a projectile pass. Otherwise the projectile bashes the door.
- A Ful bomb or Ven bomb flask explodes into a fireball or a poison cloud.
- A spell projectile becomes an explosion of its type. The kinetic energy becomes the attack of the explosion: half for a lightning bolt, a quarter for a poison bolt.

`projectile_get_impact_attack` returns an `Impact`: the attack, the attack type, and the poison attack of the hit.

### Launchers

Event 6 (wall) on a launcher sensor (type 7–10, 14, or 15) in the event cell calls `projectile_trigger_launcher`. It shoots one or two objects or explosions from the square in front of the wall. Types 14 and 15 shoot objects that lie in the launch cells. Sensor word 6 holds the kinetic energy (bits 4–11) and the step energy (bits 12–15). The attack is 100.

### Champion projectiles

`projectile_shoot_from_champion` shoots from the front cell of the side of the champion. The party cannot move in that direction for 4 ticks.

## Explosions

`explosion_create(thing, attack, at, target, cell)` puts an explosion on the square `at` and adds event 25 after 1 tick (5 for the first rebirth step). A fireball or lightning bolt damages the party or the group on the target square immediately. The damage is `random(attack / 2 + 1) + attack / 2 + 2`, half for lightning. Fire resistance reduces the damage to creatures.

Event 25 (`explosion_process_step_event`):

| Type | Effect |
|---|---|
| 0 fireball | Breaks a door. |
| 2 lightning bolt | Breaks a door with half damage. |
| 3 harm non-material | Hurts non-material creatures. It hurts a Materializer only while the Materializer attacks. |
| 7 poison cloud | Hurts the party or the group each step. The attack fades by 3 per tick until it is 5 or less. |
| 40 smoke | Fades by 40 per tick while the attack is 56 or more. |
| 100, 101 rebirth | Step 1 becomes step 2 with a strong explosion sound. |

Then the explosion disappears.

### Fluxcage and fuse

- `projectile_fluxcage_action(x, y)` puts a fluxcage (type 50) on a square that is not a wall or stairs. Event 24 removes it after 100 ticks. When this fluxcage is the third one around Lord Chaos on an adjacent square, Lord Chaos senses danger.
- `projectile_fuse_action(x, y)` creates a Harm Non-Material explosion with attack 255. Lord Chaos in 4 fluxcages sets `g_fuse_sequence_requested`. Otherwise Lord Chaos escapes through a free side to an open square.

## Spells

`magic_cast_champion_spell(champion)` casts the spell of the champion symbols. It returns `SPELL_CAST_SUCCESS` (1), `SPELL_CAST_FAILURE` (0), or `SPELL_CAST_FAILURE_NEEDS_FLASK` (3).

1. A dead champion or an unknown symbol string fails. An unknown string prints "meaningless spell".
2. The required level is `power + base level of the spell`. Each missing skill level is a draw against wisdom. A failed draw gives reduced experience and prints "needs more practice".
3. The spell kind selects the effect:
   - Potion: fills an empty flask in a hand. Potion power is `40 × power + random(16)`.
   - Projectile: the champion turns to the party direction and shoots the explosion. Kinetic energy is `(power + 2) × (4 + 2 × skill level)`, bounded to 21–255. Open Door counts the skill level twice. Step energy depends on the maximum mana.
   - Other: Light, Darkness, Thieves Eye, Invisibility, Party Shield, Magic Torch, Footprints, Zokathra, Fire Shield. Each effect adds its end event (70–74, 77–79; [Movement, sensors, and timeline rules](movement.md#timeline-dispatch)).
4. The champion gets the experience and an action delay (event 11).

`magic_click_cast_spell` casts for the magic caster and clears the symbols, except when a flask is missing.

## Kept quirks

- `wander_step` reads the party-blocked result of the last step test, also when it skips the test. The value survives between turns; `group_reset` does not clear it.
- A group turn remembers its square and group thing (`last_turn` in `group.c`) until the next group turn. A group that becomes active plays its flip sound at this square, and a group off the party map tests teleporters with this thing.

## Fixed original bugs

Recorded in [Dungeon.DAT format](../formats/dungeon.md#deviations-from-the-original).

## Tests

`tests/differential/test_creatures.py` runs these tests:

- Differential tests against the original code in `dmtools.emu`: directions to a destination, sight cone, poison resistance, champion damage by a creature, projectile impact attack, launcher objects.
- Scenario tests on the loaded dungeon: a group activates and comes next to the party, a group attacks the party, a skeleton dies and drops its possessions and smoke, a fireball flies and explodes on a wall, and spell casts (light, potion, meaningless spell, party shield, fireball).

## Related rules

- Champion actions and the events 11, 12, 13, and 75 are in [Champion rules](champions.md#actions).
