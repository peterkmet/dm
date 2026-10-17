# Advanced mode: the action damage shows twice as long: PRD

Date: 2026-10-17. Branch: `feature/2026-10-17-advanced-damage-time`. Status: reviewed (one `prd-reviewer` pass,
as part of the todo pass of 2026-10-17); the plan is not written yet.

## Goal

In the `advanced` mode the damage that a champion deals stays in the action area twice as long (owner's wish,
2026-10-16). The `vanilla` and `turnbased` modes keep the original time.

## Current state

- The damage shows for one tick: `menus_refresh_action_area` draws it, and the next refresh draws the icons
  (`src/ui/menus.c:133-162`, `src/ui/gameloop.c:275-280`). One tick is 12 frames (`WAIT_FOR_INPUT_FRAMES`,
  `src/ui/gameloop.h:22`).
- While the damage shows, the icons do not take clicks (`src/ui/command.c:625-627`,
  `src/control/control_actions.c:385-389`).

## Requirements

- In the `advanced` mode (`g_session.mode == GAME_MODE_ADVANCED`) the damage shows for two ticks. A timer in
  `ui/menus.c` holds the second tick; the save file does not hold it.
- A click on the action area during the second tick first draws the icons and then handles the click, so the
  champion can act at the same tick as in vanilla.
- `menus_reset` clears the timer; sleep and a shown candidate end the hold (`src/ui/menus.c:136-148`).

```gherkin
Feature: Longer action damage
  Scenario: The advanced mode
    Given the advanced mode
    When a champion hits a creature
    Then the action area shows the damage for two ticks
    And a click on the action area in the second tick shows the icons and acts at once
```

## Decisions

- Only `advanced`, not `turnbased`: the time stands still between turns, so a longer display gives the program
  player nothing.
- The click rule keeps the input timing, because the game rules are the same in all modes
  (`docs/manual.md:136`).
- Not in scope: the damage on the champion box (`HIDE_DAMAGE_TICKS`, `src/engine/champion.c:47`).

## Tests

- None: a display rule of `ui/`; the owner tests it manually.

## Docs

- `docs/manual.md`: the advanced features list.
- `docs/todo.md`: remove the item.
