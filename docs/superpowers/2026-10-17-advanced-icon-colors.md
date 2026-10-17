# Advanced mode: action icons keep their colors on every level: PRD

Date: 2026-10-17. Branch: `feature/2026-10-17-advanced-icon-colors`. Status: implemented (todo pass of
2026-10-17, branch `feature/2026-10-17-todo`).

## Goal

The advanced "own colors" action icons (`docs/manual.md:150`) have the same colors on every level and in every
light. Vanilla does not change.

A claim without a `file:line` source is an assumption; the plan task that relies on it checks it first.

## Current state

- The action area lies on the viewport rows, so it draws with the dungeon palette (`ui_set_palette`,
  `src/ui/ui.c:152-164`; `plat_present`, `src/platform/platform.c:170-174`).
- In vanilla the icons use only colors 0 and 4 (`TABLES_CHANGES_ACTION_ICON`, `src/ui/menus.c:69-79`); these
  two are the same in all six light palettes (assumption from a dump of the table, checked first), so vanilla is
  correct.
- The advanced icons use the other colors, which change with the light level and with the creature colors 9
  and 10 of a map (`src/ui/view.c:885`).

## Requirements

- A pixel of `g_screen.px` gets a flag bit `0x10` that selects the interface palette half for that pixel. The
  advanced icon pixels set it. `plat_present` and `video_get_color` (`src/ui/video.c:15-19`) read it. The
  vanilla bytes do not change.
- The mode gate is the gate of the "own colors" feature (`g_advanced_features`, true outside vanilla,
  `src/ui/select.c:78`), so the turnbased mode gets it too.
- Assumption, checked first: no other reader of `g_screen.px` uses a pixel without the `COLOR_INDEX_MASK`.

```gherkin
Feature: Fixed icon colors
  Scenario: A dark level with creature colors
    Given the advanced mode on level 12 with a dim light
    When the action area shows the icons
    Then the icons have the colors of the interface palette, as on level 1 in full light
```

## Invariants

- In vanilla, `g_screen.px` holds no value above 15.

## Known consequences

- The advanced screen hashes in `tests/golden/` change; the plan lists them under `Weakened assertions` for the
  owner's approval.

## Decisions

- Implemented although it changes colors: it changes only the advanced "own colors" icons; the vanilla icons
  are already correct (owner, 2026-10-17).
- The flag bit, not a box of the screen: no signature change of `video_get_color`, and it works for any pixel.

## Tests

- None (UI); the owner tests it manually.

## Docs

- `docs/todo.md`: remove the item.
