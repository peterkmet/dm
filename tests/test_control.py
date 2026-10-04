import ctypes
import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
from contextlib import contextmanager
from pathlib import Path

import pytest

from test_gameloop import STEP_EAST, STEP_NORTH, ZONE_RESURRECT, Game

ROOT = Path(__file__).parent.parent
GAME = ROOT / "game"
DIRECTIONS = ["north", "east", "south", "west"]

pytestmark = [
    pytest.mark.skipif(not shutil.which("clang"), reason="clang not installed"),
    pytest.mark.skipif(not all((GAME / f).exists() for f in ("KAOS.FTL", "Graphics.DAT", "Dungeon.DAT")), reason="game files missing"),
]


@pytest.fixture(scope="module")
def game(tmp_path_factory):
    """A new game in a private build of libdm (other tests rebuild build/ in parallel)."""
    out = tmp_path_factory.mktemp("lib") / "libdm.dylib"
    sources = [str(p) for p in sorted((ROOT / "port/src").glob("*.c")) if p.name not in ("main.c", "platform.c")]
    subprocess.run(["clang", "-std=c11", "-Wall", "-Wextra", "-Werror", "-O1", "-dynamiclib", "-o", str(out), *sources], check=True)
    lib = ctypes.CDLL(str(out))
    lib.get_thing_data.restype = ctypes.POINTER(ctypes.c_uint8)
    lib.get_square_first_thing.restype = ctypes.c_uint16
    lib.get_next_thing.restype = ctypes.c_uint16
    lib.get_square.restype = ctypes.c_uint8
    lib.kaos_s16.restype = ctypes.c_int16
    lib.state_json_string.restype = ctypes.c_void_p
    lib.control_open.argtypes = [ctypes.c_char_p]
    assert lib.gameloop_load_data(str(GAME).encode())
    assert lib.gameloop_new_game(str(GAME).encode())
    game = Game(lib)
    game.run(26)
    path, direction = game.path_to_mirror()
    game.walk(path)
    game.face(direction)
    game.run()
    game.click_box(game.boxes[5])
    game.click_zone(ZONE_RESURRECT)
    assert game.value("g_party_champion_count", ctypes.c_uint16) == 1
    game.mirrors = {(path[-1][0] + STEP_EAST[direction], path[-1][1] + STEP_NORTH[direction])}
    sock_dir = tempfile.mkdtemp(prefix="dm", dir="/tmp")  # sun_path holds about 100 bytes
    game.socket = os.path.join(sock_dir, "control.sock")
    assert lib.control_open(game.socket.encode())
    yield game
    lib.control_close()
    shutil.rmtree(sock_dir, ignore_errors=True)


def state_json(game, part):
    pointer = game.lib.state_json_string(part)
    text = ctypes.string_at(pointer).decode()
    ctypes.CDLL(None).free(ctypes.c_void_p(pointer))
    return json.loads(text)


def pump(lib):
    """One frame of the turn-based main loop."""
    lib.control_poll()
    if lib.control_is_turn_pending() and lib.gameloop_frame():
        lib.gameloop_tick()


@contextmanager
def pumped(game):
    """The turn-based main loop in a thread, for clients that block while they wait."""
    stop = threading.Event()

    def loop():
        while not stop.is_set():
            pump(game.lib)

    thread = threading.Thread(target=loop)
    thread.start()
    try:
        yield
    finally:
        stop.set()
        thread.join()


class Client:
    """A socket client that pumps the game in the same thread while it waits."""

    def __init__(self, game):
        self.game = game
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.connect(game.socket)
        self.sock.setblocking(False)

    def call(self, **payload):
        self.sock.sendall(json.dumps(payload).encode() + b"\n")
        data = b""
        for _ in range(20000):
            pump(self.game.lib)
            try:
                data += self.sock.recv(1 << 20)
            except BlockingIOError:
                pass
            if data.endswith(b"\n"):
                return json.loads(data)
        raise AssertionError("no response")

    def act(self, action, **args):
        response = self.call(cmd="act", action=action, args=args)
        assert response["ok"], response
        return response


def test_state_json_after_recruiting(game):
    state = state_json(game, 0)
    assert state["leader"] == 0
    assert set(state["party"]) >= {"map", "level", "x", "y", "facing", "darkness", "can_move"}
    [champion] = state["champions"]
    assert champion["alive"] and champion["name"]
    assert champion["health"][0] > 0 and champion["health"][1] >= champion["health"][0]
    assert set(champion["statistics"]) == {"luck", "strength", "dexterity", "wisdom", "vitality", "antimagic", "antifire"}
    assert set(champion["skills"]) == {"fighter", "ninja", "priest", "wizard"}
    assert any(s["slot"] == 1 for s in champion["slots"])  # action hand
    assert champion["actions"]
    assert state["spell"]["caster"] == 0 and len(state["spell"]["available"]) == 6
    assert isinstance(state["messages"], list)

    assert champion["stamina"] == [game.champions[0].current_stamina // 10, game.champions[0].maximum_stamina // 10]
    for skill in champion["skills"].values():
        assert (skill["rank"] is None) == (skill["level"] == 1)
    assert any(isinstance(skill["rank"], str) and skill["rank"].isupper() for skill in champion["skills"].values())
    assert champion["load_kg"][1] > champion["load_kg"][0] >= 0

    view = state_json(game, 1)
    assert len(view["squares"]) == 6  # the wall ahead hides depths 2 and 3
    ahead = next(s for s in view["squares"] if s["depth"] == 1 and s["side"] == "center")
    assert ahead["element"] == "wall" and ahead["front_face"].get("ornament")  # the used mirror
    assert ahead["front_face"]["facing"] == DIRECTIONS[(game.position()[2] + 2) % 4]

    game_map = state_json(game, 2)
    assert len(game_map["rows"]) == game_map["height"]
    x, y = game_map["party"]
    assert game_map["rows"][y][x] == "@"
    assert "?" in "".join(game_map["rows"])  # the inscription of the entrance


def test_control_session_moves_and_recruits(game):
    client = Client(game)
    state = client.call(cmd="get_state")
    assert state["ok"] and len(state["state"]["champions"]) == 1
    assert client.call(cmd="act", action="fly")["ok"] is False

    x, y, d = game.position()
    free = next(k for k in range(4) if game.lib.get_square(x + STEP_EAST[k], y + STEP_NORTH[k]) >> 5 == 1)
    while game.position()[2] != free:
        client.act("turn_right")
    response = client.act("move_forward")
    assert [response["state"]["party"]["x"], response["state"]["party"]["y"]] == [x + STEP_EAST[free], y + STEP_NORTH[free]]
    assert response["view"]["facing"] == DIRECTIONS[free]

    path, direction = game.path_to_mirror(game.mirrors)
    for (x0, y0), (x1, y1) in zip(path, path[1:]):
        step = next(k for k in range(4) if (x0 + STEP_EAST[k], y0 + STEP_NORTH[k]) == (x1, y1))
        while game.position()[2] != step:
            client.act("turn_left")
        client.act("move_forward")
        assert game.position()[:2] == [x1, y1]
    while game.position()[2] != direction:
        client.act("turn_left")
    ahead = next(s for s in client.call(cmd="get_view")["view"]["squares"] if s["depth"] == 1 and s["side"] == "center")
    assert ahead["front_face"].get("mirror")
    assert client.call(cmd="get_map")["map"]["rows"][ahead["y"]][ahead["x"]] == "M"
    shown = client.act("click_wall_ahead")["state"]
    assert shown["candidate"]["name"] and shown["candidate"]["skills"]
    assert len(shown["champions"]) == 1 and shown["leader"] in (0, None)
    from dmtools import play

    assert "\nCANDIDATE (not recruited) " + shown["candidate"]["name"] in "\n" + play.brief({"ok": True, "state": shown})
    assert client.call(cmd="act", action="move_forward")["ok"] is False  # the recruit panel is open
    response = client.act("recruit", choice="resurrect")
    assert len(response["state"]["champions"]) == 2 and response["state"]["candidate"] is None

    response = client.call(cmd="wait", ticks=5)
    assert response["ok"] and response["ticks"] == 5
    client.sock.close()


def test_mcp_tools(game, monkeypatch, tmp_path):
    from dmtools import mcp_server

    monkeypatch.setenv("DM_CONTROL", game.socket)
    game.lib.startend_set_save_dir(str(tmp_path).encode())
    with pumped(game):
        assert mcp_server.get_state()["state"]["champions"]
        assert mcp_server.get_view()["view"]["squares"]
        assert "@" in "".join(mcp_server.get_map()["map"]["rows"])
        before = mcp_server.get_state()["state"]["party"]["facing"]
        after = mcp_server.act("turn_left")["state"]["party"]["facing"]
        assert DIRECTIONS.index(after) == (DIRECTIONS.index(before) + 3) % 4
        assert mcp_server.act("take_object", {"cell": 9})["ok"] is False
        assert mcp_server.wait(2)["ticks"] == 2
        image = mcp_server.screenshot()
        assert image.data.startswith(b"\x89PNG\r\n\x1a\n")
        assert mcp_server.save_game() == {"ok": True}
        assert mcp_server.load_game()["state"]["champions"]


def test_save_and_load_restore_the_position_and_the_time(game, tmp_path):
    game.lib.startend_set_save_dir(str(tmp_path).encode())
    client = Client(game)
    assert client.call(cmd="save") == {"ok": True}
    assert (tmp_path / "DMGAME.DAT").exists()
    saved = client.call(cmd="get_state")["state"]

    x, y, _ = game.position()
    free = next(k for k in range(4) if game.lib.get_square(x + STEP_EAST[k], y + STEP_NORTH[k]) >> 5 == 1)
    while game.position()[2] != free:
        client.act("turn_right")
    client.act("move_forward")
    moved = client.call(cmd="wait", ticks=20)["state"]
    assert moved["party"] != saved["party"] and moved["time"] > saved["time"]

    loaded = client.call(cmd="load")
    assert loaded["ok"]
    assert loaded["state"]["party"] == saved["party"] and loaded["state"]["time"] == saved["time"]
    assert client.call(cmd="wait", ticks=2)["ticks"] == 2
    client.sock.close()


def test_dm_play_prints_json_and_the_brief_summary(game, monkeypatch, capsys):
    from dmtools import play

    monkeypatch.setenv("DM_CONTROL", game.socket)
    with pumped(game):
        assert play.main(["get_state"]) == 0
        out = capsys.readouterr().out
        assert out.count("\n") == 1 and json.loads(out)["state"]["champions"]
        assert play.main(["act", "take_object", '{"cell": 9}']) == 1
        assert json.loads(capsys.readouterr().out)["ok"] is False
        assert play.main(["--brief", "wait", '{"ticks": 2}']) == 0
        out = capsys.readouterr().out
        assert out.startswith("ticks 2 | stopped done | game_over False")
        assert "\n0 " in out and "HP " in out and "\nahead: " in out
        assert play.main(["--brief", "act", "turn_left"]) == 0
        assert "facing" in capsys.readouterr().out


@contextmanager
def placed(game, x, y, direction):
    """The party on another square of its map for the state JSON only; no frame runs meanwhile."""
    names = ("g_party_map_x", "g_party_map_y", "g_party_direction")
    saved = game.position()
    for name, value in zip(names, (x, y, direction)):
        ctypes.c_int16.in_dll(game.lib, name).value = value
    try:
        yield
    finally:
        for name, value in zip(names, saved):
            ctypes.c_int16.in_dll(game.lib, name).value = value


def square_byte(game, x, y):
    return game.lib.dungeon_square(game.value("g_party_map_index"), x, y)


def map_size(game):
    lib, m = game.lib, game.value("g_party_map_index")
    lib.dungeon_square.restype = ctypes.c_uint8
    lib.dungeon_map_width.restype = lib.dungeon_map_height.restype = ctypes.c_int16
    return lib.dungeon_map_width(m), lib.dungeon_map_height(m)


def test_view_stops_at_a_closed_door(game):
    lib, m = game.lib, game.value("g_party_map_index")
    lib.automap_is_seen.argtypes = [ctypes.c_int16] * 3
    width, height = map_size(game)
    for x in range(width):
        for y in range(height):
            door = square_byte(game, x, y)
            if door >> 5 != 4 or door & 7 != 4:  # a closed door
                continue
            for d in range(4):
                fx, fy = x - STEP_EAST[d], y - STEP_NORTH[d]
                bx, by = x + STEP_EAST[d], y + STEP_NORTH[d]
                if 0 <= fx < width and 0 <= fy < height and 0 <= bx < width and 0 <= by < height and square_byte(game, fx, fy) >> 5 == 1:
                    with placed(game, fx, fy, d):
                        view, game_map = state_json(game, 1), state_json(game, 2)
                    assert max(s["depth"] for s in view["squares"]) == 1
                    assert next(s for s in view["squares"] if s["depth"] == 1 and s["side"] == "center")["door"] == "closed"
                    if not lib.automap_is_seen(m, bx, by):
                        assert game_map["rows"][by][bx] == " "
                    return
    pytest.skip("no closed door with floor in front on this map")


def test_view_shows_a_mirror_on_a_side_wall(game):
    width, height = map_size(game)
    for mx, my in ((x, y) for x in range(width) for y in range(height) if game.mirror_cell(x, y) is not None):
        face = game.mirror_cell(mx, my)
        d = (face - 1) % 4  # the left wall at depth 1 faces the party lane: direction d + 1
        x, y = mx - STEP_EAST[d] - STEP_EAST[(d + 3) % 4], my - STEP_NORTH[d] - STEP_NORTH[(d + 3) % 4]
        if square_byte(game, x, y) >> 5 != 1:
            continue
        with placed(game, x, y, d):
            view = state_json(game, 1)
        left = next(s for s in view["squares"] if s["depth"] == 1 and s["side"] == "left")
        assert left["element"] == "wall" and left["side_face"]["facing"] == DIRECTIONS[face]
        assert left["side_face"].get("mirror") or left["side_face"].get("ornament")  # the recruited mirror is used
        return
    pytest.skip("no mirror seen from the side")


def free_direction(game):
    x, y, _ = game.position()
    return next(k for k in range(4) if game.lib.get_square(x + STEP_EAST[k], y + STEP_NORTH[k]) >> 5 == 1)


def test_act_many_runs_in_order_and_stops_on_failure_and_damage(game):
    client = Client(game)
    try:
        facing = game.position()[2]
        response = client.call(cmd="act_many", actions=["turn_left", {"action": "turn_left", "args": {}}, "turn_right", "turn_right"])
        assert response["ok"] and response["ran"] == 4 and response["stopped"] == "done"
        assert response["state"]["party"]["facing"] == DIRECTIONS[facing] and response["ticks"] >= 8

        response = client.call(cmd="act_many", actions=["turn_left", {"action": "take_object", "args": {"cell": 9}}, "turn_right"])
        assert response["ran"] == 1 and response["stopped"] == "failed" and "cell" in response["error"]
        assert client.call(cmd="act_many", actions=["fly"])["error"] == "unknown action"
        assert client.call(cmd="act_many", actions="turn_left")["ok"] is False
        assert client.call(cmd="act_many", actions=[5])["stopped"] == "failed"

        ctypes.c_int16.in_dll(game.lib, "g_champion_pending_damage").value = min(10, game.champions[0].current_health - 1)
        response = client.call(cmd="act_many", actions=["turn_left", "turn_left", "turn_left"])
        assert response["ran"] == 1 and response["stopped"] == "damage"
    finally:
        client.sock.close()


def test_wait_runs_all_ticks_or_stops_on_damage(game):
    client = Client(game)
    try:
        response = client.call(cmd="wait", ticks=300)
        assert response["stopped"] == "done" and response["ticks"] == 300
        ctypes.c_int16.in_dll(game.lib, "g_champion_pending_damage").value = min(10, game.champions[0].current_health - 1)
        response = client.call(cmd="wait", ticks=5000)
        assert response["stopped"] == "damage" and 1 <= response["ticks"] < 100
        assert client.call(cmd="wait", ticks=5001)["ok"] is False
    finally:
        client.sock.close()


THING_GROUP, SCREAMER = 4, 6


def add_screamer(lib, x, y):
    """A screamer on square (x, y) of the party map, active but without behavior events (map 0 allows no creatures)."""
    group = lib.get_unused_thing(THING_GROUP)
    data = lib.get_thing_data(group)
    data[2], data[3] = 0xFF, 0xFE  # no possessions
    data[4], data[5] = SCREAMER, 0xFF  # centered on the square
    data[7] = 50  # health
    lib.group_add_active_group(group, x, y)
    lib.link_thing_to_list(group, 0, x, y)


def square_with_floor_left_and_behind(game):
    """A floor square of the party map and a facing with floor to the left and behind."""
    width, height = map_size(game)

    def floor(x, y):
        return 0 <= x < width and 0 <= y < height and game.lib.get_square(x, y) >> 5 == 1

    for x in range(width):
        for y in range(height):
            for f in range(4):
                left, back = (f + 3) % 4, (f + 2) % 4
                if floor(x, y) and floor(x + STEP_EAST[left], y + STEP_NORTH[left]) and floor(x + STEP_EAST[back], y + STEP_NORTH[back]):
                    return x, y, f
    pytest.skip("no floor square with floor to the left and behind")


def test_act_many_stops_only_on_a_new_creature_group(game):
    lib = game.lib
    lib.get_unused_thing.restype = ctypes.c_uint16
    lib.get_unused_thing.argtypes = [ctypes.c_uint16]
    lib.group_add_active_group.argtypes = [ctypes.c_uint16, ctypes.c_int16, ctypes.c_int16]
    lib.link_thing_to_list.argtypes = [ctypes.c_uint16, ctypes.c_uint16, ctypes.c_int16, ctypes.c_int16]
    lib.group_delete.argtypes = [ctypes.c_int16, ctypes.c_int16]
    x, y, f = square_with_floor_left_and_behind(game)
    left = (x + STEP_EAST[(f + 3) % 4], y + STEP_NORTH[(f + 3) % 4])
    behind = (x + STEP_EAST[(f + 2) % 4], y + STEP_NORTH[(f + 2) % 4])
    names = ("g_party_map_x", "g_party_map_y", "g_party_direction")
    saved = game.position()
    for name, value in zip(names, (x, y, f)):
        ctypes.c_int16.in_dll(lib, name).value = value
    client = Client(game)
    try:
        add_screamer(lib, *left)
        response = client.call(cmd="act_many", actions=["turn_left", "turn_right", "turn_left", "turn_right"])
        assert response["stopped"] == "done" and response["ran"] == 4  # the group stays in view
        add_screamer(lib, *behind)
        response = client.call(cmd="act_many", actions=["turn_right", "turn_left"])
        assert response["stopped"] == "creature" and response["ran"] == 1  # one group leaves the view, a new one comes in
    finally:
        client.sock.close()
        for square in (left, behind):
            lib.group_delete(*square)
        for name, value in zip(names, saved):
            ctypes.c_int16.in_dll(lib, name).value = value


def test_goto_is_an_unknown_command(game):
    from dmtools import mcp_server

    client = Client(game)
    try:
        x, y, _ = game.position()
        assert client.call(cmd="goto", x=x, y=y) == {"ok": False, "error": "unknown cmd"}
    finally:
        client.sock.close()
    assert not hasattr(mcp_server, "goto")


def wall_direction(game):
    x, y, _ = game.position()
    return next(k for k in range(4) if game.lib.get_square(x + STEP_EAST[k], y + STEP_NORTH[k]) >> 5 == 0)


def test_a_move_into_a_wall_bumps(game, monkeypatch, capsys):
    from dmtools import play

    client = Client(game)
    try:
        wall = wall_direction(game)
        while game.position()[2] != wall:
            client.act("turn_left")
        position = game.position()
        response = client.act("move_forward")
        assert response["bumped"] is True and game.position() == position
        assert "bumped" not in client.act("turn_left") and "bumped" not in client.call(cmd="wait", ticks=1)
        client.act("turn_right")
        response = client.call(cmd="act_many", actions=["move_forward", "turn_left"])
        assert response["ran"] == 1 and response["stopped"] == "bumped" and response["bumped"] is True
    finally:
        client.sock.close()
    monkeypatch.setenv("DM_CONTROL", game.socket)
    with pumped(game):
        assert play.main(["--brief", "act", "move_forward"]) == 0
        assert capsys.readouterr().out.splitlines()[0].endswith(" | BUMPED")


def test_mcp_and_dm_play_batches(game, monkeypatch, capsys):
    from dmtools import mcp_server, play

    monkeypatch.setenv("DM_CONTROL", game.socket)
    with pumped(game):
        assert mcp_server.act_many(["turn_left", {"action": "turn_right"}])["ran"] == 2
        assert play.main(["--brief", "act_many", '["turn_left", "turn_right"]']) == 0
        assert capsys.readouterr().out.startswith("ticks ")


def test_swap_champions_and_look(game, monkeypatch, capsys):
    client = Client(game)
    try:
        before = [c["position"] for c in client.call(cmd="get_state")["state"]["champions"][:2]]
        after = [c["position"] for c in client.act("swap_champions", a=0, b=1)["state"]["champions"][:2]]
        assert after == before[::-1]
        assert client.call(cmd="act", action="swap_champions", args={"a": 0, "b": 0})["ok"] is False

        assert client.call(cmd="act", action="look", args={})["ok"] is False  # the eye is in the inventory
        client.act("open_inventory", champion=0)
        response = client.act("look", champion=0, slot=1)
        assert response["look"]["name"] == "SAMURAI SWORD"
        assert any(line.startswith("WEIGHS ") and line.endswith("KG.") for line in response["look"]["lines"])
        assert response["state"]["leader_hand"] is None
        slots = {s["slot"]: s["object"]["name"] for s in response["state"]["champions"][0]["slots"] if s["object"]}
        assert slots[1] == "SAMURAI SWORD"

        client.act("take_from_slot", champion=0, slot=1)
        assert client.act("look")["look"] == response["look"]
        advanced = ctypes.c_int16.in_dll(game.lib, "g_advanced_features")
        saved, advanced.value = advanced.value, 1
        try:
            assert client.act("look")["look"]["lines"][-1].startswith("ATTACK ")
        finally:
            advanced.value = saved
        monkeypatch.setenv("DM_CONTROL", game.socket)
        client.sock.close()
        with pumped(game):
            from dmtools import play

            assert play.main(["--brief", "act", "look"]) == 0
            assert "\nlook: SAMURAI SWORD | WEIGHS " in "\n" + capsys.readouterr().out
        client = Client(game)
        client.act("put_in_slot", champion=0, slot=1)
        client.act("close_inventory")
        client.act("swap_champions", a=0, b=1)
    finally:
        client.sock.close()


def chest_with_content(game):
    """A chest of the dungeon with 1-6 objects, and its content."""
    lib = game.lib
    header = ctypes.cast(ctypes.c_void_p.in_dll(lib, "g_dungeon_header"), ctypes.POINTER(ctypes.c_uint8))
    for chest in (9 << 10 | i for i in range(header[12 + 2 * 9] << 8 | header[13 + 2 * 9])):
        data, content = lib.get_thing_data(chest), []
        if bytes(data[:2]) == b"\xff\xff":
            continue
        thing = data[2] << 8 | data[3]
        while thing != 0xFFFE:
            content.append(thing)
            thing = lib.get_next_thing(thing)
        if 1 <= len(content) <= 6:
            return chest, content
    pytest.skip("no chest with 1-6 objects")


def test_take_put_and_look_in_an_open_chest(game):
    lib = game.lib
    lib.champion_put_object_in_leader_hand.argtypes = [ctypes.c_uint16]
    lib.champion_get_object_removed_from_leader_hand.restype = ctypes.c_uint16
    chest, content = chest_with_content(game)
    free = 30 + len(content)
    lib.champion_put_object_in_leader_hand(chest)  # the chest stays in its square list; the test gives it back unchanged
    client = Client(game)
    try:
        client.act("open_inventory", champion=0)
        state = client.act("take_from_slot", champion=0, slot=1)["state"]  # the chest goes to the action hand and opens
        assert state["inventory"]["panel"] == "chest" and state["leader_hand"]["name"] == "SAMURAI SWORD"
        assert [s["slot"] for s in state["inventory"]["chest"]] == list(range(30, free))
        first = state["inventory"]["chest"][0]["object"]
        client.act("put_in_slot", champion=0, slot=0)

        state = client.act("take_from_slot", champion=0, slot=30)["state"]
        assert state["leader_hand"] == first and 30 not in [s["slot"] for s in state["inventory"]["chest"]]
        state = client.act("put_in_slot", champion=0, slot=free)["state"]
        assert state["leader_hand"] is None
        assert {s["slot"]: s["object"] for s in state["inventory"]["chest"]}[free] == first

        response = client.act("look", champion=0, slot=free)
        assert response["look"]["name"] == first["name"] and response["state"]["leader_hand"] is None
        assert len(response["state"]["inventory"]["chest"]) == len(content)
        assert client.call(cmd="act", action="take_from_slot", args={"champion": 0, "slot": 38})["error"] == "slot out of range"

        client.act("take_from_slot", champion=0, slot=free)
        client.act("put_in_slot", champion=0, slot=30)
        client.act("take_from_slot", champion=0, slot=0)
        state = client.act("put_in_slot", champion=0, slot=1)["state"]  # the sword back, the chest closes in the hand
        assert state["leader_hand"]["kind"] == "container" and "chest" not in state["inventory"]
        client.act("close_inventory")
    finally:
        client.sock.close()
        removed = lib.champion_get_object_removed_from_leader_hand()
    assert removed == chest
    data = lib.get_thing_data(chest)
    assert data[2] << 8 | data[3] == content[0]


def test_brief_prints_creatures_on_side_squares():
    from dmtools.play import view_lines

    floor = {"element": "floor"}
    view = {"squares": [floor | {"depth": d, "side": s} for d in (0, 1) for s in ("left", "center", "right")], "piles": []}
    view["squares"][0] |= {"creatures": {"type": "mummy", "count": 2}}
    view["squares"][5] |= {"creatures": {"type": "screamer", "count": 1}}
    lines = view_lines(view)
    assert "here left: floor, CREATURES 2 mummy" in lines
    assert "ahead right: floor, CREATURES 1 screamer" in lines


def test_brief_prints_doors_and_pits_on_side_squares():
    from dmtools.play import view_lines

    floor = {"element": "floor"}
    view = {"squares": [floor | {"depth": d, "side": s} for d in (0, 1) for s in ("left", "center", "right")], "piles": []}
    view["squares"][3] |= {"element": "door", "door": "closed"}
    view["squares"][2] |= {"element": "pit"}
    lines = view_lines(view)
    assert "ahead left: door, closed" in lines
    assert "here right: pit" in lines
    assert not any(line.startswith(("here left:", "ahead right:")) for line in lines)


def test_map_grid_has_indexes(game, monkeypatch, capsys):
    from dmtools import play

    client = Client(game)
    try:
        game_map = client.call(cmd="get_map")["map"]
    finally:
        client.sock.close()
    width, height = game_map["width"], game_map["height"]
    assert len(game_map["rows"]) == height and all(len(row) == width for row in game_map["rows"])
    header, *rows = game_map["grid"]
    assert header == "   " + "".join(str(x % 10) for x in range(width))
    assert rows == [f"{y:2d} {row}" for y, row in enumerate(game_map["rows"])]
    x, y = game_map["party"]
    assert rows[y][3 + x] == "@"
    monkeypatch.setenv("DM_CONTROL", game.socket)
    with pumped(game):
        assert play.main(["--brief", "get_map"]) == 0
    assert capsys.readouterr().out.splitlines() == game_map["grid"]


def test_dm_play_does_not_import_mcp():
    code = "import sys, dmtools.play; assert 'mcp' not in sys.modules"
    subprocess.run([sys.executable, "-c", code], check=True)


def kill_party(game, client):
    pending = (ctypes.c_int16 * 4).in_dll(game.lib, "g_champion_pending_damage")
    for index in range(game.value("g_party_champion_count", ctypes.c_uint16)):
        pending[index] = game.champions[index].current_health + 100  # applied in the first tick of the turn
    result = client.call(cmd="wait", ticks=50)
    assert result["game_over"] is True and result["state"]["party"]["dead"] is True


def test_load_and_new_game_after_a_party_death(game, tmp_path):
    lib = game.lib
    lib.startend_set_save_dir(str(tmp_path).encode())
    client = Client(game)
    try:
        assert client.call(cmd="save") == {"ok": True}
        kill_party(game, client)
        assert client.call(cmd="save") == {"ok": False, "error": "game over"}
        assert client.call(cmd="wait", ticks=1) == {"ok": False, "error": "game over"}

        loaded = client.call(cmd="load")
        assert loaded["ok"] and loaded["state"]["party"]["dead"] is False and loaded["state"]["champions"]
        assert client.call(cmd="wait", ticks=2)["game_over"] is False

        kill_party(game, client)
        new = client.call(cmd="new_game")
        assert new["ok"] and new["state"]["party"]["dead"] is False
        assert new["state"]["champions"] == [] and new["state"]["time"] == 0
        response = client.act("turn_left")
        assert response["game_over"] is False and response["ticks"] >= 2
    finally:
        client.sock.close()
        ctypes.c_int16.in_dll(lib, "g_return_to_entrance").value = 0



def test_dm_play_logs_each_call(tmp_path):
    from dmtools.play import log_call
    path = tmp_path / "calls.log"
    log_call(str(path), {"cmd": "act", "action": "move_forward"}, {"ok": True, "state": {"time": 42, "party": {"map": 1, "x": 3, "y": 4}}})
    log_call(str(path), {"cmd": "get_state"}, {"ok": False})
    lines = [line.split("\t") for line in path.read_text().splitlines()]
    assert lines[0][1:] == ["move_forward", "ok", "1", "3", "4", "42"]
    assert lines[1][1:3] == ["get_state", "fail"]


def test_dm_play_reports_an_unreachable_game(tmp_path, monkeypatch, capsys):
    from dmtools import play

    monkeypatch.setenv("DM_CONTROL", str(tmp_path / "missing.sock"))
    monkeypatch.setattr(play, "LOG_DIR", str(tmp_path))
    assert play.main(["get_state"]) == 1
    assert "cannot reach the game" in json.loads(capsys.readouterr().out)["error"]
    assert "get_state\tfail" in (tmp_path / "calls.log").read_text()


def test_torch_holder_is_not_a_lock(game):
    lib = game.lib
    assert lib.gameloop_new_game(str(GAME).encode())
    holder = None
    for y in range(lib.dungeon_map_height(0)):
        for x in range(lib.dungeon_map_width(0)):
            t = lib.get_square_first_thing(x, y)
            while t not in (0xFFFE, 0xFFFF):
                data = lib.get_thing_data(t)
                if t >> 10 & 15 == 3 and 4 <= (data[2] << 8 | data[3]) >> 7 <= 7:  # a sensor that wants a torch
                    holder = (x, y, t >> 14)
                t = lib.get_next_thing(t)
    assert holder
    x, y, face = holder
    for name, value in (("g_party_map_x", x + STEP_EAST[face]), ("g_party_map_y", y + STEP_NORTH[face]), ("g_party_direction", (face + 2) & 3)):
        ctypes.c_int16.in_dll(lib, name).value = value
    wall = state_json(game, 1)["squares"][4]  # depth 1, center: the wall ahead
    assert wall["front_face"].get("torch_holder") is True
    assert "lock" not in wall["front_face"]


def test_brief_names_a_torch_holder():
    from dmtools.play import square_text

    square = {"element": "wall", "front_face": {"facing": "south", "torch_holder": True}}
    assert square_text(square) == "wall, front face south: torch_holder"


@contextmanager
def on_map(game, m, x, y, direction):
    """The party on map m for the state JSON only, in full light; no frame runs meanwhile."""
    lib, saved_map = game.lib, game.value("g_party_map_index")
    palette = ctypes.c_int16.in_dll(lib, "g_dungeon_view_palette_index")
    saved_palette, palette.value = palette.value, 0
    lib.set_current_map_and_party_map(m)
    lib.view_load_map(m)
    try:
        with placed(game, x, y, direction):
            yield
    finally:
        lib.set_current_map_and_party_map(saved_map)
        lib.view_load_map(saved_map)
        palette.value = saved_palette


def squares_by_place(view):
    return {(s["x"], s["y"]): s for s in view["squares"]}


def test_view_and_map_show_the_pressure_pads_of_map_1(game, monkeypatch, capsys):
    from dmtools import play

    with on_map(game, 1, 14, 1, 1):  # facing east into the room of nine pads
        view, game_map = state_json(game, 1), state_json(game, 2)
    squares = squares_by_place(view)
    for x in (15, 16, 17):
        for y in (0, 1, 2):
            assert squares[x, y]["floor_ornament"] == "pressure_pad", (x, y)
            assert game_map["rows"][y][x] == "_"
    assert "_ pressure pad" in game_map["legend"]
    with on_map(game, 1, 23, 1, 1):  # the pad that holds the east portcullis open, at depth 2
        squares = squares_by_place(state_json(game, 1))
    assert squares[25, 1]["floor_ornament"] == "pressure_pad"
    assert "floor_ornament" not in squares[24, 1] and "floor_ornament" not in squares[23, 1]
    with on_map(game, 1, 25, 1, 3):  # the view draws no floor ornament on the party square
        assert "floor_ornament" not in squares_by_place(state_json(game, 1))[25, 1]
    assert "floor, pressure_pad" in play.square_text(squares[25, 1])
    floor = {"element": "floor"}
    pad = floor | {"floor_ornament": "pressure_pad"}
    view = {"squares": [floor | {"depth": d, "side": s} for d in (0, 1) for s in ("left", "center", "right")], "piles": []}
    view["squares"][3] = pad | {"depth": 1, "side": "left"}
    assert "ahead left: floor, pressure_pad" in play.view_lines(view)


def test_a_plate_without_an_ornament_stays_hidden(game):
    lib = game.lib
    lib.dungeon_square.restype = ctypes.c_uint8
    hidden = (14, 14)  # a floor sensor of map 1 without an ornament
    for d in range(4):
        x, y = hidden[0] - STEP_EAST[d], hidden[1] - STEP_NORTH[d]
        if lib.dungeon_square(1, x, y) >> 5 == 1:
            with on_map(game, 1, x, y, d):
                view, game_map = state_json(game, 1), state_json(game, 2)
            assert "floor_ornament" not in squares_by_place(view)[hidden]
            assert game_map["rows"][hidden[1]][hidden[0]] == "."
            return
    pytest.fail("no floor square next to the hidden plate")
