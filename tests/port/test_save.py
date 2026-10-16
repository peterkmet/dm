# SPDX-License-Identifier: GPL-3.0-or-later
import ctypes
import struct

import pytest

from dmtest import constants as C
from dmtest.game import Location
from dmtest.game import ACTIVE_GROUP_BYTES, Box, Frames, Game, recruit_and_hold_object, state
from dmtest.lib import load_lib
from dmtest.markers import GAME, needs_dm
from dmtest.savefile import G_LOAD_FROM_MEMORY, SAVE_READ_CHECKED_BLOCK, SAVE_READ_HEADER, SaveFile, decode, encode

from dmtools.emu import Emu
from dmtools.ftl import load_image, parse

KAOS = GAME / "KAOS.FTL"
SAVE, BACKUP = "DMGAME.DAT", "DMGAME.BAK"
LOADED, MISSING, DAMAGED = 1, 0, -1
c16, u16, u32 = ctypes.c_int16, ctypes.c_uint16, ctypes.c_uint32

pytestmark = [needs_dm]


@pytest.fixture(scope="module")
def game():
    lib = load_lib()
    lib.gameloop_load_data.restype = ctypes.c_bool
    lib.gameloop_start_new_game.restype = ctypes.c_bool
    lib.ui_get_slot_box_zone.restype = ctypes.c_bool
    lib.rec_write.restype = ctypes.POINTER(ctypes.c_uint8)
    lib.map_get_square_first_thing.restype = u16
    lib.rec_thing_next.restype = u16
    lib.map_get_square.restype = ctypes.c_uint8
    lib.kaos_s16.restype = c16
    lib.dungeon_save.restype = ctypes.c_void_p
    lib.free.argtypes = [ctypes.c_void_p]
    lib.inventory_get_object_weight.restype = u16
    lib.savegame_save.restype = c16
    lib.savegame_load.restype = c16
    assert lib.gameloop_load_data(str(GAME).encode())
    assert lib.gameloop_start_new_game(str(GAME).encode())
    return Game(lib)


def test_save_and_load_round_trip(game, tmp_path):
    lib = game.lib
    recruit_and_hold_object(game)
    seed = game.value("g_random_seed", u32)
    assert lib.savegame_save(str(tmp_path).encode()) == 1
    assert (tmp_path / SAVE).exists()
    # the file keeps the seed from before the random header words
    before = {**state(game), "g_random_seed": seed}
    hand = before["g_leader_hand_object"]

    assert lib.gameloop_start_new_game(str(GAME).encode())
    assert game.value("g_party_champion_count", u16) == 0
    assert lib.savegame_load(str(tmp_path).encode()) == LOADED
    after = state(game)

    # the file keeps the leader load without the object in the hand; starting the game adds it again
    leader = before["g_leader_index"]
    weight = lib.inventory_get_object_weight(hand)
    load = struct.unpack_from(">H", before["champions"][leader], 0x110)[0]
    assert struct.unpack_from(">H", after["champions"][leader], 0x110)[0] == load - weight
    after["champions"][leader] = before["champions"][leader]
    assert after == before
    assert ctypes.c_bool.in_dll(lib, "g_saved_game_exists").value == 1


def test_save_keeps_a_backup(game, tmp_path):
    lib = game.lib
    assert lib.savegame_save(str(tmp_path).encode()) == 1
    first = (tmp_path / SAVE).read_bytes()
    assert lib.savegame_save(str(tmp_path).encode()) == 1
    assert (tmp_path / BACKUP).read_bytes() == first
    (tmp_path / SAVE).unlink()
    assert lib.savegame_load(str(tmp_path).encode()) == LOADED
    assert (tmp_path / SAVE).read_bytes() == first and not (tmp_path / BACKUP).exists()


def test_portraits_survive_save_and_load(game, tmp_path):
    """The file keeps all four portrait bitmaps, also ones that are not in graphic 26."""
    lib = game.lib
    keep, saved = tmp_path / "keep", tmp_path / "saved"
    keep.mkdir()
    saved.mkdir()
    assert lib.savegame_save(str(keep).encode()) == 1
    count = u16.in_dll(lib, "g_party_champion_count")
    count.value = 4
    portraits = (ctypes.c_uint8 * (4 * 32 * 29)).in_dll(lib, "g_champion_portraits")
    for i in range(len(portraits)):
        portraits[i] = (i * 7 + i // 32) & 15
    before = bytes(portraits)
    assert lib.savegame_save(str(saved).encode()) == 1
    ctypes.memset(portraits, 0, len(before))
    assert lib.savegame_load(str(saved).encode()) == LOADED
    assert count.value == 4 and bytes(portraits) == before
    assert lib.savegame_load(str(keep).encode()) == LOADED


def test_load_fails_gracefully(game, tmp_path):
    lib = game.lib
    assert lib.savegame_load(str(tmp_path).encode()) == MISSING
    assert lib.savegame_save(str(tmp_path).encode()) == 1
    good = (tmp_path / SAVE).read_bytes()
    for bad in (good[:600], good[:-100] + bytes(100), good[:0x180] + bytes([good[0x180] ^ 1]) + good[0x181:], b"\0" * 4000):
        (tmp_path / SAVE).write_bytes(bad)
        assert lib.savegame_load(str(tmp_path).encode()) == DAMAGED
    (tmp_path / SAVE).write_bytes(good)
    assert lib.savegame_load(str(tmp_path).encode()) == LOADED


def test_dm_load_raises_the_active_group_count_to_110(game, tmp_path):
    lib = game.lib
    lib.savefile_decode.argtypes = [ctypes.c_char_p, ctypes.c_uint32, ctypes.POINTER(SaveFile)]
    lib.savefile_decode.restype = ctypes.c_bool
    lib.savefile_encode.argtypes = [ctypes.POINTER(SaveFile), ctypes.POINTER(u32)]
    lib.savefile_encode.restype = ctypes.c_void_p
    lib.savefile_free.argtypes = [ctypes.POINTER(SaveFile)]
    assert lib.savegame_save(str(tmp_path).encode()) == 1
    good = (tmp_path / SAVE).read_bytes()
    plain = decode(lib, good)
    groups = 20
    blocks = [bytearray(b) for b in plain["blocks"]]
    struct.pack_into(">H", blocks[0], 46, groups)
    blocks[1] = blocks[1][: groups * ACTIVE_GROUP_BYTES]
    sizes = [len(b) for b in blocks]
    (tmp_path / SAVE).write_bytes(encode(lib, plain["header"], blocks, plain["portraits"], plain["dungeon"], sizes))
    assert lib.savegame_load(str(tmp_path).encode()) == LOADED
    assert u16.in_dll(lib, "g_max_active_group_count").value == 110
    (tmp_path / SAVE).write_bytes(good)
    assert lib.savegame_load(str(tmp_path).encode()) == LOADED


def test_original_load_routines_accept_the_file(game, tmp_path):
    lib = game.lib
    assert lib.savegame_save(str(tmp_path).encode()) == 1
    file = (tmp_path / SAVE).read_bytes()
    plain = decode(lib, file)
    emu = Emu(load_image(parse(KAOS.read_bytes()), 0x100000))
    scratch, filebuf = 0x300000, 0x400000
    emu.write(filebuf, file)
    emu.write(G_LOAD_FROM_MEMORY, struct.pack(">HII", 1, filebuf, len(file)))
    assert emu.call(SAVE_READ_HEADER, [(4, scratch)]) & 0xFFFF
    header = plain["header"]
    assert header[0x12B] == 5 and struct.unpack_from(">H", header, 0x176)[0] == 3
    for i, block in enumerate(plain["blocks"]):
        key, checksum = struct.unpack_from(">HH", header[0x136 + 2 * i : 0x136 + 2 * i + 2] + header[0x156 + 2 * i : 0x156 + 2 * i + 2])
        assert emu.call(SAVE_READ_CHECKED_BLOCK, [(4, scratch), (2, len(block)), (2, key), (2, checksum)]) & 0xFFFF == 1
        assert emu.read(scratch, len(block)) == block
    dungeon = plain["dungeon"]
    assert struct.unpack(">H", dungeon[-2:])[0] == sum(dungeon[:-2]) & 0xFFFF


KEY_CTRL_S = 8595
LEFT_PRESS, LEFT_RELEASE = 2, 4
NO_CHOICE = 99
ZONE_DIALOG_CHOICE = {"cancel": 457, "save": 460}  # in-game dialog with 3 choices, relative to the viewport
ENDGAME_TABLE = 0x1031D4
VIEWPORT_ORIGIN = 0x10293A


def click(lib, x, y):
    lib.command_process_click(x, y, LEFT_PRESS)
    lib.command_process_click(x, y, LEFT_RELEASE)


def dialog_waits(lib):
    return ctypes.c_int16.in_dll(lib, "g_dialog_choice").value == NO_CHOICE


def kill_party(game):
    for i in range(game.value("g_party_champion_count", u16)):
        game.champions[i].current_health = 0
        game.lib.champion_kill(i)
    assert game.value("g_party_dead") == 1


@pytest.fixture
def quit_reset(game):
    yield
    ctypes.c_bool.in_dll(game.lib, "g_quit_requested").value = 0
    ctypes.c_bool.in_dll(game.lib, "g_return_to_entrance").value = 0
    ctypes.c_void_p.in_dll(game.lib, "g_frame_hook").value = None


def test_save_command_runs_the_dialog(game, tmp_path, quit_reset):
    lib = game.lib
    assert lib.gameloop_start_new_game(str(GAME).encode())
    lib.startend_set_save_dir(str(tmp_path).encode())
    choices = ["save", "cancel"]
    origin = game.origin

    def action(frames):
        if choices and dialog_waits(lib):
            box = Box()
            assert lib.layout_get_zone_box(ZONE_DIALOG_CHOICE[choices.pop(0)], 10000, 10000, ctypes.byref(box))
            click(lib, origin[0] + box.x + box.w // 2, origin[1] + box.y + box.h // 2)

    frames = Frames(lib, action)
    game.key(KEY_CTRL_S)
    game.run(2)
    frames.remove()
    assert not choices
    assert (tmp_path / SAVE).exists()
    assert game.value("g_game_frozen") == 0
    assert game.value("g_saved_game_exists") == 1


def test_party_death_returns_to_the_entrance_headless(game, quit_reset):
    lib = game.lib
    assert lib.gameloop_start_new_game(str(GAME).encode())
    recruit_and_hold_object(game)
    kill_party(game)
    lib.gameloop_tick()
    assert game.value("g_return_to_entrance") == 1 and game.value("g_quit_requested") == 0
    assert game.value("g_game_screen_inactive") == 1


def test_party_death_restart_loads_the_saved_game(game, tmp_path, quit_reset):
    lib = game.lib
    assert lib.gameloop_start_new_game(str(GAME).encode())
    recruit_and_hold_object(game)
    lib.startend_set_save_dir(str(tmp_path).encode())
    assert lib.savegame_save(str(tmp_path).encode()) == 1
    saved_time = game.value("g_game_time", u32)
    game.run(20)
    kill_party(game)
    restart = []

    def action(frames):
        if not restart and frames.count > 540:  # 240 frames after the death, 300 on THE END
            x1, x2, y1, y2 = (lib.kaos_s16(ENDGAME_TABLE + 2 + 2 * i) for i in range(4))
            click(lib, (x1 + x2) // 2, (y1 + y2) // 2)
            restart.append(frames.count)
        elif restart and dialog_waits(lib):
            lib.command_process_key(C.KEY_RETURN)

    frames = Frames(lib, action)
    lib.gameloop_tick()
    frames.remove()
    assert restart and game.value("g_quit_requested") == 0 and game.value("g_return_to_entrance") == 0
    assert game.value("g_party_dead") == 0
    assert game.value("g_party_champion_count", u16) == 1
    assert game.champions[0].current_health > 0
    assert game.value("g_game_time", u32) == saved_time
    assert game.value("g_game_screen_inactive") == 0


def test_fuse_sequence_ends_the_game(game, quit_reset):
    lib = game.lib
    assert lib.gameloop_start_new_game(str(GAME).encode())
    recruit_and_hold_object(game)
    lib.map_find_thing.restype = u16
    spot = None
    for level in range(1, 14):
        lib.group_remove_all_active_groups()
        lib.map_set_current_map_and_party_map(level)
        lib.group_add_all_active_groups()
        width, height = game.value("g_current_map_width"), game.value("g_current_map_height")
        groups = {(x, y) for x in range(width) for y in range(height) if lib.map_find_thing(Location(x, y), C.THING_GROUP) != 0xFFFE}
        spot = next(
            ((x - STEP[d][0], y - STEP[d][1], d, (x, y)) for x, y in sorted(groups) for d in range(4)
             if 0 <= x - STEP[d][0] < width and 0 <= y - STEP[d][1] < height
             and lib.rec_square_type(lib.map_get_square(x - STEP[d][0], y - STEP[d][1])) == 1 and (x - STEP[d][0], y - STEP[d][1]) not in groups),
            None)
        if spot:
            break
    assert spot, "no group next to a corridor"
    px, py, direction, lord_square = spot
    for name, v in (("g_party_map_x", px), ("g_party_map_y", py), ("g_party_direction", direction)):
        ctypes.c_int16.in_dll(lib, name).value = v
    lord = lib.map_find_thing(Location(*lord_square), C.THING_GROUP)
    lib.startend_run_fuse_sequence()
    assert game.value("g_game_won") == 1 and game.value("g_return_to_entrance") == 1 and game.value("g_quit_requested") == 0
    assert lib.rec_write(lord)[4] == 26  # Grey Lord
    # the other groups are gone; the test group is not caged and can walk away and go too
    assert {(x, y) for x in range(width) for y in range(height) if lib.map_find_thing(Location(x, y), C.THING_GROUP) != 0xFFFE} <= {lord_square}
    assert lib.gameloop_start_new_game(str(GAME).encode())  # a new game draws fluxcages again for the other tests
    assert game.value("g_do_not_draw_fluxcages") == 0 and game.value("g_game_won") == 0


STEP = [(0, -1), (1, 0), (0, 1), (-1, 0)]  # east, north steps of each direction


SLEEP_CLICK = (187, 35)  # ZZZ in the open inventory


def test_new_game_after_sleep_runs_at_normal_speed(game):
    lib = game.lib
    assert lib.gameloop_start_new_game(str(GAME).encode())
    recruit_and_hold_object(game)
    game.key(0x5000)  # F1
    game.run()
    game.click(*SLEEP_CLICK)
    assert game.value("g_party_is_sleeping") == 1
    assert lib.gameloop_start_new_game(str(GAME).encode())
    start = game.value("g_game_time", u32)
    game.run(130)
    assert game.value("g_game_time", u32) - start == 10


def test_endgame_quit_button_shows_the_credits_then_the_entrance(game, tmp_path, quit_reset):
    lib = game.lib
    assert lib.gameloop_start_new_game(str(GAME).encode())
    recruit_and_hold_object(game)
    assert lib.savegame_save(str(tmp_path).encode()) == 1
    kill_party(game)
    clicked = []

    def action(frames):
        if not clicked and frames.count > 540:
            x1, x2, y1, y2 = (lib.kaos_s16(ENDGAME_TABLE + 12 + 2 + 2 * i) for i in range(4))
            click(lib, (x1 + x2) // 2, (y1 + y2) // 2)
            clicked.append(frames.count)
        elif clicked and frames.count > clicked[0] + 10:
            lib.command_process_key(C.KEY_RETURN)  # any key ends the credits

    frames = Frames(lib, action)
    lib.gameloop_tick()
    frames.remove()
    assert clicked and frames.count < 2000
    assert game.value("g_return_to_entrance") == 1 and game.value("g_quit_requested") == 0
