# Advanced and turn-based modes: plan

Approved by the user on 2026-10-04 (design in chat, no PRD). Code style: `docs/coding-standard.md`.

## Modes

- `build/dm --mode vanilla|advanced|turnbased`. Default `vanilla`. Game rules are the same in all modes.
- `advanced`: display and comfort features. F4 turns them on and off during the game.
- `turnbased`: advanced plus control through a Unix socket. The game waits for a turn.

## Advanced features (from PyDM)

1. Automap: `port/src/automap.c`. Key M or Tab shows a top-down map in place of the viewport. Only explored squares. Walls, doors, stairs, pits, teleporters seen, party with direction. Explored state is saved as an extension block after the original save data.
2. Rune symbols on spell scrolls (`ui_champion.c`).
3. Smooth projectile flight: interpolate the drawn position between ticks (`view.c`). No rule change.
4. Coloured action icons (`ui_menus.c`).
5. Item stat meters in the object description panel (`ui_champion.c`).
6. Debug read-out on F3: level, X/Y, facing, light. Display only, no cheats.
7. Escape closes an open inventory first. With no inventory open, it freezes the game as in vanilla. Vanilla keeps the original: Escape always freezes.

## Turn-based mode

- `build/dm --mode turnbased --control <socket path>`. The window shows the game. The game does not advance until a turn arrives. A turn runs one action, then advances a fixed number of ticks, redraws, and answers with the new state.
- Protocol: one JSON object per line over a Unix socket. Request `{"cmd": "...", ...}`, response `{"ok": true, ...}` or `{"ok": false, "error": "..."}`.
- C side: `port/src/control.c` (socket, line protocol), `port/src/state_json.c` (state, map, view as JSON).
- MCP server: `src/dmtools/mcp_server.py`, run with uv. Tools: `get_state`, `get_map`, `get_view`, `act(action, args)`, `wait(ticks)`, `screenshot`.
- The LLM sees only what a player sees, plus the explored map.

## Order

1. Now, in parallel with the vanilla Step 15: automap module, control and state JSON modules, MCP server (new files only).
2. After Step 15: wire the advanced features and the modes into `view.c`, `ui*.c`, `command.c`, `gameloop.c`, `main.c`.
3. Tests: unit tests per module, an integration test with a headless game and scripted MCP turns, a manual run where Claude Code plays while the user watches.
