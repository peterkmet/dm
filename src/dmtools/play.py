"""dm-play: sends one request to the turn-based control socket and prints the response.

    uv run dm-play <cmd> [json-args]
    uv run dm-play act <action> [json-args]

The response is printed as compact JSON on one line. --brief prints a short summary instead.
The exit code is 1 when the response has "ok": false. The socket path comes from DM_CONTROL
(default ~/.dm/control.sock).
"""

import argparse
import json
import os
import sys

from dmtools.mcp_server import request, socket_path

LANE = ["here", "ahead", "ahead 2", "ahead 3"]


def square_text(square: dict) -> str:
    """One square of the view in a few words: element, door, wall face, creatures, objects, flying."""
    words = [square["element"]]
    if "door" in square:
        words.append(square["door"] + (" with button" if square.get("button") else ""))
    words += [key for key in ("mirror", "ornament") if square.get(key)]
    if "text" in square:
        words.append(f'text "{square["text"]}"')
    if creatures := square.get("creatures"):
        words.append(f"CREATURES {creatures['count']} {creatures['type']}")
    if square.get("objects"):
        words.append("objects " + ", ".join(f"{o['name']} ({o['cell']})" for o in square["objects"]))
    if square.get("flying"):
        words.append("flying " + ", ".join(f.get("name", f["kind"]) for f in square["flying"]))
    return ", ".join(words)


def state_lines(state: dict) -> list[str]:
    party = state["party"]
    flags = [flag for flag in ("sleeping", "dead") if party[flag]] + ([] if party["can_move"] else ["cannot move yet"])
    hand = state["leader_hand"]
    lines = [
        f"time {state['time']} | level {party['level']} x {party['x']} y {party['y']} facing {party['facing']} | "
        f"darkness {party['darkness']}" + "".join(f" | {flag}" for flag in flags),
        f"leader {'-' if state['leader'] is None else state['leader']} | hand {hand['name'] if hand else '-'}"
        + (f" | CANDIDATE {state['candidate']} (act recruit)" if state["candidate"] else ""),
    ]
    for c in state["champions"]:
        bars = " ".join(f"{label} {c[key][0]}/{c[key][1]}" for label, key in (("HP", "health"), ("ST", "stamina"), ("MA", "mana")))
        notes = [note for note in ("poisoned", "action_disabled") if c[note]] + ([] if c["alive"] else ["DEAD"])
        lines.append(f"{c['index']} {c['name']}: {bars} food {c['food']} water {c['water']}" + "".join(f" {n}" for n in notes))
    if state["action_menu"]:
        lines.append(f"action menu of {state['action_menu']['champion']}: " + ", ".join(state["action_menu"]["actions"]))
    if state["messages"]:
        lines.append("messages: " + " / ".join(state["messages"]))
    return lines


def view_lines(view: dict) -> list[str]:
    squares = {(s["depth"], s["side"]): s for s in view["squares"]}
    lines = [f"sides: left {squares[0, 'left']['element']}, right {squares[0, 'right']['element']}"]
    for depth, name in enumerate(LANE):
        square = squares[depth, "center"]
        lines.append(f"{name}: {square_text(square)}")
        if square["element"] == "wall":
            break
    if view["piles"]:
        lines.append("piles: " + ", ".join(f"cell {p['cell']} {p['top']}" for p in view["piles"]))
    return lines


def brief(response: dict) -> str:
    """A few lines for the player: the turn, the party, the champions, what is ahead, the messages."""
    if not response.get("ok"):
        return f"error: {response.get('error')}"
    head = " | ".join(f"{key} {response[key]}" for key in ("ticks", "game_over", "action_damage", "path") if key in response)
    lines = [head] if head else []
    if "state" in response:
        lines += state_lines(response["state"])
    if "view" in response:
        lines += view_lines(response["view"])
    if "map" in response:
        lines += response["map"]["rows"]
    return "\n".join(lines) or "ok"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="dm-play", description="Sends one request to the Dungeon Master control socket.")
    parser.add_argument("--brief", action="store_true", help="print a short summary instead of the JSON")
    parser.add_argument("cmd", help="get_state, get_view, get_map, act, wait, screenshot, save, load, new_game")
    parser.add_argument("rest", nargs="*", help="act: the action, then the args as JSON; other commands: the fields as JSON")
    options = parser.parse_args(argv)
    payload = {"cmd": options.cmd}
    rest = options.rest
    if options.cmd == "act":
        if not rest:
            parser.error("act needs an action")
        payload["action"], rest = rest[0], rest[1:]
        payload["args"] = json.loads(rest[0]) if rest else {}
    elif rest:
        payload.update(json.loads(rest[0]))
    if "path" in payload:
        payload["path"] = os.path.abspath(payload["path"])  # the game runs in another directory
    try:
        response = request(payload)
    except OSError as error:
        response = {"ok": False, "error": f"cannot reach the game at {socket_path()}: {error}"}
    print(brief(response) if options.brief else json.dumps(response, separators=(",", ":")))
    return 0 if response.get("ok") else 1


if __name__ == "__main__":
    sys.exit(main())
