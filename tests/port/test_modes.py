# SPDX-License-Identifier: GPL-3.0-or-later
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

from dmtest import constants as C
from dmtest.game import (screen, ADVANCED, STEP_EAST, STEP_NORTH, VANILLA, Box, Location, Party, ProjectileLaunch, press, recruit_first_champion, set_mode,
                         start_game, session)
from dmtest.lib import load_lib
from dmtest.markers import ROOT, needs_dm, needs_sdl

PNG_DIR = os.environ.get("DM_TEST_PNG_DIR")  # optional: a folder for PNG renders of the screen
QUICK = "QUICK.DAT"
CHEAT_BLOCK_SIZE = 6 + 4  # the cheat block of an advanced save and its length
SAVE = "DMGAME.DAT"
c16, u16, u32 = ctypes.c_int16, ctypes.c_uint16, ctypes.c_uint32

pytestmark = [needs_dm]


@pytest.fixture(scope="module")
def game():
    """A new game with one recruited champion in a private copy of libdm."""
    lib = load_lib()
    lib.transport_open.restype = ctypes.c_bool
    lib.dungeon_map_width.restype = c16
    lib.dungeon_map_height.restype = c16
    lib.savegame_save.restype = ctypes.c_bool
    lib.savegame_load.restype = ctypes.c_int
    lib.video_get_color.restype = u16
    lib.video_convert_argb.restype = u32
    lib.transport_open.argtypes = [ctypes.c_char_p]
    game = start_game(lib)
    recruit_first_champion(game)
    game.run(30)
    yield game
    set_mode(game, VANILLA)


def zone_box(game, zone):
    box = Box()
    assert game.lib.ui_zone_box(zone, ctypes.byref(box))
    return box


def pixels(game, zone):
    fb = screen(game.lib).px
    b = zone_box(game, zone)
    return [fb[(b.y + y) * 320 + b.x + x] for y in range(b.h) for x in range(b.w)]


def screenshot(game, name):
    """The screen as a PNG in DM_TEST_PNG_DIR, 2 times larger."""
    if not PNG_DIR:
        return
    lib, scale = game.lib, 2
    fb = screen(lib).px
    rows = []
    for y in range(200):
        row = b"".join(lib.video_convert_argb(lib.video_get_color(y, fb[y * 320 + x])).to_bytes(4, "big")[1:] * scale for x in range(320))
        rows += [b"\0" + row] * scale
    chunk = lambda t, d: struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d))
    header = struct.pack(">IIBBBBB", 320 * scale, 200 * scale, 8, 2, 0, 0, 0)
    (Path(PNG_DIR) / name).write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(b"".join(rows))) + chunk(b"IEND", b""))


def seen(game):
    lib, m = game.lib, game.value("g_party_map_index")
    lib.explored_is_seen.restype = ctypes.c_bool
    return {(x, y) for x in range(lib.dungeon_map_width(m)) for y in range(lib.dungeon_map_height(m)) if lib.explored_is_seen(m, x, y)}


def unfreeze(game):
    for key in (C.KEY_ESCAPE, C.KEY_RETURN):
        if game.value("g_game_frozen"):
            press(game, key)
    assert game.value("g_game_frozen") == 0


def test_vanilla_is_the_default_and_the_advanced_keys_do_nothing(game, tmp_path):
    assert game.session.mode == VANILLA and game.value("g_advanced_features") == 0
    game.lib.startend_set_save_dir(str(tmp_path).encode())
    position = game.position()
    for key in (C.KEY_F5, C.KEY_F9, C.KEY_M, C.KEY_TAB, C.KEY_C):
        press(game, key)
    assert game.value("g_advanced_screen") == C.ADVANCED_SCREEN_NONE and not (tmp_path / QUICK).exists()
    assert game.session.inventory_champion_index == -1 and game.position() == position
    assert [game.lib.advanced_letter_key(ord(c)) for c in "wsadqe"] == [0] * 6


def message_text(game):
    rows = (ctypes.c_char * (4 * 53)).in_dll(game.lib, "g_message")
    return bytes(rows).replace(b"\0", b" ").decode("latin-1")


def test_quick_save_and_quick_load(game, tmp_path):
    lib = game.lib
    set_mode(game, ADVANCED)
    lib.startend_set_save_dir(str(tmp_path).encode())
    press(game, C.KEY_F9)
    assert "NO QUICK SAVE" in message_text(game)
    position, time_saved = game.position(), game.value("g_game_time", u32)
    press(game, C.KEY_F5)
    assert (tmp_path / QUICK).exists() and not (tmp_path / SAVE).exists()
    assert "GAME SAVED" in message_text(game)
    game.face((position[2] + 2) & 3)
    game.run(30)
    assert game.position() != position
    press(game, C.KEY_F9)
    assert game.position() == position
    assert game.value("g_game_time", u32) < time_saved + 30
    assert "GAME LOADED" in message_text(game)
    assert game.value("g_game_screen_shown") == 1 and game.value("g_game_time_ticking") == 1
    assert game.session.saved_game_exists == 0  # a quick save is not a saved game for RESTART
    good = (tmp_path / QUICK).read_bytes()
    for offset in (600, len(good) // 2):  # a checked block, then the dungeon part
        damaged = bytearray(good)
        damaged[offset] ^= 0xFF
        (tmp_path / QUICK).write_bytes(bytes(damaged))
        position = game.position()
        press(game, C.KEY_F9)
        assert "QUICK SAVE DAMAGED" in message_text(game)
        assert game.value("g_party_champion_count", c16) == 1
        game.face((position[2] + 1) & 3)  # turns and moves read the squares of the running dungeon
        press(game, C.KEY_UP, 30)
        game.run(100)
    lib.startend_set_save_dir(b".")
    set_mode(game, VANILLA)


def test_automap_stops_the_game_time_and_draws_the_map(game):
    set_mode(game, ADVANCED)
    game.run(13)
    time_before, position = game.value("g_game_time", u32), game.position()
    press(game, C.KEY_M)
    assert game.value("g_advanced_screen") == C.ADVANCED_SCREEN_AUTOMAP
    view = pixels(game, C.ZONE_VIEWPORT)
    assert C.COLOR_YELLOW in view  # the party arrow, in the interface palette
    palette = list(screen(game.lib).palette)
    assert palette[:16] == palette[16:]  # the viewport rows use the interface palette
    screenshot(game, "adv_automap.png")
    press(game, C.KEY_UP, 50)
    assert game.value("g_game_time", u32) == time_before
    assert game.position() == position
    press(game, C.KEY_TAB)
    assert game.value("g_advanced_screen") == C.ADVANCED_SCREEN_NONE
    game.run(30)
    assert game.value("g_game_time", u32) > time_before
    set_mode(game, VANILLA)


def test_automap_opens_only_in_the_dungeon_view(game):
    set_mode(game, ADVANCED)
    press(game, C.KEY_F1)
    assert game.session.inventory_champion_index == 0
    press(game, C.KEY_M)
    assert game.value("g_advanced_screen") == C.ADVANCED_SCREEN_NONE
    press(game, C.KEY_F1)
    assert game.session.inventory_champion_index == -1
    press(game, C.KEY_M)
    press(game, C.KEY_ESCAPE)
    assert game.value("g_advanced_screen") == C.ADVANCED_SCREEN_NONE and game.value("g_game_frozen") == 0
    set_mode(game, VANILLA)


def test_quick_keys_work_only_in_the_dungeon_view(game, tmp_path):
    lib = game.lib
    set_mode(game, ADVANCED)
    lib.startend_set_save_dir(str(tmp_path).encode())
    press(game, C.KEY_F1)
    press(game, C.KEY_F5)
    assert not (tmp_path / QUICK).exists()
    press(game, C.KEY_F1)
    waiting = c16.in_dll(lib, "g_frame_wait_depth")
    waiting.value = 1  # a sequence or an endgame wait runs the frame
    press(game, C.KEY_F5)
    waiting.value = 0
    assert not (tmp_path / QUICK).exists()
    press(game, C.KEY_F5)
    assert (tmp_path / QUICK).exists()
    lib.startend_set_save_dir(b".")
    set_mode(game, VANILLA)


@pytest.mark.parametrize("mode", [VANILLA, ADVANCED])
def test_escape_rule(game, mode):
    set_mode(game, mode)
    press(game, C.KEY_F1)
    assert game.session.inventory_champion_index == 0
    press(game, C.KEY_ESCAPE)
    if mode == ADVANCED:
        assert game.session.inventory_champion_index == -1 and game.value("g_game_frozen") == 0
        press(game, C.KEY_ESCAPE)
        assert game.value("g_advanced_screen") == C.ADVANCED_SCREEN_OPTIONS and game.value("g_game_frozen") == 0
        press(game, C.KEY_ESCAPE)
        assert game.value("g_advanced_screen") == C.ADVANCED_SCREEN_NONE
    else:
        assert game.value("g_game_frozen") == 1
        unfreeze(game)
    if game.session.inventory_champion_index >= 0:
        press(game, C.KEY_F1)
    set_mode(game, VANILLA)


def test_options_menu_stops_the_game_time(game):
    set_mode(game, ADVANCED)
    game.run(13)
    time_before, position = game.value("g_game_time", u32), game.position()
    press(game, C.KEY_ESCAPE)
    assert game.value("g_advanced_screen") == C.ADVANCED_SCREEN_OPTIONS
    screenshot(game, "adv_options.png")
    press(game, C.KEY_UP, 50)
    assert game.value("g_game_time", u32) == time_before and game.position() == position
    press(game, C.KEY_ESCAPE)
    game.run(30)
    assert game.value("g_game_time", u32) > time_before
    set_mode(game, VANILLA)


def test_one_viewport_screen_at_a_time(game):
    set_mode(game, ADVANCED)
    press(game, C.KEY_M)
    press(game, C.KEY_ESCAPE)  # closes the automap only
    assert game.value("g_advanced_screen") == C.ADVANCED_SCREEN_NONE
    press(game, C.KEY_ESCAPE)
    press(game, C.KEY_M)  # the menu stays, the automap does not open
    assert game.value("g_advanced_screen") == C.ADVANCED_SCREEN_OPTIONS
    press(game, C.KEY_ESCAPE)
    assert game.value("g_advanced_screen") == C.ADVANCED_SCREEN_NONE
    set_mode(game, VANILLA)


def test_options_menu_writes_the_settings_only_after_a_change(game, tmp_path):
    lib, settings = game.lib, tmp_path / "SETTINGS.TXT"
    volume = c16.in_dll(lib, "g_audio_volume")
    set_mode(game, ADVANCED)
    lib.startend_set_save_dir(str(tmp_path).encode())
    for key in (C.KEY_ESCAPE, C.KEY_DOWN, C.KEY_DOWN, C.KEY_DOWN, C.KEY_DOWN, C.KEY_RETURN):  # CONTINUE without a change
        press(game, key)
    assert game.value("g_advanced_screen") == C.ADVANCED_SCREEN_NONE and not settings.exists()
    for key in (C.KEY_ESCAPE, C.KEY_LEFT, C.KEY_LEFT, C.KEY_DOWN, C.KEY_RIGHT, C.KEY_ESCAPE):  # sound 5, music 7
        press(game, key)
    assert volume.value == 5 and settings.read_text() == "sound_volume=5\nmusic_volume=7\nmusic=original\nfilter=none\n"
    lib.startend_set_save_dir(str(tmp_path / "missing").encode())
    for key in (C.KEY_ESCAPE, C.KEY_RIGHT, C.KEY_ESCAPE):
        press(game, key)
    assert volume.value == 6 and "UNABLE TO SAVE OPTIONS" in message_text(game)
    volume.value = C.SETTINGS_VOLUME_FULL
    lib.startend_set_save_dir(b".")
    set_mode(game, VANILLA)


@pytest.mark.parametrize("mode", [VANILLA, ADVANCED])
def test_tab_closes_the_inventory(game, mode):
    set_mode(game, mode)
    press(game, C.KEY_F1)
    press(game, C.KEY_TAB)
    assert game.session.inventory_champion_index == (-1 if mode == ADVANCED else 0)
    assert game.value("g_advanced_screen") == C.ADVANCED_SCREEN_NONE
    if game.session.inventory_champion_index >= 0:
        press(game, C.KEY_F1)
    set_mode(game, VANILLA)

def test_save_and_load_keep_the_automap(game, tmp_path):
    lib = game.lib
    set_mode(game, ADVANCED)
    for key in (C.KEY_UP, 0x5F00, C.KEY_UP, C.KEY_UP):  # forward, turn right (Help), forward
        press(game, key, 30)
    explored = seen(game)
    assert len(explored) > 6
    seed = game.value("g_random_seed", u32)
    vanilla_dir, advanced_dir = tmp_path / "vanilla", tmp_path / "advanced"
    vanilla_dir.mkdir()
    advanced_dir.mkdir()
    set_mode(game, VANILLA)
    assert lib.savegame_save(str(vanilla_dir).encode()) is True
    u32.in_dll(lib, "g_random_seed").value = seed
    set_mode(game, ADVANCED)
    assert lib.savegame_save(str(advanced_dir).encode()) is True
    vanilla, advanced = (vanilla_dir / SAVE).read_bytes(), (advanced_dir / SAVE).read_bytes()
    assert len(advanced) > len(vanilla) and advanced[: len(vanilla)] == vanilla

    lib.explored_reset()
    assert seen(game) == set()
    assert lib.savegame_load(str(advanced_dir).encode()) == C.SAVEGAME_LOADED
    assert seen(game) == explored

    set_mode(game, VANILLA)  # vanilla loads a file with the block, and the vanilla file without one
    assert lib.savegame_load(str(advanced_dir).encode()) == C.SAVEGAME_LOADED
    assert lib.savegame_load(str(vanilla_dir).encode()) == C.SAVEGAME_LOADED
    assert seen(game) == set()

    damaged = bytearray(advanced)
    damaged[len(vanilla) + CHEAT_BLOCK_SIZE + 4] ^= 0xFF  # the version byte of the automap block
    (advanced_dir / SAVE).write_bytes(bytes(damaged))
    assert lib.savegame_load(str(advanced_dir).encode()) == C.SAVEGAME_LOADED
    assert seen(game) == set()
    lib.gameloop_start_loaded_game()
    assert len(seen(game)) > 0



def cheats(game):
    s = game.session
    return {n for n in ("light", "immortal", "food", "kill", "walls", "map") if getattr(s, "cheat_" + n)}


def set_cheats(game, names):
    for n in ("light", "immortal", "food", "kill", "walls", "map"):
        setattr(game.session, "cheat_" + n, n in names)


def split_extension(data):
    """The original part and the extension blocks (without their lengths) of a save file, the first block first."""
    blocks = []
    while data[-4 - int.from_bytes(data[-4:], "big"):][:4] in (b"AMAP", b"CHTS"):
        length = int.from_bytes(data[-4:], "big")
        blocks.insert(0, data[-4 - length:-4])
        data = data[:-4 - length]
    return data, blocks


def join_extension(original, blocks):
    return original + b"".join(b + len(b).to_bytes(4, "big") for b in blocks)


def test_save_and_load_keep_the_cheats(game, tmp_path):
    lib, path = game.lib, tmp_path / SAVE
    set_mode(game, ADVANCED)
    press(game, C.KEY_UP, 30)
    explored = seen(game)
    set_cheats(game, {"light", "walls"})
    assert lib.savegame_save(str(tmp_path).encode()) is True
    good = path.read_bytes()
    original, blocks = split_extension(good)
    assert [b[:5] for b in blocks] == [b"CHTS\x01", b"AMAP\x01"] and blocks[0] == b"CHTS\x01\x11"

    def load(data, mode=ADVANCED):
        path.write_bytes(data)
        set_cheats(game, {"immortal"})
        lib.explored_reset()
        set_mode(game, mode)
        result = lib.savegame_load(str(tmp_path).encode())
        set_mode(game, ADVANCED)
        return result, cheats(game), seen(game) == explored

    try:
        assert load(good) == (C.SAVEGAME_LOADED, {"light", "walls"}, True)
        lib.gameloop_start_loaded_game()  # the start of the loaded game keeps them
        assert cheats(game) == {"light", "walls"}
        assert load(join_extension(original, blocks[::-1])) == (C.SAVEGAME_LOADED, {"light", "walls"}, True)
        assert load(join_extension(original, [b"CHTS\x01\x08", *blocks])) == (C.SAVEGAME_LOADED, {"light", "walls"}, True)
        assert load(join_extension(original, blocks[1:])) == (C.SAVEGAME_LOADED, set(), True)  # an old save
        for damaged in (b"CHTS\x02\x11", b"CHTS\x01\x40", b"CHTS\x01\x11\x00"):  # version, unknown bit, size
            assert load(join_extension(original, [damaged, blocks[1]])) == (C.SAVEGAME_LOADED, set(), True)
        for mode in (VANILLA, C.GAME_MODE_TURNBASED):
            assert load(good, mode)[:2] == (C.SAVEGAME_LOADED, set())
        unknown = join_extension(original, [*blocks, b"XXXX"])  # the unknown block stops the scan
        assert load(unknown) == (C.SAVEGAME_LOADED, set(), False)
        damaged = bytearray(good)
        damaged[600] ^= 0xFF  # a checked block of the original part
        assert load(bytes(damaged))[:2] == (C.SAVEGAME_DAMAGED, {"immortal"})  # the running game keeps its cheats
        set_mode(game, VANILLA)
        assert lib.savegame_save(str(tmp_path).encode()) is True
        assert path.read_bytes() == original  # a vanilla save has no extension
    finally:
        set_cheats(game, set())
        set_mode(game, VANILLA)


@needs_sdl
def test_turnbased_program_answers_get_state_and_act(tmp_path):
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


@needs_sdl
def test_turnbased_load_sent_at_once_runs_the_game(tmp_path):
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
        if lib.rec_square_type(lib.map_get_square(nx, ny)) == C.SQUARE_CORRIDOR:
            break
    fireball = 0xFF80
    for mode in (VANILLA, ADVANCED):
        set_mode(game, mode)
        game.run(13)
        lib.projectile_create(ctypes.byref(ProjectileLaunch(fireball, Location(x, y), direction, direction, 255, 100, 10)))
        frames = []
        for _ in range(3 * 13):
            seed = game.value("g_random_seed", u32)
            ticked = lib.gameloop_run_frame()
            if ticked:
                lib.gameloop_tick()
            else:
                assert game.value("g_random_seed", u32) == seed  # the drawn motion uses no game random numbers
            frames.append((ticked, pixels(game, C.ZONE_VIEWPORT)))
        changes = sum(view != before for (_, before), (ticked, view) in zip(frames, frames[1:]) if not ticked)  # frames without a tick
        if mode == VANILLA:
            assert changes == 0
        else:
            assert changes > 3
            screenshot(game, "adv_projectile.png")
        game.run(200)  # the fireball explodes
    set_mode(game, VANILLA)


def test_cheat_light_draws_the_bright_palette(game):
    lib, party = game.lib, Party.in_dll(game.lib, "g_party")
    place = {n: c16.in_dll(lib, n) for n in ("g_party_map_index", "g_party_map_x", "g_party_map_y", "g_party_direction")}
    start = {n: v.value for n, v in place.items()}
    palette = screen(lib).palette
    try:
        for name, v in zip(place, (1, 14, 4, 3)):  # map 1 has darkness
            place[name].value = v
        party.magical_light_amount = 1000
        lib.gameloop_draw_view()
        bright = palette[:16]
        party.magical_light_amount = -1000
        lib.gameloop_draw_view()
        assert palette[:16] != bright
        session(lib).cheat_light = 1
        lib.gameloop_draw_view()
        assert palette[:16] == bright
        assert game.value("g_party_light_level") == 5  # the creatures still see the darkness
    finally:
        session(lib).cheat_light = 0
        party.magical_light_amount = 0
        for name, v in start.items():
            place[name].value = v
        lib.gameloop_draw_view()


def test_cheat_immortal_keeps_1_health(game):
    lib, champion = game.lib, game.champions[0]
    health = champion.current_health
    lib.champion_damage_all.argtypes = [c16, u16, c16]
    session(lib).cheat_immortal = 1
    try:
        lib.champion_damage_all(5000, 0, 0)
        lib.champion_apply_pending_damage()
        assert champion.current_health == 1
    finally:
        session(lib).cheat_immortal = 0
        champion.current_health = health

def test_cheat_food_keeps_food_and_water(game):
    lib, champion = game.lib, game.champions[0]
    food, water = champion.food, champion.water
    session(lib).cheat_food = 1
    try:
        for _ in range(1000):
            lib.champion_apply_time_effects()
        assert (champion.food, champion.water) == (food, water)
        session(lib).cheat_food = 0
        for _ in range(1000):
            lib.champion_apply_time_effects()
        assert champion.food < food and champion.water < water
    finally:
        session(lib).cheat_food = 0
        champion.food, champion.water = food, water

