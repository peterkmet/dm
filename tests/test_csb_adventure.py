"""CSB new adventure: the prison save merges the party into MINI.DAT (docs/csb-new-adventure.md), RESUME, the CSB save dialog."""

import ctypes
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

import pytest

from conftest import PORT_INCLUDES, game_sources
from test_gameloop import STEP_EAST, STEP_NORTH, ZONE_RESURRECT, Game
from test_save import KEY_RETURN, Frames

ROOT = Path(__file__).parent.parent
GAME_CSB = ROOT / "game-csb"
MINI = GAME_CSB / "disk-b" / "MINI.DAT"
CEDT = GAME_CSB / "disk-b" / "CEDT.FTL"
DM, CSB = 0, 1
LOADED = 1
ADVENTURE_MADE, ADVENTURE_NO_CHAMPIONS, ADVENTURE_NAMES_NOT_UNIQUE = 1, 2, 3
CHOICE_CANCEL, CHOICE_SAVE_AND_PLAY, CHOICE_SAVE_AND_QUIT = 1, 2, 3
SAVED_GAME, SAVED_NEW_ADVENTURE = 1, 2
CHAMPION_BYTES, FOOD, WATER = 0x140, 0x42, 0x44
PAYLOAD_START, PAYLOAD_REST = slice(0x100, 0x138), slice(0x162, 0x200)  # the payload without block keys and checksums
NAME = "CSBGAME.DAT"
c16, u16 = ctypes.c_int16, ctypes.c_uint16

pytestmark = [
    pytest.mark.skipif(not shutil.which("clang"), reason="clang not installed"),
    pytest.mark.skipif(not all((GAME_CSB / f).exists() for f in ("APPB.FTL", "Graphics.DAT", "Dungeon.DAT")) or not MINI.exists(),
                       reason="game-csb files missing"),
]


@pytest.fixture(scope="module")
def game(tmp_path_factory):
    """A private build of libdm with the CSB data loaded."""
    out = tmp_path_factory.mktemp("lib") / "libdm.dylib"
    sources = game_sources()
    subprocess.run(["clang", "-std=c11", "-Wall", "-Wextra", "-Werror", "-O1", "-dynamiclib", *PORT_INCLUDES, "-o", str(out), *sources], check=True)
    lib = ctypes.CDLL(str(out))
    lib.dungeon_get_thing_data.restype = ctypes.POINTER(ctypes.c_uint8)
    lib.get_square_first_thing.restype = u16
    lib.get_next_thing.restype = u16
    lib.get_square.restype = ctypes.c_uint8
    lib.kaos_s16.restype = c16
    lib.savegame_make_adventure.restype = c16
    lib.savegame_save_as.restype = ctypes.c_bool
    lib.savegame_load_from.restype = c16
    lib.startend_load_saved_game.restype = c16
    lib.control_open.argtypes = [ctypes.c_char_p]
    c16.in_dll(lib, "g_adventure").value = CSB
    assert lib.gameloop_load_data(str(GAME_CSB).encode())
    yield Game(lib)
    c16.in_dll(lib, "g_adventure").value = DM


@pytest.fixture
def saves(game, tmp_path):
    """The save folder of the test; the frame hook and the end flags are reset after the test."""
    game.lib.startend_set_save_dir(str(tmp_path).encode())
    yield tmp_path
    for name in ("g_quit_requested", "g_return_to_entrance"):
        c16.in_dll(game.lib, name).value = 0
    ctypes.c_void_p.in_dll(game.lib, "g_frame_hook").value = None


def value(game, name, kind=c16):
    return kind.in_dll(game.lib, name).value


def new_prison_game(game, champions):
    """NEW ADVENTURE, then the champions of the nearest mirrors (resurrected)."""
    assert game.lib.gameloop_new_game(str(GAME_CSB).encode())
    assert value(game, "g_dungeon_id", u16) == 12
    game.run(26)
    used = set()
    for count in range(1, champions + 1):
        path, direction = game.path_to_mirror(skip=used)
        game.walk(path)
        game.face(direction)
        game.run()
        game.click_box(game.boxes[5])
        game.click_zone(ZONE_RESURRECT)
        assert value(game, "g_party_champion_count", u16) == count
        used.add((path[-1][0] + STEP_EAST[direction], path[-1][1] + STEP_NORTH[direction]))
    return [bytes(game.champions[i].name).rstrip(b"\0") for i in range(champions)]


def save_now(game):
    reason = ctypes.c_char_p()
    return game.lib.startend_save_current_game(ctypes.byref(reason)), reason.value


def assert_in_adventure(game, names):
    assert [value(game, n) for n in ("g_party_map_index", "g_party_map_x", "g_party_map_y", "g_party_direction")] == [4, 22, 18, 2]
    assert value(game, "g_dungeon_id", u16) == 13
    assert value(game, "g_party_champion_count", u16) == len(names)
    for i, name in enumerate(names):
        c = game.champions[i]
        assert bytes(c.name).rstrip(b"\0") == name
        assert (c.current_health, c.current_stamina, c.current_mana) == (c.maximum_health, c.maximum_stamina, c.maximum_mana)
        assert 1500 <= c.food <= 1755 and 1500 <= c.water <= 1755
        assert set(c.slots) == {0xFFFF}
    assert len({game.champions[i].cell for i in range(len(names))}) == len(names)


def test_merge_equals_the_python_merge(game, saves):
    from dmtools import csb_mini as cm

    names = new_prison_game(game, 3)
    game.champions[1].cell = game.champions[0].cell  # the cell fix of the merge
    assert game.lib.savegame_save_as(str(saves).encode(), b"PRISON.DAT", None) == 1
    assert game.lib.savegame_make_adventure(str(saves).encode(), str(MINI).encode()) == ADVENTURE_MADE
    raw = (saves / NAME).read_bytes()
    port = cm.decode_save(raw)
    b2 = port["blocks"][2]
    randoms = [cm.w(b2, i * CHAMPION_BYTES + o) - 1500 for i in range(len(names)) for o in (FOOD, WATER)]
    assert all(0 <= r <= 255 for r in randoms)
    feed = iter(randoms)
    py = cm.merge(cm.decode_save((saves / "PRISON.DAT").read_bytes()), cm.decode_save(MINI.read_bytes()), lambda: next(feed))
    for i in range(5):
        assert bytes(port["blocks"][i]) == bytes(py["blocks"][i]), i
    assert port["portraits"] == py["portraits"]
    assert port["dungeon"] == py["dungeon"]
    assert port["header"][PAYLOAD_START] == py["header"][PAYLOAD_START] and port["header"][PAYLOAD_REST] == py["header"][PAYLOAD_REST]
    assert port["header"][0x12C] == 2 and port["dungeon_id"] == 13
    assert raw[0x200:] != MINI.read_bytes()[0x200:]  # new block keys
    assert_in_adventure(game, names)
    if CEDT.exists():
        import csb_diff

        csb_diff.check_decode(raw, "port CSBGAME.DAT")


def test_prison_save_continues_in_the_adventure(game, saves):
    names = new_prison_game(game, 2)
    (saves / NAME).write_bytes(b"old")
    assert save_now(game) == (SAVED_NEW_ADVENTURE, None)
    assert (saves / "CSBGAME.BAK").read_bytes() == b"old"
    assert_in_adventure(game, names)
    game.lib.gameloop_start_loaded_game()
    assert value(game, "g_game_screen_inactive") == 0
    assert game.lib.savegame_load_from(str(saves).encode(), NAME.encode(), None) == LOADED
    assert_in_adventure(game, names)
    assert save_now(game) == (SAVED_GAME, None)  # outside the prison: a plain save
    assert value(game, "g_party_map_index") == 4


def test_merge_refuses_an_empty_party(game, saves):
    assert game.lib.gameloop_new_game(str(GAME_CSB).encode())
    assert save_now(game) == (0, b"CAN'T MAKE NEW ADVENTURE WITHOUT CHAMPIONS.")
    assert game.lib.savegame_make_adventure(str(saves).encode(), str(MINI).encode()) == ADVENTURE_NO_CHAMPIONS
    assert not (saves / NAME).exists()
    assert value(game, "g_dungeon_id", u16) == 12


def test_merge_refuses_equal_first_names(game, saves):
    new_prison_game(game, 2)
    game.champions[1].name = game.champions[0].name
    assert save_now(game) == (0, b"EACH CHAMPION MUST HAVE A UNIQUE FIRST NAME!")
    game.champions[1].name = b""
    assert game.lib.savegame_make_adventure(str(saves).encode(), str(MINI).encode()) == ADVENTURE_NAMES_NOT_UNIQUE
    assert not (saves / NAME).exists()


def test_resume_loads_csbgame(game, saves):
    names = new_prison_game(game, 2)
    assert save_now(game) == (SAVED_NEW_ADVENTURE, None)
    assert game.lib.gameloop_new_game(str(GAME_CSB).encode())
    assert game.lib.startend_load_saved_game() == 1
    game.lib.gameloop_start_loaded_game()
    assert_in_adventure(game, names)
    (saves / NAME).unlink()
    assert game.lib.startend_load_saved_game() == 0  # CAN'T FIND SAVED GAME without frames: cancelled


def run_save_dialog(game, choices):
    """The save command with a frame hook that picks the dialog choices in order, then presses a key.
    Returns the number of frames in which a dialog waited without a planned choice."""
    lib = game.lib
    unplanned = []

    def action(frames):
        waiting = value(game, "g_dialog_choice") == 99
        if waiting and choices:
            c16.in_dll(lib, "g_dialog_choice").value = choices.pop(0)
        elif not choices:
            unplanned.append(waiting)
            lib.command_process_key(KEY_RETURN)

    frames = Frames(lib, action)
    lib.startend_save_game()
    frames.remove()
    return sum(unplanned)


def test_save_and_play_saves_and_goes_on(game, saves):
    names = new_prison_game(game, 2)
    run_save_dialog(game, [CHOICE_SAVE_AND_PLAY])
    assert (saves / NAME).exists()
    assert_in_adventure(game, names)
    assert value(game, "g_game_frozen") == 0 and value(game, "g_return_to_entrance") == 0
    (saves / NAME).unlink()
    run_save_dialog(game, [CHOICE_SAVE_AND_PLAY])
    assert (saves / NAME).exists() and value(game, "g_return_to_entrance") == 0


def test_prison_save_and_quit_makes_the_adventure_and_ends_the_game(game, saves):
    new_prison_game(game, 2)
    assert run_save_dialog(game, [CHOICE_SAVE_AND_QUIT]) == 0  # no GAME LOADED dialog before the credits
    assert (saves / NAME).exists()
    assert value(game, "g_dungeon_id", u16) == 13
    assert value(game, "g_return_to_entrance") == 1 and value(game, "g_quit_requested") == 0


def test_save_and_quit_saves_and_ends_the_game(game, saves):
    new_prison_game(game, 2)
    assert save_now(game) == (SAVED_NEW_ADVENTURE, None)
    game.lib.gameloop_start_loaded_game()
    (saves / NAME).unlink()
    run_save_dialog(game, [CHOICE_SAVE_AND_QUIT])
    assert (saves / NAME).exists()
    assert value(game, "g_return_to_entrance") == 1 and value(game, "g_quit_requested") == 0


def test_cancel_and_refused_merge_go_back_to_the_game(game, saves):
    assert game.lib.gameloop_new_game(str(GAME_CSB).encode())
    run_save_dialog(game, [CHOICE_CANCEL])
    run_save_dialog(game, [CHOICE_SAVE_AND_QUIT, 1])  # the reason dialog: OK
    assert not (saves / NAME).exists()
    assert value(game, "g_return_to_entrance") == 0 and value(game, "g_game_frozen") == 0
    assert value(game, "g_dungeon_id", u16) == 12


def test_advanced_save_keeps_the_automap_block(game, saves):
    names = new_prison_game(game, 1)
    assert save_now(game) == (SAVED_NEW_ADVENTURE, None)
    c16.in_dll(game.lib, "g_game_mode").value = 1
    try:
        assert save_now(game) == (SAVED_GAME, None)
        raw = (saves / NAME).read_bytes()
        length = int.from_bytes(raw[-4:], "big")
        assert raw[-4 - length:][:4] == b"AMAP"
        assert game.lib.savegame_load_from(str(saves).encode(), NAME.encode(), None) == LOADED
        assert_in_adventure(game, names)
    finally:
        c16.in_dll(game.lib, "g_game_mode").value = 0


def test_control_new_game_save_and_load(game, saves):
    from test_control import Client

    sock_dir = tempfile.mkdtemp(prefix="dm", dir="/tmp")
    game.socket = os.path.join(sock_dir, "control.sock")
    assert game.lib.control_open(game.socket.encode())
    try:
        client = Client(game)
        state = client.call(cmd="new_game")
        assert state["ok"] and value(game, "g_dungeon_id", u16) == 12
        assert client.call(cmd="save") == {"ok": False, "error": "CAN'T MAKE NEW ADVENTURE WITHOUT CHAMPIONS."}
        names = new_prison_game(game, 2)
        assert client.call(cmd="save") == {"ok": True}
        assert_in_adventure(game, names)
        assert client.call(cmd="new_game")["ok"] and value(game, "g_dungeon_id", u16) == 12
        assert client.call(cmd="load")["ok"]
        assert_in_adventure(game, names)
    finally:
        game.lib.control_close()
        shutil.rmtree(sock_dir, ignore_errors=True)


def run_dm(*args):
    subprocess.run(["make", "-C", "port"], cwd=ROOT, check=True, capture_output=True)
    env = dict(os.environ, SDL_VIDEO_DRIVER="dummy", SDL_AUDIO_DRIVER="dummy")
    env.pop("DM_AUTO_ENTER", None)
    return subprocess.run([str(ROOT / "build/dm"), *args, "--frames", "20"], cwd=ROOT, env=env, capture_output=True, text=True, timeout=60)


def test_turnbased_csb_starts_the_prison(tmp_path):
    folder = tmp_path / "csb"
    shutil.copytree(GAME_CSB, folder, ignore=shutil.ignore_patterns("CSBGAME.*", "QUICK.DAT"))
    sock = Path(tempfile.mkdtemp(prefix="dm", dir="/tmp")) / "c.sock"
    try:
        result = run_dm("--adventure", "csb", "--mode", "turnbased", "--data", str(folder), "--control", str(sock))
        assert result.returncode == 0, result.stderr
        assert "adventure csb, mode turnbased" in result.stderr
    finally:
        shutil.rmtree(sock.parent, ignore_errors=True)
