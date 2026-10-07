"""dm-play: sends one request to the turn-based control socket and prints the response.

    uv run dm-play <cmd> [json-args]
    uv run dm-play act <action> [json-args]
    uv run dm-play act_many '<json-list>'
    uv run dm-play wait '{"ticks": N}'    (1-5000; stops early on damage, a new creature, a message)

The response is printed as compact JSON on one line. --brief prints a short summary instead.
The exit code is 1 when the response has "ok": false. The socket path comes from DM_CONTROL
(default ~/.dm/control.sock).
"""

import argparse
import time
import json
import os
import sys

from dmtools.control_client import request, socket_path

LOG_DIR = os.environ.get("DM_PLAY_LOG_DIR", "playthroughs/current/coordinator")  # one line per call goes to calls.log here, when the folder exists
LANE = ["here", "ahead", "ahead 2", "ahead 3"]


def square_text(square: dict) -> str:
    """One square of the view in a few words: element, door, wall face, creatures, objects, flying."""
    words = [square["element"]]
    if (ornament := square.get("floor_ornament")) is not None:
        words.append(ornament if isinstance(ornament, str) else f"floor ornament {ornament}")
    if "door" in square:
        words.append(square["door"] + (" with button" if square.get("button") else ""))
    for face in ("front_face", "side_face"):
        if wall := square.get(face):
            features = [key for key in ("mirror", "switch", "lock", "torch_holder", "alcove", "fountain", "vi_altar", "ornament") if wall.get(key)]
            features = [f"{key} {wall[key]}" if key in ("lock", "ornament") and wall[key] is not True else key for key in features]
            features += [f'text "{wall["text"]}"'] if "text" in wall else []
            words.append(f"{face.replace('_', ' ')} {wall['facing']}: " + " ".join(features))
    if creatures := square.get("creatures"):
        words.append(f"CREATURES {creatures['count']} {creatures['type']}")
    if square.get("objects"):
        words.append("objects " + ", ".join(f"{o['name']} ({o['cell']})" for o in square["objects"]))
    if square.get("flying"):
        words.append("flying " + ", ".join(str(f.get("name", f["kind"])) for f in square["flying"]))
    return ", ".join(words)


def champion_text(c: dict) -> str:
    bars = " ".join(f"{label} {c[key][0]}/{c[key][1]}" for label, key in (("HP", "health"), ("ST", "stamina"), ("MA", "mana")))
    return f"{bars} food {c['food']} water {c['water']}"


def state_lines(state: dict) -> list[str]:
    party = state["party"]
    flags = [flag for flag in ("sleeping", "dead") if party[flag]] + ([] if party["can_move"] else ["cannot move yet"])
    hand = state["leader_hand"]
    tag = " | CSB" if state.get("adventure") == "csb" else ""
    lines = [
        f"time {state['time']}{tag} | level {party['level']} x {party['x']} y {party['y']} facing {party['facing']} | "
        f"darkness {party['darkness']}" + "".join(f" | {flag}" for flag in flags),
        f"leader {'-' if state['leader'] is None else state['leader']} | hand {hand['name'] if hand else '-'}"
    ]
    if candidate := state["candidate"]:
        lines.append(f"CANDIDATE (not recruited) {candidate['name']} {candidate['title']}: " + champion_text(candidate) + " | act recruit")
    for c in state["champions"]:
        notes = [note for note in ("poisoned", "action_disabled") if c[note]] + ([] if c["alive"] else ["DEAD"])
        lines.append(f"{c['index']} {c['name']} ({c['position']}): {champion_text(c)}" + "".join(f" {n}" for n in notes))
    if state["action_menu"]:
        lines.append(f"action menu of {state['action_menu']['champion']}: " + ", ".join(state["action_menu"]["actions"]))
    if state["messages"]:
        lines.append("messages: " + " / ".join(state["messages"]))
    return lines


def view_lines(view: dict) -> list[str]:
    squares = {(s["depth"], s["side"]): s for s in view["squares"]}
    lines = [f"sides: left {squares[0, 'left']['element']}, right {squares[0, 'right']['element']}"]
    for depth, name in enumerate(LANE):
        if (depth, "center") not in squares:
            break
        lines.append(f"{name}: {square_text(squares[depth, 'center'])}")
        for side in ("left", "right"):
            square = squares.get((depth, side))
            if square is None:  # the screen does not draw this side square
                continue
            opening = depth > 0 and square["element"] != "wall"  # a side passage the player sees
            if opening or square["element"] == "pit" or any(key in square for key in ("door", "side_face", "floor_ornament", "creatures")):
                lines.append(f"{name} {side}: {square_text(square)}")
    if view["piles"]:
        lines.append("piles: " + ", ".join(f"cell {p['cell']} {p['top']}" for p in view["piles"]))
    return lines


def place_text(at: dict) -> str:
    return LANE[at["depth"]] + ("" if at["side"] == "center" else f" {at['side']}")


def seen_text(seen: dict) -> str:
    """What the party saw during the turn, also when it was gone at the end."""
    parts = [f"{e['name']} at {place_text(e)}" for e in seen["explosions"]]
    for c in seen["creature_changes"]:
        lost = "was killed" if c["after"] == 0 else f"lost {c['before'] - c['after']}"
        parts.append(f"a group of {c['before']} {c['type']} at {place_text(c)} {lost}")
    return "seen: " + "; ".join(parts)


def brief(response: dict) -> str:
    """A few lines for the player: the turn, the party, the champions, what is ahead, the messages."""
    if not response.get("ok"):
        return f"error: {response.get('error')}"
    head = " | ".join(f"{key} {response[key]}" for key in ("ticks", "ran", "stopped", "error", "game_over", "action_damage", "health_lost", "path") if key in response)
    head += " | BUMPED" if response.get("bumped") else ""
    lines = [head] if head else []
    if look := response.get("look"):
        lines.append(f"look: {look['name']}" + "".join(f" | {line}" for line in look["lines"]))
    if seen := response.get("seen"):
        lines.append(seen_text(seen))
    if "state" in response:
        lines += state_lines(response["state"])
    if "view" in response:
        lines += view_lines(response["view"])
    if "map" in response:
        lines += response["map"]["grid"]
    return "\n".join(lines) or "ok"


def log_call(path: str, payload: dict, response: dict) -> None:
    """Appends one line per call: time, command, result, level, position, game time."""
    party = (response.get("state") or {}).get("party") or {}
    what = payload.get("action") or payload["cmd"]
    line = [time.strftime("%Y-%m-%dT%H:%M:%S"), what, "ok" if response.get("ok") else "fail",
            party.get("map", ""), party.get("x", ""), party.get("y", ""), (response.get("state") or {}).get("time", "")]
    with open(path, "a") as log:
        log.write("\t".join(str(v) for v in line) + "\n")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="dm-play", description="Sends one request to the Dungeon Master control socket.")
    parser.add_argument("--brief", action="store_true", help="print a short summary instead of the JSON")
    parser.add_argument("cmd", help="get_state, get_view, get_map, act, act_many, wait, screenshot, save, load, new_game")
    parser.add_argument("rest", nargs="*", help="act: the action, then the args as JSON; act_many: the actions as a JSON list; "
                        "other commands: the fields as JSON")
    options = parser.parse_args(argv)
    payload = {"cmd": options.cmd}
    rest = options.rest
    if options.cmd == "act":
        if not rest:
            parser.error("act needs an action")
        payload["action"], rest = rest[0], rest[1:]
        payload["args"] = json.loads(rest[0]) if rest else {}
    elif options.cmd == "act_many":
        if len(rest) != 1:
            parser.error("act_many needs the actions as one JSON list")
        payload["actions"] = json.loads(rest[0])
    elif rest:
        payload.update(json.loads(rest[0]))
    if "path" in payload:
        payload["path"] = os.path.abspath(payload["path"])  # the game runs in another directory
    try:
        response = request(payload)
    except OSError as error:
        response = {"ok": False, "error": f"cannot reach the game at {socket_path()}: {error}"}
    if os.path.isdir(LOG_DIR):
        log_call(os.path.join(LOG_DIR, "calls.log"), payload, response)
    print(brief(response) if options.brief else json.dumps(response, separators=(",", ":")))
    return 0 if response.get("ok") else 1


if __name__ == "__main__":
    sys.exit(main())
