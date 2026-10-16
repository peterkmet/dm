import ctypes
import json
import shutil

import pytest

from dmtest import constants as C
from dmtest.game import ADVANCED, TURNBASED, VANILLA, press, recruit_first_champion, set_mode, start_game
from dmtest.lib import load_lib
from dmtest.markers import GAME, needs_dm

c16, u16 = ctypes.c_int16, ctypes.c_uint16
KEY_1, KEY_2, KEY_3 = 0x0100, 0x0200, 0x0300
ORIGINAL, SUPER_NES, FM_TOWNS = 0, 1, 2
DM, CSB = 0, 1
CSB_PRISON_DUNGEON_ID, CSB_DUNGEON_ID = 12, 13
CSB_MAPS, CSB_MAP_SIZE = 10, 32
MIX_FRAMES = 44100
TRACK_02 = GAME / "music/FM-Towns,Track_02,Music.mp3"
MUSIC_MAP = GAME.parent / "reference/extracted/music_map.json"

pytestmark = [pytest.mark.skipif(not shutil.which("clang"), reason="clang not installed"), needs_dm]


@pytest.fixture(scope="module")
def game():
    """A new game with one recruited champion in a private copy of libdm."""
    game = start_game(load_lib())
    recruit_first_champion(game)
    game.run(30)
    yield game
    set_mode(game, VANILLA)


def var(game, name, kind=c16):
    return kind.in_dll(game.lib, name)


def update_at(game, map_index, x, y):
    var(game, "g_party_map_index").value, var(game, "g_party_map_x").value, var(game, "g_party_map_y").value = map_index, x, y
    game.lib.music_update()


def last(game):
    return var(game, "g_music_last_track").value


def csb_table(game):
    return (ctypes.c_uint8 * (CSB_MAPS * CSB_MAP_SIZE * CSB_MAP_SIZE)).in_dll(game.lib, "g_music_csb_square_tracks")


def csb_square(game, map_index, track):
    """The first square (x, y) of a track on a CSB map."""
    table = csb_table(game)
    for y in range(CSB_MAP_SIZE):
        for x in range(CSB_MAP_SIZE):
            if table[(map_index * CSB_MAP_SIZE + y) * CSB_MAP_SIZE + x] == track:
                return x, y
    raise AssertionError(f"track {track} not on map {map_index}")


@pytest.fixture(autouse=True)
def restore(game):
    names = ("g_party_map_index", "g_party_map_x", "g_party_map_y", "g_adventure")
    saved = [var(game, n).value for n in names]
    dungeon_id = var(game, "g_dungeon_id", u16).value
    yield
    game.lib.music_select(ORIGINAL)
    for n, v in zip(names, saved):
        var(game, n).value = v
    var(game, "g_dungeon_id", u16).value = dungeon_id


def mix(game, frames):
    out = (c16 * (2 * frames))()
    game.lib.sound_mix(out, frames)
    game.lib.music_mix(out, frames)
    return out


def test_csb_square_table_matches_the_research(game):
    table = list(csb_table(game))
    assert sum(1 for t in table if t) == 3798
    assert sum(1 for t in table[:CSB_MAP_SIZE * CSB_MAP_SIZE] if t == 2) == 217
    if not MUSIC_MAP.exists():
        pytest.skip("music_map.json missing")
    for m in json.loads(MUSIC_MAP.read_text())["fm_towns_csb"]["maps"]:
        for y, row in enumerate(m["grid_rows_y_then_x"]):
            start = (m["map"] * CSB_MAP_SIZE + y) * CSB_MAP_SIZE
            assert table[start:start + len(row)] == row


@pytest.mark.parametrize("mode", [VANILLA, ADVANCED, TURNBASED])
def test_music_keys_work_only_on_the_advanced_automap(game, mode):
    set_mode(game, mode)
    press(game, C.KEY_M)
    press(game, KEY_3)
    assert var(game, "g_music_soundtrack").value == (FM_TOWNS if mode == ADVANCED else ORIGINAL)
    press(game, KEY_2)
    assert var(game, "g_music_soundtrack").value == (SUPER_NES if mode == ADVANCED else ORIGINAL)
    press(game, KEY_1)
    assert var(game, "g_music_soundtrack").value == ORIGINAL
    press(game, C.KEY_M)
    set_mode(game, VANILLA)


def test_fm_towns_dm_plays_the_level_track(game):
    update_at(game, 0, 1, 1)
    game.lib.music_select(FM_TOWNS)
    assert last(game) == 2
    update_at(game, 1, 1, 1)
    assert last(game) == 8
    update_at(game, 1, 2, 1)
    assert last(game) == 8
    game.lib.music_select(ORIGINAL)
    assert var(game, "g_music_track").value == 0


@pytest.mark.skipif(not TRACK_02.exists(), reason="FM-Towns MP3 files missing")
def test_track_plays_one_time(game):
    update_at(game, 0, 1, 1)
    game.lib.music_select(FM_TOWNS)
    assert var(game, "g_music_track").value == 2
    assert any(mix(game, MIX_FRAMES))
    for _ in range(120):  # track 02 is 102 seconds
        mix(game, MIX_FRAMES)
    assert var(game, "g_music_track").value == 0
    assert not any(mix(game, MIX_FRAMES))
    game.lib.music_update()
    assert (var(game, "g_music_track").value, last(game)) == (0, 2)  # an ended track does not start again


@pytest.mark.skipif(not TRACK_02.exists(), reason="FM-Towns MP3 files missing")
def test_music_stop_after_sound_stop_all_stops_the_track_and_keeps_the_soundtrack(game):
    update_at(game, 0, 1, 1)
    game.lib.music_select(FM_TOWNS)
    game.lib.sound_stop_all()
    game.lib.music_stop()
    assert (var(game, "g_music_track").value, var(game, "g_music_soundtrack").value) == (0, FM_TOWNS)


@pytest.mark.skipif(not TRACK_02.exists(), reason="FM-Towns MP3 files missing")
def test_music_mix_clips(game):
    update_at(game, 0, 1, 1)
    game.lib.music_select(FM_TOWNS)
    mix(game, MIX_FRAMES)  # past the quiet start
    frames = 4096
    out = (c16 * (2 * frames))(*([32767, -32768] * frames))
    game.lib.music_mix(out, frames)
    assert all(v >= 0 for v in out[0::2]) and all(v <= 0 for v in out[1::2])  # no wrap to the other sign
    assert out[0::2].count(32767) < frames  # the music changed samples


def test_a_loaded_game_selects_original(game):
    update_at(game, 0, 1, 1)
    game.lib.music_select(FM_TOWNS)
    game.lib.gameloop_continue_loaded_game()
    assert (var(game, "g_music_soundtrack").value, var(game, "g_music_track").value) == (ORIGINAL, 0)


def test_super_nes_trigger_square_starts_its_track(game):
    update_at(game, 0, 1, 1)
    game.lib.music_select(SUPER_NES)
    assert last(game) == 0
    update_at(game, 0, 8, 4)
    assert last(game) == 1
    update_at(game, 2, 9, 17)
    assert last(game) == 2
    var(game, "g_music_last_track").value = 0
    update_at(game, 2, 9, 17)
    assert last(game) == 0  # no step: no new start


def test_super_nes_does_nothing_in_csb(game):
    var(game, "g_adventure").value = CSB
    game.lib.music_select(SUPER_NES)
    assert var(game, "g_music_soundtrack").value == ORIGINAL


def test_fm_towns_csb_zone_track_waits_for_the_current_track(game):
    var(game, "g_adventure").value = CSB
    var(game, "g_dungeon_id", u16).value = CSB_DUNGEON_ID
    update_at(game, 0, *csb_square(game, 0, 21))
    game.lib.music_select(FM_TOWNS)
    assert last(game) == 21
    var(game, "g_music_track").value = 21  # the track plays
    update_at(game, 0, *csb_square(game, 0, 7))
    assert (last(game), var(game, "g_music_requested_track").value) == (21, 7)
    table = csb_table(game)
    x0, y0 = next((x, y) for y in range(CSB_MAP_SIZE) for x in range(CSB_MAP_SIZE) if not table[y * CSB_MAP_SIZE + x])
    update_at(game, 0, x0, y0)
    assert var(game, "g_music_requested_track").value == 7  # a square with 0 changes nothing
    var(game, "g_music_track").value = 0  # the track ended
    game.lib.music_update()
    assert last(game) == 7
    var(game, "g_music_track").value = 7
    update_at(game, 1, *csb_square(game, 1, 7))
    assert (var(game, "g_music_track").value, last(game)) == (0, 7)  # a level change stops; the same track does not start again
    var(game, "g_dungeon_id", u16).value = CSB_PRISON_DUNGEON_ID
    game.lib.music_update()
    assert last(game) == 6


@pytest.mark.parametrize("adventure, soundtrack, won, track", [
    (DM, FM_TOWNS, 0, 13), (DM, FM_TOWNS, 1, 18), (DM, SUPER_NES, 1, 16), (DM, SUPER_NES, 0, 0),
    (CSB, FM_TOWNS, 0, 17), (CSB, FM_TOWNS, 1, 0), (DM, ORIGINAL, 0, 0),
])
def test_end_of_the_game_track(game, adventure, soundtrack, won, track):
    var(game, "g_adventure").value = adventure
    var(game, "g_music_soundtrack").value = soundtrack
    var(game, "g_music_last_track").value = 0
    game.lib.music_play_end(won)
    assert last(game) == track
