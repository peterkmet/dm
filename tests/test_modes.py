import ctypes
import json
import os
import shutil
import signal
import socket
import struct
import subprocess
import tempfile
import time
import zlib
from pathlib import Path

import pytest

from conftest import PORT_INCLUDES, game_sources
from test_control import Client, pump
from test_gameloop import CORRIDOR, KEY_ESCAPE, KEY_F1, KEY_RETURN, KEY_UP, LEFT_PRESS, LEFT_RELEASE, STEP_EAST, STEP_NORTH, ZONE_RESURRECT, Box, Game

ROOT = Path(__file__).parent.parent
GAME = ROOT / "game"
SCRATCH = Path("/private/tmp/claude-501/-Users-lego-github-dm/a035dd1c-2cf2-40ec-9360-f301ae6b451f/scratchpad")
VANILLA, ADVANCED, TURNBASED = 0, 1, 2
KEY_TAB, KEY_M, KEY_F5, KEY_F9 = 0x4200, 0x3700, 0x5400, 0x5800
KEY_DOWN, KEY_RIGHT, KEY_LEFT, KEY_DEL, KEY_HELP = 0x4D00, 0x4E00, 0x4F00, 0x4600, 0x5F00
QUICK = "QUICK.DAT"
THING_WEAPON, THING_ARMOUR, THING_SCROLL = 5, 6, 7
ZONE_VIEWPORT, ZONE_ACTION_AREA, ZONE_PANEL = 7, 11, 101
COLOR_BLACK, COLOR_CYAN, COLOR_RED, COLOR_YELLOW, COLOR_WHITE = 0, 4, 8, 11, 15
SLOT_HAND_ACTION = 1
SAVE = "DMGAME.DAT"
c16, u16, u32 = ctypes.c_int16, ctypes.c_uint16, ctypes.c_uint32

pytestmark = [
    pytest.mark.skipif(not shutil.which("clang"), reason="clang not installed"),
    pytest.mark.skipif(not all((GAME / f).exists() for f in ("KAOS.FTL", "Graphics.DAT", "Dungeon.DAT")), reason="game files missing"),
]


@pytest.fixture(scope="module")
def game(tmp_path_factory):
    """A new game with one recruited champion in a private build of libdm."""
    out = tmp_path_factory.mktemp("lib") / "libdm.dylib"
    sources = game_sources()
    subprocess.run(["clang", "-std=c11", "-Wall", "-Wextra", "-Werror", "-O1", "-dynamiclib", *PORT_INCLUDES, "-o", str(out), *sources], check=True)
    lib = ctypes.CDLL(str(out))
    lib.get_thing_data.restype = ctypes.POINTER(ctypes.c_uint8)
    lib.get_square_first_thing.restype = u16
    lib.get_next_thing.restype = u16
    lib.get_square.restype = ctypes.c_uint8
    lib.kaos_s16.restype = c16
    lib.dungeon_map_width.restype = c16
    lib.dungeon_map_height.restype = c16
    lib.savegame_save.restype = c16
    lib.savegame_load.restype = c16
    lib.video_color.restype = u16
    lib.video_argb.restype = u32
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
    assert game.value("g_party_champion_count", u16) == 1
    game.run(30)
    yield game
    set_mode(game, VANILLA)


def set_mode(game, mode, features=None):
    c16.in_dll(game.lib, "g_game_mode").value = mode
    c16.in_dll(game.lib, "g_advanced_features").value = int(mode != VANILLA) if features is None else features


def zone_box(game, zone):
    box = Box()
    assert game.lib.ui_zone_box(zone, ctypes.byref(box))
    return box


def pixels(game, zone):
    fb = (ctypes.c_uint8 * (320 * 200)).in_dll(game.lib, "fb")
    b = zone_box(game, zone)
    return [fb[(b.y + y) * 320 + b.x + x] for y in range(b.h) for x in range(b.w)]


def screenshot(game, name):
    """The screen as a PNG in the scratchpad, 2 times larger."""
    if not SCRATCH.exists():
        return
    lib, scale = game.lib, 2
    fb = (ctypes.c_uint8 * (320 * 200)).in_dll(lib, "fb")
    rows = []
    for y in range(200):
        row = b"".join(lib.video_argb(lib.video_color(y, fb[y * 320 + x])).to_bytes(4, "big")[1:] * scale for x in range(320))
        rows += [b"\0" + row] * scale
    chunk = lambda t, d: struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d))
    header = struct.pack(">IIBBBBB", 320 * scale, 200 * scale, 8, 2, 0, 0, 0)
    (SCRATCH / name).write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(b"".join(rows))) + chunk(b"IEND", b""))


def seen(game):
    lib, m = game.lib, game.value("g_party_map_index")
    lib.explored_is_seen.restype = ctypes.c_bool
    return {(x, y) for x in range(lib.dungeon_map_width(m)) for y in range(lib.dungeon_map_height(m)) if lib.explored_is_seen(m, x, y)}


def thing_count(game, thing_type):
    header = ctypes.c_void_p.in_dll(game.lib, "g_dungeon_header").value
    return int.from_bytes(ctypes.string_at(header + 12 + 2 * thing_type, 2), "big")


def press(game, key, frames=13):
    game.key(key)
    game.run(frames)


def unfreeze(game):
    for key in (KEY_ESCAPE, KEY_RETURN):
        if game.value("g_game_frozen"):
            press(game, key)
    assert game.value("g_game_frozen") == 0


def test_vanilla_is_the_default_and_the_advanced_keys_do_nothing(game, tmp_path):
    assert game.value("g_game_mode") == VANILLA and game.value("g_advanced_features") == 0
    game.lib.startend_set_save_dir(str(tmp_path).encode())
    position = game.position()
    for key in (KEY_F5, KEY_F9, KEY_M, KEY_TAB):
        press(game, key)
    assert game.value("g_automap_open") == 0 and not (tmp_path / QUICK).exists()
    assert game.value("g_inventory_champion_ordinal") == 0 and game.position() == position
    assert [game.lib.advanced_letter_key(ord(c)) for c in "wsadqe"] == [0] * 6


def test_letter_keys_move_and_turn_outside_vanilla(game):
    set_mode(game, ADVANCED)
    keys = [game.lib.advanced_letter_key(ord(c)) for c in "wsadqex"]
    assert keys == [KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_DEL, KEY_HELP, 0]
    set_mode(game, VANILLA)


def message_text(game):
    rows = (ctypes.c_char * (4 * 53)).in_dll(game.lib, "g_message")
    return bytes(rows).replace(b"\0", b" ").decode("latin-1")


def test_quick_save_and_quick_load(game, tmp_path):
    lib = game.lib
    set_mode(game, ADVANCED)
    lib.startend_set_save_dir(str(tmp_path).encode())
    press(game, KEY_F9)
    assert "NO QUICK SAVE" in message_text(game)
    position, time_saved = game.position(), game.value("g_game_time", u32)
    press(game, KEY_F5)
    assert (tmp_path / QUICK).exists() and not (tmp_path / SAVE).exists()
    assert "GAME SAVED" in message_text(game)
    game.face((position[2] + 2) & 3)
    game.run(30)
    assert game.position() != position
    press(game, KEY_F9)
    assert game.position() == position
    assert game.value("g_game_time", u32) < time_saved + 30
    assert "GAME LOADED" in message_text(game)
    assert game.value("g_game_screen_inactive") == 0 and game.value("g_game_time_ticking") == 1
    assert game.value("g_saved_game_exists") == 0  # a quick save is not a saved game for RESTART
    good = (tmp_path / QUICK).read_bytes()
    for offset in (600, len(good) // 2):  # a checked block, then the dungeon part
        damaged = bytearray(good)
        damaged[offset] ^= 0xFF
        (tmp_path / QUICK).write_bytes(bytes(damaged))
        position = game.position()
        press(game, KEY_F9)
        assert "QUICK SAVE DAMAGED" in message_text(game)
        assert game.value("g_party_champion_count", u16) == 1
        game.face((position[2] + 1) & 3)  # turns and moves read the squares of the running dungeon
        press(game, KEY_UP, 30)
        game.run(100)
    lib.startend_set_save_dir(b".")
    set_mode(game, VANILLA)


def test_bad_mode_stops_the_program():
    subprocess.run(["make", "-C", "port"], cwd=ROOT, check=True, capture_output=True)
    env = dict(os.environ, SDL_VIDEO_DRIVER="dummy", SDL_AUDIO_DRIVER="dummy")
    result = subprocess.run([str(ROOT / "build/dm"), "--mode", "fast", "--frames", "1"], cwd=ROOT, env=env, capture_output=True, timeout=60)
    assert result.returncode == 1


def test_automap_stops_the_game_time_and_draws_the_map(game):
    set_mode(game, ADVANCED)
    game.run(13)
    time_before, position = game.value("g_game_time", u32), game.position()
    press(game, KEY_M)
    assert game.value("g_automap_open") == 1
    view = pixels(game, ZONE_VIEWPORT)
    assert COLOR_YELLOW in view  # the party arrow, in the interface palette
    palette = list((u16 * 32).in_dll(game.lib, "palette"))
    assert palette[:16] == palette[16:]  # the viewport rows use the interface palette
    screenshot(game, "adv_automap.png")
    press(game, KEY_UP, 50)
    assert game.value("g_game_time", u32) == time_before
    assert game.position() == position
    press(game, KEY_TAB)
    assert game.value("g_automap_open") == 0
    game.run(30)
    assert game.value("g_game_time", u32) > time_before
    set_mode(game, VANILLA)


def arrow_x(game):
    """The mean x of the party arrow pixels in the viewport."""
    b = zone_box(game, ZONE_VIEWPORT)
    xs = [i % b.w for i, c in enumerate(pixels(game, ZONE_VIEWPORT)) if c == COLOR_YELLOW]
    return sum(xs) / len(xs) if xs else None


def test_a_drag_scrolls_the_automap(game):
    set_mode(game, ADVANCED)
    press(game, KEY_M)
    centered = arrow_x(game)
    b = zone_box(game, ZONE_VIEWPORT)
    x, y = b.x + b.w // 2, b.y + b.h // 2
    game.click(x, y)
    assert arrow_x(game) == centered  # a click without a move does not scroll
    game.lib.command_process_click(x, y, LEFT_PRESS)
    game.lib.advanced_drag(x - 3 * 7 - 3, y)  # 3 squares and a part of one to the west
    game.lib.command_process_click(x - 3 * 7 - 3, y, LEFT_RELEASE)
    game.run()
    assert arrow_x(game) == centered - 3 * 7  # the map follows the mouse in whole squares
    press(game, KEY_M)
    press(game, KEY_M)
    assert arrow_x(game) == centered
    press(game, KEY_M)
    set_mode(game, VANILLA)

def test_automap_opens_only_in_the_dungeon_view(game):
    set_mode(game, ADVANCED)
    press(game, KEY_F1)
    assert game.value("g_inventory_champion_ordinal") == 1
    press(game, KEY_M)
    assert game.value("g_automap_open") == 0
    press(game, KEY_F1)
    assert game.value("g_inventory_champion_ordinal") == 0
    press(game, KEY_M)
    press(game, KEY_ESCAPE)
    assert game.value("g_automap_open") == 0 and game.value("g_game_frozen") == 0
    set_mode(game, VANILLA)


def test_quick_keys_work_only_in_the_dungeon_view(game, tmp_path):
    lib = game.lib
    set_mode(game, ADVANCED)
    lib.startend_set_save_dir(str(tmp_path).encode())
    press(game, KEY_F1)
    press(game, KEY_F5)
    assert not (tmp_path / QUICK).exists()
    press(game, KEY_F1)
    waiting = c16.in_dll(lib, "g_waiting_frame")
    waiting.value = 1  # a sequence or an endgame wait runs the frame
    press(game, KEY_F5)
    waiting.value = 0
    assert not (tmp_path / QUICK).exists()
    press(game, KEY_F5)
    assert (tmp_path / QUICK).exists()
    lib.startend_set_save_dir(b".")
    set_mode(game, VANILLA)


def test_automap_shows_the_position_readout(game):
    lib = game.lib
    set_mode(game, ADVANCED)
    game.run(13)
    lib.gameloop_draw_view()
    view = pixels(game, ZONE_VIEWPORT)
    set_mode(game, VANILLA)
    lib.gameloop_draw_view()
    assert pixels(game, ZONE_VIEWPORT) == view  # the dungeon view has no read-out
    set_mode(game, ADVANCED)
    press(game, KEY_M)
    with_readout = pixels(game, ZONE_VIEWPORT)
    assert COLOR_WHITE in with_readout[:224 * 14]  # the read-out text in the top rows
    screenshot(game, "adv_automap_readout.png")
    press(game, KEY_M)
    set_mode(game, VANILLA)


@pytest.mark.parametrize("mode", [VANILLA, ADVANCED])
def test_escape_rule(game, mode):
    set_mode(game, mode)
    press(game, KEY_F1)
    assert game.value("g_inventory_champion_ordinal") == 1
    press(game, KEY_ESCAPE)
    if mode == ADVANCED:
        assert game.value("g_inventory_champion_ordinal") == 0 and game.value("g_game_frozen") == 0
        press(game, KEY_ESCAPE)
    assert game.value("g_game_frozen") == 1
    unfreeze(game)
    if game.value("g_inventory_champion_ordinal"):
        press(game, KEY_F1)
    set_mode(game, VANILLA)


@pytest.mark.parametrize("mode", [VANILLA, ADVANCED])
def test_tab_closes_the_inventory(game, mode):
    set_mode(game, mode)
    press(game, KEY_F1)
    press(game, KEY_TAB)
    assert game.value("g_inventory_champion_ordinal") == (0 if mode == ADVANCED else 1)
    assert game.value("g_automap_open") == 0
    if game.value("g_inventory_champion_ordinal"):
        press(game, KEY_F1)
    set_mode(game, VANILLA)

def test_save_and_load_keep_the_automap(game, tmp_path):
    lib = game.lib
    set_mode(game, ADVANCED)
    for key in (KEY_UP, 0x5F00, KEY_UP, KEY_UP):  # forward, turn right (Help), forward
        press(game, key, 30)
    explored = seen(game)
    assert len(explored) > 6
    seed = game.value("g_random_seed", u32)
    vanilla_dir, advanced_dir = tmp_path / "vanilla", tmp_path / "advanced"
    vanilla_dir.mkdir()
    advanced_dir.mkdir()
    set_mode(game, VANILLA)
    assert lib.savegame_save(str(vanilla_dir).encode()) == 1
    u32.in_dll(lib, "g_random_seed").value = seed
    set_mode(game, ADVANCED)
    assert lib.savegame_save(str(advanced_dir).encode()) == 1
    vanilla, advanced = (vanilla_dir / SAVE).read_bytes(), (advanced_dir / SAVE).read_bytes()
    assert len(advanced) > len(vanilla) and advanced[: len(vanilla)] == vanilla

    lib.explored_reset()
    assert seen(game) == set()
    assert lib.savegame_load(str(advanced_dir).encode()) == 1
    assert seen(game) == explored

    set_mode(game, VANILLA)  # vanilla loads a file with the block, and the vanilla file without one
    assert lib.savegame_load(str(advanced_dir).encode()) == 1
    assert lib.savegame_load(str(vanilla_dir).encode()) == 1
    assert seen(game) == set()

    damaged = bytearray(advanced)
    damaged[len(vanilla) + 4] ^= 0xFF  # the version byte of the block
    (advanced_dir / SAVE).write_bytes(bytes(damaged))
    assert lib.savegame_load(str(advanced_dir).encode()) == 1
    assert seen(game) == set()
    lib.gameloop_start_loaded_game()
    assert len(seen(game)) > 0


def test_render_advanced_panels(game):
    lib, champion = game.lib, game.champions[0]
    set_mode(game, ADVANCED)
    hand = champion.slots[SLOT_HAND_ACTION]

    assert hand >> 10 & 15 == THING_WEAPON  # the sword of the recruited champion
    armour = next(t for t in champion.slots if t != 0xFFFF and t >> 10 & 15 == THING_ARMOUR)
    set_mode(game, VANILLA)
    lib.menus_draw_action_area()
    black_icon = pixels(game, ZONE_ACTION_AREA)
    set_mode(game, ADVANCED)
    lib.menus_draw_action_area()
    colored_icon = pixels(game, ZONE_ACTION_AREA)
    assert set(black_icon) <= {COLOR_BLACK, COLOR_CYAN} and len(set(colored_icon)) > 2
    screenshot(game, "adv_action_icons.png")

    press(game, KEY_F1)
    assert game.value("g_inventory_champion_ordinal") == 1
    champion.slots[SLOT_HAND_ACTION] = hand
    leader_hand = u16.in_dll(lib, "g_leader_hand_object")
    empty_handed = c16.in_dll(lib, "g_leader_empty_handed")
    held, was_empty = leader_hand.value, empty_handed.value
    for thing, name in ((hand, "adv_stat_meters.png"), (armour, "adv_stat_meters_armour.png")):
        leader_hand.value, empty_handed.value = thing, 0  # the eye shows the description of the leader hand object
        set_mode(game, VANILLA)
        lib.draw_panel_eye()
        lib.ui_draw_viewport()
        plain = pixels(game, ZONE_VIEWPORT)
        set_mode(game, ADVANCED)
        lib.draw_panel_eye()
        lib.ui_draw_viewport()
        assert pixels(game, ZONE_VIEWPORT) != plain
        screenshot(game, name)
    leader_hand.value, empty_handed.value = held, was_empty

    runes = False
    for index in range(thing_count(game, THING_SCROLL)):
        champion.slots[SLOT_HAND_ACTION] = THING_SCROLL << 10 | index
        set_mode(game, VANILLA)
        lib.draw_panel()
        lib.ui_draw_viewport()
        plain_red = pixels(game, ZONE_VIEWPORT).count(COLOR_RED)
        set_mode(game, ADVANCED)
        lib.draw_panel()
        lib.ui_draw_viewport()
        if pixels(game, ZONE_VIEWPORT).count(COLOR_RED) > plain_red:
            runes = True
            screenshot(game, "adv_scroll_runes.png")
            break
    assert runes

    champion.slots[SLOT_HAND_ACTION] = hand
    press(game, KEY_F1)
    assert game.value("g_inventory_champion_ordinal") == 0
    set_mode(game, VANILLA)


def test_turnbased_program_answers_get_state_and_act(tmp_path):
    subprocess.run(["make", "-C", "port"], cwd=ROOT, check=True, capture_output=True)
    sock_dir = tempfile.mkdtemp(prefix="dm", dir="/tmp")  # sun_path holds about 100 bytes
    path = os.path.join(sock_dir, "control.sock")
    env = dict(os.environ, SDL_VIDEO_DRIVER="dummy", SDL_AUDIO_DRIVER="dummy")
    program = subprocess.Popen([str(ROOT / "build/dm"), "--mode", "turnbased", "--control", path, "--frames", "100000"],
                               cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        for _ in range(200):
            if os.path.exists(path):
                break
            time.sleep(0.05)

        def call(**payload):
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
                s.settimeout(60)
                s.connect(path)
                s.sendall(json.dumps(payload).encode() + b"\n")
                data = b""
                while not data.endswith(b"\n"):
                    data += s.recv(1 << 20)
                return json.loads(data)

        for _ in range(100):  # the entrance frames run before the game exists
            state = call(cmd="get_state")
            if state["ok"]:
                break
            time.sleep(0.05)
        assert state["ok"] and state["state"]["party"]["map"] == 0
        time_before = state["state"]["time"]
        time.sleep(0.3)
        assert call(cmd="get_state")["state"]["time"] == time_before  # the game waits for a turn
        result = call(cmd="act", action="move_forward", args={})
        assert result["ok"] and result["ticks"] >= 2 and result["game_over"] is False
        assert result["state"]["time"] > time_before
    finally:
        program.send_signal(signal.SIGINT)
        try:
            program.wait(timeout=10)
        except subprocess.TimeoutExpired:
            program.kill()
        shutil.rmtree(sock_dir, ignore_errors=True)
    assert not os.path.exists(path)


def test_turnbased_load_sent_at_once_runs_the_game(tmp_path):
    subprocess.run(["make", "-C", "port"], cwd=ROOT, check=True, capture_output=True)
    data = tmp_path / "game"
    shutil.copytree(ROOT / "game", data, ignore=shutil.ignore_patterns("DMGAME.*", "QUICK.DAT"))
    sock_dir = tempfile.mkdtemp(prefix="dm", dir="/tmp")  # sun_path holds about 100 bytes
    path = os.path.join(sock_dir, "control.sock")
    env = dict(os.environ, SDL_VIDEO_DRIVER="dummy", SDL_AUDIO_DRIVER="dummy")

    def session(requests, wait_for_game):
        program = subprocess.Popen([str(ROOT / "build/dm"), "--mode", "turnbased", "--data", str(data), "--control", path,
                                    "--frames", "100000"], cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            while not os.path.exists(path):
                time.sleep(0.001)
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
                s.settimeout(60)
                s.connect(path)
                lines = s.makefile("rw")

                def call(**payload):
                    lines.write(json.dumps(payload) + "\n")
                    lines.flush()
                    return json.loads(lines.readline())

                while wait_for_game and not call(cmd="get_state")["ok"]:
                    time.sleep(0.05)
                return [call(**r) for r in requests]
        finally:
            program.send_signal(signal.SIGINT)
            try:
                program.wait(timeout=10)
            except subprocess.TimeoutExpired:
                program.kill()

    try:
        assert session([{"cmd": "save"}], wait_for_game=True)[0]["ok"]
        loaded, waited = session([{"cmd": "load"}, {"cmd": "wait", "ticks": 5}], wait_for_game=False)
        assert loaded["ok"]
        assert waited["ticks"] == 5  # the load before the first entrance frame still leaves the entrance
    finally:
        shutil.rmtree(sock_dir, ignore_errors=True)


def test_smooth_projectile_flight(game):
    lib = game.lib
    for direction in range(4):  # a direction with open floor ahead
        game.face(direction)
        game.run()
        x, y, _ = game.position()
        nx, ny = x + STEP_EAST[direction], y + STEP_NORTH[direction]
        if lib.get_square(nx, ny) >> 5 == CORRIDOR:
            break
    fireball = 0xFF80
    for mode in (VANILLA, ADVANCED):
        set_mode(game, mode)
        game.run(13)
        lib.projectile_create(fireball, x, y, direction, direction, 255, 100, 10)
        frames = []
        for _ in range(3 * 13):
            seed = game.value("g_random_seed", u32)
            ticked = lib.gameloop_frame()
            if ticked:
                lib.gameloop_tick()
            else:
                assert game.value("g_random_seed", u32) == seed  # the drawn motion uses no game random numbers
            frames.append((ticked, pixels(game, ZONE_VIEWPORT)))
        changes = sum(view != before for (_, before), (ticked, view) in zip(frames, frames[1:]) if not ticked)  # frames without a tick
        if mode == VANILLA:
            assert changes == 0
        else:
            assert changes > 3
            screenshot(game, "adv_projectile.png")
        game.run(200)  # the fireball explodes
    set_mode(game, VANILLA)


@pytest.mark.parametrize("mode", [VANILLA, ADVANCED])
def test_empty_hand_at_a_fountain(game, mode):
    lib, champions = game.lib, game.champions
    set_mode(game, mode)
    count = u16.in_dll(lib, "g_party_champion_count")
    start = game.value("g_party_map_index"), game.position()
    ctypes.memmove(ctypes.byref(champions[1]), ctypes.byref(champions[0]), ctypes.sizeof(champions[0]))
    champions[1].cell = (champions[0].cell + 1) % 4
    count.value = 2
    lib.set_current_map_and_party_map(1)
    for name, v in zip(("g_party_map_x", "g_party_map_y", "g_party_direction"), (0, 21, 2)):  # facing a fountain on level 2
        c16.in_dll(lib, name).value = v
    lib.gameloop_draw_view()
    assert game.value("g_facing_fountain") == 1
    champions[0].water = champions[1].water = 100
    game.click_box(game.boxes[5])
    assert (champions[0].water, champions[1].water) == (2048, 2048 if mode == ADVANCED else 100)
    count.value = 1
    ctypes.memset(ctypes.byref(champions[1]), 0, ctypes.sizeof(champions[1]))
    lib.set_current_map_and_party_map(start[0])
    for name, v in zip(("g_party_map_x", "g_party_map_y", "g_party_direction"), start[1]):
        c16.in_dll(lib, name).value = v
    lib.gameloop_draw_view()
    set_mode(game, VANILLA)


KEY_L, KEY_I, KEY_F = 0x2800, 0x1700, 0x2300


@pytest.mark.parametrize("mode", [VANILLA, ADVANCED, TURNBASED])
def test_cheat_keys_work_only_on_the_advanced_automap(game, mode):
    set_mode(game, mode)
    press(game, KEY_L)  # not on the automap: nothing
    assert game.value("g_cheat_light") == 0
    press(game, KEY_M)
    press(game, KEY_L)
    press(game, KEY_I)
    press(game, KEY_F)
    on = mode == ADVANCED
    assert (game.value("g_cheat_light"), game.value("g_cheat_immortal"), game.value("g_cheat_food")) == (on, on, on)
    if on:
        assert COLOR_WHITE in pixels(game, ZONE_VIEWPORT)[:224 * 24]  # the cheat line under the read-out
        press(game, KEY_L)
        press(game, KEY_I)
        press(game, KEY_F)
        assert (game.value("g_cheat_light"), game.value("g_cheat_immortal"), game.value("g_cheat_food")) == (0, 0, 0)
    press(game, KEY_M)
    set_mode(game, VANILLA)


def test_cheat_light_draws_the_bright_palette(game):
    from test_champion import Party
    lib, party = game.lib, Party.in_dll(game.lib, "g_party")
    place = {n: c16.in_dll(lib, n) for n in ("g_party_map_index", "g_party_map_x", "g_party_map_y", "g_party_direction")}
    start = {n: v.value for n, v in place.items()}
    palette = (u16 * 16).in_dll(lib, "palette")
    try:
        for name, v in zip(place, (1, 14, 4, 3)):  # map 1 has darkness
            place[name].value = v
        party.magical_light_amount = 1000
        lib.gameloop_draw_view()
        bright = list(palette)
        party.magical_light_amount = -1000
        lib.gameloop_draw_view()
        assert list(palette) != bright
        c16.in_dll(lib, "g_cheat_light").value = 1
        lib.gameloop_draw_view()
        assert list(palette) == bright
        assert game.value("g_dungeon_view_palette_index") == 5  # the creatures still see the darkness
    finally:
        c16.in_dll(lib, "g_cheat_light").value = 0
        party.magical_light_amount = 0
        for name, v in start.items():
            place[name].value = v
        lib.gameloop_draw_view()


def test_cheat_immortal_keeps_1_health(game):
    lib, champion = game.lib, game.champions[0]
    health = champion.current_health
    lib.champion_damage_all.argtypes = [u16, u16, c16]
    c16.in_dll(lib, "g_cheat_immortal").value = 1
    try:
        lib.champion_damage_all(5000, 0, 0)
        lib.champion_apply_and_draw_pending_damage_and_wounds()
        assert champion.current_health == 1
    finally:
        c16.in_dll(lib, "g_cheat_immortal").value = 0
        champion.current_health = health

def test_cheat_food_keeps_food_and_water(game):
    lib, champion = game.lib, game.champions[0]
    food, water = champion.food, champion.water
    c16.in_dll(lib, "g_cheat_food").value = 1
    try:
        for _ in range(1000):
            lib.champion_apply_time_effects()
        assert (champion.food, champion.water) == (food, water)
        c16.in_dll(lib, "g_cheat_food").value = 0
        for _ in range(1000):
            lib.champion_apply_time_effects()
        assert champion.food < food and champion.water < water
    finally:
        c16.in_dll(lib, "g_cheat_food").value = 0
        champion.food, champion.water = food, water


def test_party_death_during_a_turn_reports_game_over(game):
    lib = game.lib
    sock_dir = tempfile.mkdtemp(prefix="dm", dir="/tmp")
    game.socket = os.path.join(sock_dir, "control.sock")
    assert lib.control_open(game.socket.encode())
    try:
        set_mode(game, TURNBASED)
        client = Client(game)
        assert client.call(cmd="get_state")["state"]["party"]["dead"] is False
        pending = (c16 * 4).in_dll(lib, "g_champion_pending_damage")
        pending[0] = game.champions[0].current_health + 100  # applied in the first tick of the turn
        result = client.call(cmd="wait", ticks=50)
        assert result["ok"] and result["game_over"] is True and result["state"]["party"]["dead"] is True
        assert result["ticks"] < 50
        assert client.call(cmd="act", action="move_forward", args={}) == {"ok": False, "error": "game over"}
        assert client.call(cmd="get_state")["ok"]
        client.sock.close()
    finally:
        lib.control_close()
        shutil.rmtree(sock_dir, ignore_errors=True)
        c16.in_dll(lib, "g_return_to_entrance").value = 0
        set_mode(game, VANILLA)
        assert lib.gameloop_new_game(str(GAME).encode())
