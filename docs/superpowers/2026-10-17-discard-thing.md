# Discard a thing when the records run out: PRD

Date: 2026-10-17. Branch: `feature/2026-10-17-discard-thing`. Status: implemented (reviewed by one `prd-reviewer`
pass, as part of the todo pass of 2026-10-17).

## Goal

When no record of a type is free, the port frees one as the original does. Today `rec_find_unused_thing`
(`src/engine/record.c:193-209`) returns `THING_NONE`, and each caller does nothing.

A claim without a `file:line` source is an assumption; the plan task that relies on it checks it first.

## The original

- `get_unused_thing` (KAOS.c:8969-9008) calls `get_discarded_thing(type)` when nothing is free (KAOS.c:8998);
  it clears the returned record and sets its next word to `0xfffe`.
- `get_discarded_thing(type)` (`0x10af2e`, KAOS.c:8846-8964):
  - Explosions (type 15) are never discarded.
  - Map order: start at `g_last_discarded_thing_map_index[type]` (`0x101c62`, 16 bytes); skip the party map,
    wrap, and scan the party map last. After the party map, or with fewer than 2 maps, store the map and return
    none.
  - Squares: column by column, only squares with things; on the party map skip the squares with |dx| ≤ 5 and
    |dy| ≤ 5.
  - A disabled sensor is skipped; any other sensor protects its square.
  - Group: skipped with the no-discard bit (`GROUP_NO_DISCARD_BIT`, `src/engine/record.c:84`) or with
    possessions; else drop the possessions and delete the group.
  - Projectile: delete its event, unlink, delete.
  - Weapon, armour, junk (byte 3 bit 7) and potion (byte 2 bit 7): skipped when important; else removed from
    the map through the move rule.
  - Any other type is returned without removal.
  - At the end: restore the current map, store the last map, return the thing.

## Requirements

- The new module `engine/discard.c` (prefix `discard_`) holds `discard_get_unused_thing`; it needs group,
  projectile and move functions, so it lives above `record.c`.
- The eight callers of `rec_find_unused_thing` call it: `src/engine/champion.c:878`, `src/engine/magic.c:215`,
  `src/engine/projectile.c:107,165,649,907`, `src/engine/group.c:508,608`.
- `game_start_new` and the game load reset the last-map array to 0. Deviation: the original never resets it,
  but the port can change the dungeon in one run (DM and CSB).

```gherkin
Feature: Discard a thing
  Scenario: Records run out
    Given no free record of a type
    When a rule needs a record of that type
    Then the port discards a thing on a far square, as the original does
```

## Known consequences

- When the records run out, objects, groups and projectiles on far squares disappear, as in the original.

## Decisions

- The differential test runs the original `get_discarded_thing` (`0x10af2e`) with its own removal functions,
  not stubs (game rules stay real, c-decomp-port skill), and compares the returned thing, the last-map array
  and the dungeon state.

## Tests

- The differential test above.

## Docs

- `docs/architecture.md`: the module `engine/discard.h`.
- `docs/formats/dungeon.md`: the last-map reset deviation.
- `docs/todo.md`: remove the item.
