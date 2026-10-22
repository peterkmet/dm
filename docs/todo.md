# Ideas for later

These items are not planned.

## Small cleanups

- `light_depths` (`src/engine/explored.c`): the darkest light level hides the two far rows, the next light level one row. Write a finer rule if the LLM player needs it.


## Documentation

- CSB: the meaning of the `AN` count of the animation files (the player skips `AN`).
- CSB: the rule differences for renamed objects (for example STORM, RA BLADE).
- CSB: whether DM v3.60 reads the empty `CSBGAME.DAT` of the DM disk.

- The amounts and durations of the spells Light, Darkness, Thieves Eye, Invisibility, Party Shield, Magic Torch,
  Footprints, Zokathra, and Fire Shield are not in [Creature, projectile, and magic rules](rules/creatures.md#spells).
- `Graphics.DAT` has no format page in `docs/formats/`. The header is in [KAOS module map](reference/kaos-map.md#graphicsdat); the decoding is in `src/data/graphics.c`.
