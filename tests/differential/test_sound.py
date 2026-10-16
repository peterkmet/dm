import ctypes
import random
import struct

import pytest

from dmtest import constants as C
from dmtest.lib import load_lib
from dmtest.markers import ROOT, sdl3_installed

from dmtools.emu import Emu
from dmtools.ftl import load_image, parse

GAME = ROOT / "game"
G_SOUNDS = 0x100D52
G_PARTY = {"g_party_direction": 0x1001EA, "g_party_map_x": 0x1001EC, "g_party_map_y": 0x1001EE}
G_PARTY_MAP_INDEX_ADDR, G_CURRENT_MAP_INDEX = 0x1001F0, 0x101C60
G_PENDING_VOLUMES, G_PENDING_SOUNDS = 0x10093E, 0x100942
SOUND_COMPARE_LOUDNESS, SOUND_REQUEST_PLAY, SOUND_GET_VOLUME = 0x10871A, 0x108DF4, 0x109054
CLOCK, RATE = 3546895, 44100
SCRATCH = 0x400000

pytestmark = [
    pytest.mark.skipif(not sdl3_installed(), reason="clang or SDL3 not installed"),
    pytest.mark.skipif(not (GAME / "KAOS.FTL").exists() or not (GAME / "Graphics.DAT").exists(), reason="game files missing"),
]
u8p = ctypes.POINTER(ctypes.c_uint8)


@pytest.fixture(scope="module")
def lib():
    lib = load_lib()
    lib.sound_get_sample.restype = ctypes.POINTER(ctypes.c_int8)
    lib.sound_get_sample.argtypes = [ctypes.c_int16, ctypes.POINTER(ctypes.c_uint16), ctypes.POINTER(ctypes.c_uint16)]
    lib.gfx_get_item.restype = u8p
    lib.gfx_get_item.argtypes = [ctypes.c_int, ctypes.POINTER(ctypes.c_uint16)]
    lib.sound_compare_loudness.restype = ctypes.c_int16
    lib.sound_get_volume.restype = ctypes.c_bool
    lib.sound_get_volume.argtypes = [ctypes.c_int16, ctypes.c_int16, u8p]
    lib.sound_request_play.argtypes = [ctypes.c_int16] * 3 + [ctypes.c_uint16]
    lib.sound_play_sample.argtypes = [ctypes.c_int16, u8p]
    assert lib.kaos_load(str(GAME / "KAOS.FTL").encode()) == 0
    assert lib.tables_init() == 0
    assert lib.gfx_open(str(GAME / "Graphics.DAT").encode()) > 0
    return lib


@pytest.fixture(scope="module")
def emu():
    return Emu(load_image(parse((GAME / "KAOS.FTL").read_bytes()), 0x100000))


def sounds():
    img = load_image(parse((GAME / "KAOS.FTL").read_bytes()), 0x100000)
    o = G_SOUNDS - img.base
    return [img.data[o + 12 * i : o + 12 * i + 12] for i in range(35)]


def sample(lib, sound):
    n, period = ctypes.c_uint16(), ctypes.c_uint16()
    p = lib.sound_get_sample(sound, ctypes.byref(n), ctypes.byref(period))
    return bytes(ctypes.cast(p, u8p)[: n.value]), period.value


def test_sample_decode(lib):
    for i, entry in enumerate(sounds()):
        graphic, rate = struct.unpack(">h", entry[:2])[0], entry[3]
        size = ctypes.c_uint16()
        item = ctypes.cast(lib.gfx_get_item(graphic, ctypes.byref(size)), ctypes.c_void_p).value
        n, period = ctypes.c_uint16(), ctypes.c_uint16()
        data = ctypes.cast(lib.sound_get_sample(i, ctypes.byref(n), ctypes.byref(period)), ctypes.c_void_p).value
        header = struct.unpack(">H", ctypes.string_at(item, 2))[0]
        assert (data, n.value, header, period.value) == (item + 2, size.value - 2, size.value - 2, 0x11C60 // rate), i


def expected_mix(voices, frames):
    """voices: (samples, period, volume, channel); Paula holds each sample for one period, no interpolation."""
    out = [[0, 0] for _ in range(frames)]
    for data, period, vol, ch in voices:
        for f in range(frames):
            i = f * CLOCK // (period * RATE)
            if i < len(data):
                out[f][ch in (1, 2)] += struct.unpack("b", data[i : i + 1])[0] * vol * 2
    return [v for pair in out for v in pair]


def mix(lib, frames):
    buf = (ctypes.c_int16 * (2 * frames))()
    lib.sound_mix(buf, frames)
    return list(buf)


def vols(left, right):
    return (ctypes.c_uint8 * 2)(left, right)


def test_mix_single_voice(lib):
    lib.sound_stop_all()
    lib.sound_demo_play(0)
    data, period = sample(lib, 0)
    frames = len(data) * 9  # beyond the end: 44100 Hz / 5457 Hz is about 8 frames per sample
    assert mix(lib, frames) == expected_mix([(data, period, 64, 3), (data, period, 64, 2)], frames)
    assert mix(lib, 4) == [0] * 8


def test_mix_replaces_quieter_pair(lib):
    lib.sound_stop_all()
    lib.sound_play_sample(5, vols(40, 0))  # pair A: channels 3, 2
    lib.sound_play_sample(7, vols(1, 1))  # pair B: channels 0, 1
    lib.sound_play_sample(9, vols(0, 64))  # both busy: replaces B, the quieter pair
    (a, pa), (c, pc) = sample(lib, 5), sample(lib, 9)
    frames = 2000
    assert mix(lib, frames) == expected_mix([(a, pa, 40, 3), (a, pa, 0, 2), (c, pc, 0, 0), (c, pc, 64, 1)], frames)


def test_compare_loudness_matches_original(lib, emu):
    for a in [(0, 0), (7, 7), (8, 7), (12, 13), (13, 13), (64, 0), (30, 30)]:
        for b in [(0, 14), (0, 15), (25, 0), (26, 0), (64, 64)]:
            emu.write(SCRATCH, bytes(a + b))
            want = ctypes.c_int16(emu.call(SOUND_COMPARE_LOUDNESS, [(4, SCRATCH), (4, SCRATCH + 2)])).value
            assert lib.sound_compare_loudness(vols(*a), vols(*b)) == want, (a, b)


def set_party(lib, emu, x, y, direction):
    for name, value in (("g_party_map_x", x), ("g_party_map_y", y), ("g_party_direction", direction)):
        ctypes.c_int16.in_dll(lib, name).value = value
        emu.write(G_PARTY[name], struct.pack(">h", value))


def test_get_volume_matches_original(lib, emu):
    for d in range(4):
        set_party(lib, emu, 15, 16, d)
        for x in range(1, 31, 2):
            for y in range(2, 32, 2):
                emu.write(SCRATCH, b"\xee\xee")
                want = emu.call(SOUND_GET_VOLUME, [(2, x), (2, y), (4, SCRATCH)]) & 0xFFFF
                v = vols(0xEE, 0xEE)
                assert (lib.sound_get_volume(x, y, v), bytes(v)) == (want, bytes(emu.read(SCRATCH, 2))), (d, x, y)


def test_request_queue_matches_original(lib, emu):
    rnd = random.Random(12)
    emu.write(G_PARTY_MAP_INDEX_ADDR, b"\0\0")
    emu.write(G_CURRENT_MAP_INDEX, b"\0\0")
    pending = (ctypes.c_int16 * 2).in_dll(lib, "g_pending_sounds")
    volumes = (ctypes.c_uint8 * 4).in_dll(lib, "g_pending_sound_volumes")
    for _ in range(40):
        set_party(lib, emu, 10, 10, rnd.randrange(4))
        pending[0] = pending[1] = -1
        volumes[:] = [0] * 4  # earlier tests can leave volumes behind
        emu.write(G_PENDING_SOUNDS, b"\xff\xff\xff\xff")
        emu.write(G_PENDING_VOLUMES, bytes(4))
        for _ in range(6):
            s, x, y = rnd.randrange(35), rnd.randrange(-3, 24), rnd.randrange(-3, 24)
            emu.call(SOUND_REQUEST_PLAY, [(2, s), (2, x), (2, y), (2, 1)])
            lib.sound_request_play(s, x, y, 1)
            want = struct.unpack(">2h", bytes(emu.read(G_PENDING_SOUNDS, 4))), bytes(emu.read(G_PENDING_VOLUMES, 4))
            assert (tuple(pending), bytes(volumes)) == want
