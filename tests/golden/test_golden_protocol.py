"""Golden snapshot of the control protocol: a fixed request sequence gives the same JSON replies."""

import ctypes
import hashlib
import json
import os
import shutil
import tempfile
from pathlib import Path

import pytest

from dmtest.control import Client
from dmtest.game import STEP_EAST, STEP_NORTH, Game
from dmtest.lib import load_lib
from dmtest.markers import GAME, needs_dm

GOLDEN = Path(__file__).with_name("protocol.json")
SEED = 0x1234ABCD



@pytest.fixture
def game(tmp_path):
    """The loaded game data in a private copy of libdm, with an open control socket and a temp save folder."""
    lib = load_lib()
    lib.gameloop_load_data.restype = ctypes.c_bool
    lib.control_open.restype = ctypes.c_bool
    lib.control_open.argtypes = [ctypes.c_char_p]
    lib.control_is_turn_pending.restype = ctypes.c_bool
    lib.map_get_square.restype = ctypes.c_uint8
    lib.map_get_square_first_thing.restype = ctypes.c_uint16
    lib.map_get_next_thing.restype = ctypes.c_uint16
    lib.dungeon_get_thing_data.restype = ctypes.POINTER(ctypes.c_uint8)
    lib.kaos_s16.restype = ctypes.c_int16
    assert lib.gameloop_load_data(str(GAME).encode())
    game = Game(lib)
    game.saves = tmp_path / "saves"
    game.saves.mkdir()
    lib.startend_set_save_dir(str(game.saves).encode())
    sock_dir = tempfile.mkdtemp(prefix="dm", dir="/tmp")  # sun_path holds about 100 bytes
    game.socket = os.path.join(sock_dir, "control.sock")
    assert lib.control_open(game.socket.encode())
    yield game
    lib.control_close()
    shutil.rmtree(sock_dir, ignore_errors=True)


def act(client, action, **args):
    return client.call(cmd="act", action=action, args=args)


def session(game, client):
    """The fixed request sequence: a new game, the walk to the first mirror, a recruit, the queries, a save and a load."""
    ctypes.c_uint32.in_dll(game.lib, "g_random_seed").value = SEED
    replies = [client.call(cmd="new_game"), client.call(cmd="get_state")]
    path, direction = game.path_to_mirror()
    for (x0, y0), (x1, y1) in zip(path, path[1:]):
        step = next(k for k in range(4) if (x0 + STEP_EAST[k], y0 + STEP_NORTH[k]) == (x1, y1))
        while game.position()[2] != step:
            replies.append(act(client, "turn_left"))
        replies.append(act(client, "move_forward"))
    while game.position()[2] != direction:
        replies.append(act(client, "turn_left"))
    replies.append(act(client, "click_wall_ahead"))
    replies.append(act(client, "recruit", choice="resurrect"))
    replies.append(client.call(cmd="get_view"))
    replies.append(client.call(cmd="get_map"))
    replies.append(act(client, "look"))
    replies.append(client.call(cmd="wait", ticks=10))
    replies.append(client.call(cmd="act_many", actions=["turn_left", {"action": "turn_right", "args": {}}]))
    replies.append(act(client, "fly"))
    replies.append(client.call(cmd="goto"))
    replies.append(client.call(cmd="save"))
    replies.append(client.call(cmd="wait", ticks=5))
    replies.append(client.call(cmd="load"))
    replies.append(client.call(cmd="get_state"))
    return replies


@needs_dm
def test_protocol_matches_the_golden_snapshot(game):
    client = Client(game)
    try:
        replies = [json.dumps(r, sort_keys=True) for r in session(game, client)]
    finally:
        client.sock.close()
    assert not any(str(game.saves) in r for r in replies)  # the replies hold no machine path
    got = {"replies": [hashlib.sha256(r.encode()).hexdigest() for r in replies]}
    if os.environ.get("DM_UPDATE_GOLDEN"):
        GOLDEN.write_text(json.dumps(got, indent=1) + "\n")
    golden = json.loads(GOLDEN.read_text())
    first = next((i for i, (a, b) in enumerate(zip(got["replies"], golden["replies"])) if a != b), None)
    assert got == golden, f"reply {first} differs: {replies[first] if first is not None else 'count'}"
