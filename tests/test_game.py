import ctypes
import random
import shutil
import struct
import subprocess
from pathlib import Path

import pytest

from dmtools.emu import Emu
from dmtools.ftl import load_image, parse

ROOT = Path(__file__).parent.parent
GAME = ROOT / "game"
SEED, GAME_TIME = 0x100EF8, 0x1001F8
RANDOM16, RANDOM_BIT, RANDOM_2BITS = 0x1076A0, 0x1076BC, 0x1076DA
MIN16, MAX16, BOUNDED_VALUE, SCALE_VALUE = 0x107652, 0x107668, 0x10767E, 0x1076F8
MOVE_RELATIVE = 0x10A85C
STAMINA_ADJUSTED, ADJUSTED_ATTACK, ADJUST_STATISTIC, IS_LUCKY = 0x110F4C, 0x110F88, 0x10F860, 0x110FC8
EVENT_MAX_COUNT, EVENT_HIGH_WATER, TIMELINE_EVENTS, TIMELINE = 0x103174, 0x103176, 0x103178, 0x10317C
TIMELINE_COUNT, FIRST_UNUSED = 0x103180, 0x103182
ADD_EVENT, EXTRACT_FIRST, IS_EXPIRED = 0x11F59C, 0x11F634, 0x11F662
BUF = 0x200000  # free emulator RAM above the KAOS image

pytestmark = pytest.mark.skipif(not shutil.which("clang"), reason="clang not installed")
c16, c32 = ctypes.c_int16, ctypes.c_uint32


class Skill(ctypes.Structure):
    _fields_ = [("temporary_experience", ctypes.c_int16), ("experience", ctypes.c_int32)]


class Champion(ctypes.Structure):
    _fields_ = [
        ("name", ctypes.c_char * 8), ("title", ctypes.c_char * 20),
        ("direction", ctypes.c_uint8), ("cell", ctypes.c_uint8),
        ("action_index", ctypes.c_uint8), ("symbol_step", ctypes.c_uint8), ("symbols", ctypes.c_char * 5),
        ("direction_maximum_damage_received", ctypes.c_uint8), ("maximum_damage_received", ctypes.c_uint8),
        ("poison_event_count", ctypes.c_uint8),
        ("enable_action_event_index", c16), ("hide_damage_received_event_index", c16),
        ("attributes", ctypes.c_uint16), ("wounds", ctypes.c_uint16),
        ("current_health", c16), ("maximum_health", c16), ("current_stamina", c16), ("maximum_stamina", c16),
        ("current_mana", c16), ("maximum_mana", c16), ("action_defense", c16), ("food", c16), ("water", c16),
        ("statistics", (ctypes.c_uint8 * 3) * 7), ("skills", Skill * 20), ("slots", ctypes.c_uint16 * 30),
        ("load", ctypes.c_uint16), ("shield_defense", c16), ("magic_map", ctypes.c_uint8 * 6),
    ]


class Event(ctypes.Structure):
    _fields_ = [("map_time", c32), ("type", ctypes.c_uint8), ("priority", ctypes.c_uint8),
                ("b", ctypes.c_uint16), ("c", ctypes.c_uint16)]

    def be(self):
        return struct.pack(">IBBHH", self.map_time, self.type, self.priority, self.b, self.c)


class ActiveGroup(ctypes.Structure):
    _fields_ = [("group_index", c16), ("rest", ctypes.c_uint8 * 14)]


@pytest.fixture(scope="module")
def lib():
    subprocess.run(["make", "-C", "port", "../build/libdm.dylib"], cwd=ROOT, check=True, capture_output=True)
    lib = ctypes.CDLL(str(ROOT / "build/libdm.dylib"))
    for name in ("random16", "random_bit", "random_2bits"):
        getattr(lib, name).restype = ctypes.c_uint16
    lib.scale_value.restype = ctypes.c_int32
    lib.dungeon_load.restype = ctypes.c_bool
    lib.scale_value.argtypes = [ctypes.c_uint16, c16, ctypes.c_uint16]
    for name in ("min16", "max16", "bounded_value", "timeline_add_event",
                 "game_get_stamina_adjusted_value"):
        getattr(lib, name).restype = c16
    lib.min16.argtypes = lib.max16.argtypes = [c16, c16]
    lib.bounded_value.argtypes = [c16, c16, c16]
    lib.game_get_stamina_adjusted_value.argtypes = [ctypes.POINTER(Champion), c16]
    lib.game_get_statistic_adjusted_attack.restype = ctypes.c_uint16
    lib.game_get_statistic_adjusted_attack.argtypes = [ctypes.POINTER(Champion), ctypes.c_uint16, ctypes.c_uint16]
    lib.game_adjust_statistic_current_value.argtypes = [ctypes.POINTER(Champion), ctypes.c_uint16, c16]
    lib.game_is_lucky.argtypes = [ctypes.POINTER(Champion), ctypes.c_uint16]
    lib.game_is_lucky.restype = lib.timeline_is_first_event_expired.restype = ctypes.c_bool
    lib.game_update_map_coordinates_after_relative_movement.argtypes = [c16, c16, c16, ctypes.POINTER(c16), ctypes.POINTER(c16)]
    return lib


def need(lib, *names):
    missing = [n for n in ("KAOS.FTL", *names) if not (GAME / n).exists()]
    if missing:
        pytest.skip(f"missing game/{missing[0]}")
    assert lib.kaos_load(str(GAME / "KAOS.FTL").encode()) == 0
    assert lib.tables_init() == 0


@pytest.fixture
def emu(lib):
    need(lib)
    return Emu(load_image(parse((GAME / "KAOS.FTL").read_bytes()), 0x100000))


def word(emu, addr):
    return struct.unpack(">H", emu.read(addr, 2))[0]


def put(emu, addr, fmt, *values):
    emu.write(addr, struct.pack(fmt, *values))


@pytest.mark.parametrize("seed", [0, 1, 0x12345678, 0xFFFFFFFF])
def test_random_matches_original(lib, emu, seed):
    seed_c = c32.in_dll(lib, "g_random_seed")
    seed_c.value = seed
    put(emu, SEED, ">I", seed)
    rng = random.Random(seed)
    for _ in range(300):
        name, addr, mask = rng.choice([("random16", RANDOM16, 0xFFFF), ("random_bit", RANDOM_BIT, 1), ("random_2bits", RANDOM_2BITS, 3)])
        value = getattr(lib, name)()
        assert value == emu.call(addr) & 0xFFFF and value <= mask
        assert seed_c.value == struct.unpack(">I", emu.read(SEED, 4))[0]


def s16(v):
    return (v & 0xFFFF) - ((v & 0x8000) << 1)


def test_math_matches_original(lib, emu):
    rng = random.Random(1)
    edge = [-32768, -1, 0, 1, 10, 15, 32767]
    for _ in range(300):
        a, b, c = (rng.choice(edge + [rng.randint(-32768, 32767)]) for _ in range(3))
        assert lib.min16(a, b) == s16(emu.call(MIN16, [(2, a), (2, b)]))
        assert lib.max16(a, b) == s16(emu.call(MAX16, [(2, a), (2, b)]))
        assert lib.bounded_value(a, b, c) == s16(emu.call(BOUNDED_VALUE, [(2, a), (2, b), (2, c)]))
        shift = rng.choice([0, 1, 3, 7, 15, 31, 32, 40])
        assert lib.scale_value(a & 0xFFFF, shift, c & 0xFFFF) & 0xFFFFFFFF == emu.call(SCALE_VALUE, [(2, a), (2, shift), (2, c)])
    assert lib.bounded_value(0, 15, 10) == 10 and lib.scale_value(100, 3, 50) == 625


def test_relative_movement_matches_original(lib, emu):
    for d in range(4):
        for fwd in (-2, 0, 1, 3):
            for right in (-1, 0, 2):
                x, y = c16(10), c16(20)
                lib.game_update_map_coordinates_after_relative_movement(d, fwd, right, ctypes.byref(x), ctypes.byref(y))
                put(emu, BUF, ">hh", 10, 20)
                emu.call(MOVE_RELATIVE, [(2, d), (2, fwd), (2, right), (4, BUF), (4, BUF + 2)])
                assert (x.value, y.value) == struct.unpack(">hh", emu.read(BUF, 4))


def champion_bytes(ch):
    data = bytearray(0x140)
    struct.pack_into(">hh", data, 0x38, ch.current_stamina, ch.maximum_stamina)
    for s in range(7):
        data[0x46 + 3 * s: 0x49 + 3 * s] = bytes(ch.statistics[s])
    return bytes(data)


def test_champion_helpers_match_original(lib, emu):
    rng = random.Random(2)
    seed_c = c32.in_dll(lib, "g_random_seed")
    for i in range(400):
        ch = Champion()
        ch.maximum_stamina = rng.randint(0, 2000)
        ch.current_stamina = rng.randint(0, ch.maximum_stamina)
        for s in range(7):
            ch.statistics[s][:] = [rng.choice([0, 1, 30, 120, 127, 128, 150, 170, 255, rng.randint(0, 255)]) for _ in range(3)]
        stat, value = rng.randrange(7), rng.randint(-200, 200)
        emu.write(BUF, champion_bytes(ch))
        assert lib.game_get_stamina_adjusted_value(ch, value) == s16(emu.call(STAMINA_ADJUSTED, [(4, BUF), (2, value)]))
        attack = rng.randint(0, 0xFFFF)
        assert lib.game_get_statistic_adjusted_attack(ch, stat, attack) == emu.call(ADJUSTED_ATTACK, [(4, BUF), (2, stat), (2, attack)]) & 0xFFFF
        lib.game_adjust_statistic_current_value(ch, stat, value)
        emu.call(ADJUST_STATISTIC, [(4, BUF), (2, stat), (2, value)])
        assert ch.statistics[stat][1] == emu.read(BUF + 0x47 + 3 * stat, 1)[0]
        seed_c.value = i * 7919
        put(emu, SEED, ">I", i * 7919)
        emu.write(BUF, champion_bytes(ch))
        percentage = rng.choice([0, 10, 50, 99, 100, 300])
        assert lib.game_is_lucky(ch, percentage) == s16(emu.call(IS_LUCKY, [(4, BUF), (2, percentage)]))
        assert ch.statistics[0][1] == emu.read(BUF + 0x47, 1)[0]
        assert seed_c.value == struct.unpack(">I", emu.read(SEED, 4))[0]


def timeline_state(lib):
    n = c16.in_dll(lib, "g_event_max_count").value
    events = ctypes.cast(ctypes.c_void_p.in_dll(lib, "g_timeline_events"), ctypes.POINTER(Event))
    heap = ctypes.cast(ctypes.c_void_p.in_dll(lib, "g_timeline"), ctypes.POINTER(ctypes.c_uint16))
    count = ctypes.c_uint16.in_dll(lib, "g_timeline_count").value
    first = c16.in_dll(lib, "g_first_unused_event_index").value
    free, i = [], first
    while i != -1 and len(free) <= n:
        free.append(i)
        i = s16(events[i].map_time)
    live = {i: events[i].be() for i in range(n) if events[i].type}
    return count, first, ctypes.c_uint16.in_dll(lib, "g_event_high_water").value, heap[:count], live, free


def emu_timeline_state(emu, n):
    count, first = word(emu, TIMELINE_COUNT), s16(word(emu, FIRST_UNUSED))
    raw = emu.read(BUF, 10 * n)
    free, i = [], first
    while i != -1 and len(free) <= n:
        free.append(i)
        i = s16(struct.unpack_from(">H", raw, 10 * i)[0])
    heap = list(struct.unpack(f">{count}H", emu.read(BUF + 0x1000, 2 * count)))
    live = {i: raw[10 * i: 10 * i + 10] for i in range(n) if raw[10 * i + 4]}
    return count, first, word(emu, EVENT_HIGH_WATER), heap, live, free


def test_timeline_matches_original(lib, emu):
    n, arg = 40, BUF + 0x2000
    c16.in_dll(lib, "g_event_max_count").value = n
    c32.in_dll(lib, "g_game_time").value = 0
    lib.timeline_init()
    put(emu, EVENT_MAX_COUNT, ">hHIIHhI", n, 0, BUF, BUF + 0x1000, 0, 0, 0)
    put(emu, GAME_TIME, ">I", 0)
    emu.write(BUF, b"".join(struct.pack(">HHIH", i + 1 if i < n - 1 else 0xFFFF, 0, 0, 0) for i in range(n)))
    emu.write(arg, Event(300, 53).be())
    emu.call(ADD_EVENT, [(4, arg)])
    assert timeline_state(lib) == emu_timeline_state(emu, n)
    rng = random.Random(3)
    types = [1, 1, 2, 5, 6, 6, 7, 8, 9, 10, 10, 11, 12, 20, 25, 48, 53, 60]
    for _ in range(1500):
        count = word(emu, TIMELINE_COUNT)
        if count and (count >= n - 1 or rng.random() < 0.35):
            got = Event()
            lib.timeline_extract_first_event(ctypes.byref(got))
            emu.call(EXTRACT_FIRST, [(4, arg)])
            assert got.be() == emu.read(arg, 10)
        else:
            e = Event(rng.randrange(3) << 24 | rng.randrange(8), rng.choice(types), rng.randrange(4),
                      rng.randrange(2) << 8 | rng.randrange(2), rng.randrange(4) << 8 | rng.randrange(3))
            emu.write(arg, e.be())
            assert lib.timeline_add_event(ctypes.byref(e)) == s16(emu.call(ADD_EVENT, [(4, arg)]))
        assert timeline_state(lib) == emu_timeline_state(emu, n)
    for t in range(0, 12):
        c32.in_dll(lib, "g_game_time").value = t
        put(emu, GAME_TIME, ">I", t)
        assert lib.timeline_is_first_event_expired() == emu.call(IS_EXPIRED) & 0xFFFF


def test_timeline_order(lib):
    c16.in_dll(lib, "g_event_max_count").value = 10
    c32.in_dll(lib, "g_game_time").value = 0
    lib.timeline_init()  # watchdog: time 300, type 53
    for t, typ, prio in [(5, 20, 0), (5, 25, 0), (2, 11, 3), (5, 20, 1), (2, 11, 1)]:
        lib.timeline_add_event(ctypes.byref(Event(t, typ, prio)))
    order = []
    for _ in range(6):
        e = Event()
        lib.timeline_extract_first_event(ctypes.byref(e))
        order.append((e.map_time, e.type, e.priority))
    assert order == [(2, 11, 3), (2, 11, 1), (5, 25, 0), (5, 20, 1), (5, 20, 0), (300, 53, 0)]


def test_game_new(lib):
    need(lib, "Dungeon.DAT")
    from dmcsb import open_dungeon

    assert lib.dungeon_load(str(GAME / "Dungeon.DAT").encode()) == 1
    loc = open_dungeon(str(GAME / "Dungeon.DAT")).hdr["InitialPartyLocation"]
    c32.in_dll(lib, "g_random_seed").value = 1234
    lib.game_start_new()
    party = [c16.in_dll(lib, f"g_party_{n}").value for n in ("map_x", "map_y", "direction", "map_index")]
    assert party == [loc & 0x1F, loc >> 5 & 0x1F, loc >> 10 & 3, 0]
    assert ctypes.c_uint16.in_dll(lib, "g_party_champion_count").value == 0
    assert c16.in_dll(lib, "g_leader_index").value == -1
    assert ctypes.c_uint16.in_dll(lib, "g_leader_hand_object").value == 0xFFFF
    assert c32.in_dll(lib, "g_game_time").value == 0
    count, first, high_water, heap, live, free = timeline_state(lib)
    assert (count, first, high_water, heap, live) == (1, 1, 1, [0], {0: Event(300, 53).be()})
    assert len(free) == c16.in_dll(lib, "g_event_max_count").value - 1
    groups = ctypes.cast(ctypes.c_void_p.in_dll(lib, "g_active_groups"), ctypes.POINTER(ActiveGroup))
    assert ctypes.c_uint16.in_dll(lib, "g_max_active_group_count").value == 110
    assert all(groups[i].group_index == -1 for i in range(110))
    seed, r = 1234, []
    for _ in range(2):
        seed = (seed * 0xBB40E62D + 11) & 0xFFFFFFFF
        r.append(seed >> 8 & 0xFFFF)
    assert c32.in_dll(lib, "g_game_id").value == r[0] * r[1] & 0xFFFFFFFF
