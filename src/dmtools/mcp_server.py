"""MCP stdio server that plays Dungeon Master through the turn-based control socket.

Start the game with `build/dm --mode turnbased --control <socket>`, then run `uv run dm-mcp`.
The socket path comes from the environment variable DM_CONTROL (default ~/.dm/control.sock).
"""

import tempfile
from pathlib import Path

from mcp.server.mcpserver import Image, MCPServer

from dmtools.control_client import request

INSTRUCTIONS = """You play Dungeon Master (1987), a real-time dungeon crawler, in turn-based mode.
The game time stands still between your calls. Each `act` runs one player input, then the game
runs until the party can move again (2 to 64 game ticks) and returns
the new state and view. `wait` lets time pass: to rest, to let an action recover, or to let a
door finish opening.

Goal: explore the dungeon, find the Firestaff, defeat Lord Chaos. First recruit up to four
champions from the mirrors in the Hall of Champions on the first level.

How to play:
1. Look: `get_view` shows the squares you can see in front (depth 0 is your own square, depth 1
   the square ahead; nothing behind a wall or a closed door), `get_map` shows the explored map,
   `get_state` shows the champions.
2. Walk with move_forward, move_back, move_left, move_right (strafe), turn_left, turn_right.
   Walls, closed doors, and creatures block you; the state does not change then.
   Fewer calls: `act_many` runs a list of actions, `goto` walks to an explored square. Both stop
   early when something happens (damage, a creature comes into view, a message, a failed step).
3. Recruit: face a wall whose "front_face" has "mirror": true at depth 1, act click_wall_ahead, then
   look at the "candidate" in the state and act recruit
   with choice "resurrect" (keeps the skills; the best choice) or "reincarnate" (trades skill
   levels for statistics), or "cancel".
4. Objects: the leader has one hand for moving objects ("leader_hand"). take_object picks up the
   top object of a pile (cells in get_view "piles"), drop_object puts it down, put_in_slot or
   take_from_slot swaps the hand with a champion slot (open_inventory first for slots other than
   the hands). Eat and drink: hold the food in the leader hand, open the inventory of the
   champion who eats, then act eat.
5. Fight: choose_action with the champion and the index of an action in its "actions" list
   (the actions of the object in its action hand, slot 1). A champion with "action_disabled"
   must wait. A creature must be in the square ahead for melee.
6. Magic: the "spell" state shows the caster. add_rune with symbol 0-5 (power, element, form,
   alignment in four steps; each rune costs mana), then cast. Examples: FUL IR (fireball)
   = a power rune, then symbol 3 (FUL), then symbol 3 (IR); a power rune and FUL alone make a
   magic torch (light).
7. Doors: click_wall_ahead on a door with "button": true opens or closes it. Some doors open
   with keys or levers on walls ("switch" or "lock" on a wall face).
8. Health: keep food and water above 0, rest with sleep (wake to stop) when stamina or mana
   is low. Watch "poisoned" and "wounds".
9. Save often with save_game. After game over ("game_over": true), only get_*, load_game, and
   new_game work.
"""

mcp = MCPServer("dm", instructions=INSTRUCTIONS)


@mcp.tool()
def get_state() -> dict:
    """The party and the champions.

    Fields: time (game ticks); party {map, level, x, y, facing, darkness 0 bright to 5 dark,
    can_move, sleeping, dead}; leader (champion index); leader_hand (the object that the leader
    moves, or null); candidate (name of a champion shown by a mirror, answer with act recruit);
    candidate: the champion a mirror shows, with the same fields as a champion, not yet in the
    party (answer with act recruit), or null;
    champions [{index, name, title, alive, health/stamina/mana [current, maximum] as the game
    shows them, food, water (below 0 is starving), load_kg [current, maximum], position in the
    party, poisoned, action_disabled, statistics {name: [current, maximum]}, skills {fighter,
    ninja, priest, wizard: {level, rank}}, wounds, slots [{slot, where, object}], actions
    (action menu of the action hand), runes (chosen runes)}]; action_menu (open menu); spell
    {caster, chosen, available [{symbol, name, mana}]}; inventory {champion, panel, chest};
    messages (message area rows).

    Slot numbers: 0 ready hand, 1 action hand, 2 head, 3 torso, 4 legs, 5 feet, 6 pouch 2,
    7-9 quiver, 10 neck, 11 pouch 1, 12 quiver 1, 13-29 backpack, 30-37 open chest.
    """
    return request({"cmd": "get_state"})


@mcp.tool()
def get_view() -> dict:
    """The squares that the party sees.

    squares: depth 0 (own square) to 3, side left/center/right, x, y, element (wall, floor,
    door, pit, stairs_up, stairs_down, teleporter), door (open, closed, partly_open, destroyed)
    and button, objects [{cell, name}], creatures {type, count}, flying (projectiles,
    explosions). The depths end at the first wall or closed door ahead. A wall has "front_face" (the
    side that faces you) and "side_face" (the side that faces your lane) when they show something:
    {facing, text, mirror, switch, lock, alcove, fountain, vi_altar, ornament}.
    piles: the objects that take_object can reach; cell 0-1 your own square (far left, far
    right), 2-3 the square ahead (near right, near left), 4 the alcove ahead.
    wall_ahead_clickable, alcove, fountain, vi_altar: what click_wall_ahead touches.
    """
    return request({"cmd": "get_view"})


@mcp.tool()
def get_map() -> dict:
    """The explored squares of the current level as text rows.

    Row index is y (north at the top), column is x. @ party, # wall, . floor, D closed door,
    d open door, < stairs up, > stairs down, O pit, T teleporter, space not explored. Walls with
    a seen feature: M mirror, L switch, K lock, V vi altar, A alcove, F fountain, ? text. North
    is y - 1, east is x + 1. Fake walls look like walls, closed pits look like floor.
    """
    return request({"cmd": "get_map"})


@mcp.tool()
def act(action: str, args: dict | None = None) -> dict:
    """Runs one player input, advances the game until the party can move again, and returns
    {ok, ticks, action_damage (choose_action only: damage, 0 miss, -1 can't reach, -2 need
    ammo), state, view}. ok false with error: nothing happened.

    Actions and args:
    move_forward, move_back, move_left, move_right, turn_left, turn_right;
    click_wall_ahead (mirror, button, lever, door button, fountain);
    take_object {cell 0-4}; drop_object {cell 0-3, 4 alcove; default 0};
    open_inventory {champion}; close_inventory;
    put_in_slot {champion, slot}; take_from_slot {champion, slot} (swaps with the leader hand);
    eat, drink (leader hand object to the champion whose inventory is open);
    choose_action {champion, index}; pass (closes an open action menu);
    set_caster {champion}; add_rune {symbol 0-5}; delete_rune; cast;
    recruit {choice: resurrect, reincarnate, cancel}; set_leader {champion};
    sleep; wake;
    swap_champions {a, b} (the two champions swap their positions, as clicks on their icons);
    look {champion?, slot?} (the eye of the open inventory on the leader hand object; with a
    slot the empty hand takes the slot object and puts it back; the response has look {name, lines}).
    """
    return request({"cmd": "act", "action": action, "args": args or {}})


@mcp.tool()
def act_many(actions: list) -> dict:
    """Runs a list of actions in order, each as one act turn, and returns {ok, ticks, ran,
    stopped, error?, game_over, state, view} after the last turn. Each item is an action name
    ("move_forward") or {"action": ..., "args": {...}}. stopped: done, damage (a champion lost
    health), creature (a creature came into view), message (a new message), game_over, or
    failed (error says why; ran counts only the turns that ran)."""
    return request({"cmd": "act_many", "actions": actions})


@mcp.tool()
def goto(x: int, y: int) -> dict:
    """Walks to square (x, y) of the current level on the shortest path over explored squares
    (floor, open doors; stairs only as the target; no pits) with turns and moves. Returns like
    act_many; stopped can also be blocked (a move failed) or moved_elsewhere (a teleporter).
    ok false: no such path."""
    return request({"cmd": "goto", "x": x, "y": y})


@mcp.tool()
def wait(ticks: int = 10) -> dict:
    """Lets ticks (1-1000) of game time pass without input; returns {ok, ticks, state, view}.
    Use it to rest, to wait for action_disabled to clear, or for doors to open."""
    return request({"cmd": "wait", "ticks": ticks})


@mcp.tool()
def save_game() -> dict:
    """Writes the game to the save file DMGAME.DAT (one save slot; the old file becomes
    DMGAME.BAK). Returns {ok}. Not possible after game over. Save before risky fights."""
    return request({"cmd": "save"})


@mcp.tool()
def load_game() -> dict:
    """Loads the saved game, also after game over. Returns {ok, state}."""
    return request({"cmd": "load"})


@mcp.tool()
def new_game() -> dict:
    """Starts a new game at the dungeon entrance, also after game over. All progress since the
    last save is lost. Returns {ok, state}."""
    return request({"cmd": "new_game"})


@mcp.tool()
def screenshot() -> Image:
    """The game screen (320 x 200 PNG) as the player sees it."""
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "screen.png"
        response = request({"cmd": "screenshot", "path": str(path)})
        if not response.get("ok"):
            raise RuntimeError(response.get("error", "screenshot failed"))
        return Image(data=path.read_bytes(), format="png")


def main() -> None:
    mcp.run()


if __name__ == "__main__":
    main()
