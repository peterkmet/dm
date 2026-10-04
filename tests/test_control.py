import ctypes
import json
import os
import shutil
import socket
import subprocess
import tempfile
import threading
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

    view = state_json(game, 1)
    assert len(view["squares"]) == 12
    ahead = next(s for s in view["squares"] if s["depth"] == 1 and s["side"] == "center")
    assert ahead["element"] == "wall" and ahead.get("ornament")  # the used mirror

    game_map = state_json(game, 2)
    assert len(game_map["rows"]) == game_map["height"]
    x, y = game_map["party"]
    assert game_map["rows"][y][x] == "@"


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
    assert next(s for s in client.call(cmd="get_view")["view"]["squares"] if s["depth"] == 1 and s["side"] == "center").get("mirror")
    assert client.act("click_wall_ahead")["state"]["candidate"]
    assert client.call(cmd="act", action="move_forward")["ok"] is False  # the recruit panel is open
    response = client.act("recruit", choice="resurrect")
    assert len(response["state"]["champions"]) == 2 and response["state"]["candidate"] is None

    response = client.call(cmd="wait", ticks=5)
    assert response["ok"] and response["ticks"] == 5
    client.sock.close()


def test_mcp_tools(game, monkeypatch, tmp_path):
    from dmtools import mcp_server

    monkeypatch.setenv("DM_CONTROL", game.socket)
    stop = threading.Event()

    def loop():
        while not stop.is_set():
            pump(game.lib)

    thread = threading.Thread(target=loop)
    thread.start()
    try:
        assert mcp_server.get_state()["state"]["champions"]
        assert len(mcp_server.get_view()["view"]["squares"]) == 12
        assert "@" in "".join(mcp_server.get_map()["map"]["rows"])
        before = mcp_server.get_state()["state"]["party"]["facing"]
        after = mcp_server.act("turn_left")["state"]["party"]["facing"]
        assert DIRECTIONS.index(after) == (DIRECTIONS.index(before) + 3) % 4
        assert mcp_server.act("take_object", {"cell": 9})["ok"] is False
        assert mcp_server.wait(2)["ticks"] == 2
        image = mcp_server.screenshot()
        assert image.data.startswith(b"\x89PNG\r\n\x1a\n")
    finally:
        stop.set()
        thread.join()
