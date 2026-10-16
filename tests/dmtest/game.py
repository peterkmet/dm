# SPDX-License-Identifier: GPL-3.0-or-later
"""The game harness of the tests: the ctypes structures of the C port, the Game driver, and helpers that set up a game."""

import ctypes
import random
import struct
from collections import deque

import pytest

from dmtest import constants as C
from dmtest.emu import kaos_emu, need, var
from dmtest.markers import GAME

c16, u16, u8, u32 = ctypes.c_int16, ctypes.c_uint16, ctypes.c_uint8, ctypes.c_uint32
STEP_EAST, STEP_NORTH = (0, 1, 0, -1), (-1, 0, 1, 0)
VANILLA, ADVANCED, TURNBASED = C.GAME_MODE_VANILLA, C.GAME_MODE_ADVANCED, C.GAME_MODE_TURNBASED
ZONE_RESURRECT = 570  # the resurrect button of the resurrect/reincarnate panel; no C constant
PARTY_BYTES, EVENT_BYTES, ACTIVE_GROUP_BYTES, CHAMPION_BYTES = 88, 12, 16, 0x140

# Original addresses (docs/reference/kaos-symbols.csv).
CHAMPIONS, COUNT, SLEEPING, GAME_TIME, SEED = 0x101F54, 0x1001E8, 0x1001DE, 0x1001F8, 0x100EF8
LAST_ATTACK, LAST_MOVE, DIRECTION = 0x10314E, 0x103152, 0x1001EA
CURRENT_MAP, THING_DATA, PENDING_DAMAGE, PENDING_WOUNDS = 0x101CBC, 0x101CEA, 0x102462, 0x10246A
VIEWPORT_ORIGIN = 0x10293A
RTS = b"\x4e\x75"
STUBS = {0x112880: RTS, 0x112D2C: RTS, 0x1098A6: RTS, 0x10999E: RTS,  # draw state, draw all, message print, new line
         0x111440: b"\x42\x6d\xc0\x7e" + RTS}  # champion_wake_up: clr.w g_party_is_sleeping
THING_BUF, MAP_BUF = 0x200000, 0x208000
# (thing type, record count, record size)
THINGS = [(5, 46, 4), (6, 58, 4), (7, 1, 4), (8, 21, 4), (9, 1, 8), (10, 53, 4)]

# Globals of libdm that hold a truth value: C bool, one byte.
BOOL_GLOBALS = frozenset({
    "g_party_is_sleeping",
    "g_party_dead",
    "g_cheat_light",
    "g_cheat_immortal",
    "g_cheat_food",
    "g_leader_empty_handed",
    "g_use_rope_to_climb_down_pit",
    "g_create_launcher_projectile",
    "g_fuse_sequence_requested",
    "g_saved_game_exists",
    "g_game_won",
    "g_advanced_features",
    "g_automap_open",
    "g_game_screen_inactive",
    "g_stop_waiting_for_input",
    "g_game_time_ticking",
    "g_game_frozen",
    "g_mouse_pointer_hidden",
    "g_input_received",
    "g_quit_requested",
    "g_restart_requested",
    "g_return_to_entrance",
    "g_action_area_contains_icons",
    "g_do_not_draw_fluxcages",
    "g_facing_alcove",
    "g_facing_fountain",
    "g_facing_vi_altar",
})


def global_type(name, default=ctypes.c_int16):
    """The ctypes type of a libdm global: c_bool for a truth flag, else default."""
    return ctypes.c_bool if name in BOOL_GLOBALS else default


class Box(ctypes.Structure):
    _fields_ = [("x", c16), ("y", c16), ("w", c16), ("h", c16)]


class Bitmap(ctypes.Structure):
    _fields_ = [("w", u16), ("h", u16), ("px", ctypes.POINTER(ctypes.c_uint8))]


class Skill(ctypes.Structure):
    _fields_ = [("temporary_experience", c16), ("experience", ctypes.c_int32)]


class Champion(ctypes.Structure):
    _fields_ = [
        ("name", ctypes.c_char * 8), ("title", ctypes.c_char * 20),
        ("direction", u8), ("cell", u8), ("action_index", u8), ("symbol_step", u8), ("symbols", ctypes.c_char * 5),
        ("direction_maximum_damage_received", u8), ("maximum_damage_received", u8), ("poison_event_count", u8),
        ("enable_action_event_index", c16), ("hide_damage_received_event_index", c16),
        ("attributes", u16), ("wounds", u16),
        ("current_health", c16), ("maximum_health", c16), ("current_stamina", c16), ("maximum_stamina", c16),
        ("current_mana", c16), ("maximum_mana", c16), ("action_defense", c16), ("food", c16), ("water", c16),
        ("statistics", (u8 * 3) * 7), ("skills", Skill * 20), ("slots", u16 * 30), ("load", u16), ("shield_defense", c16), ("magic_map", u8 * 6),
    ]


class Party(ctypes.Structure):
    _fields_ = [
        ("magical_light_amount", c16), ("thieves_eye", u8), ("footprints", u8),
        ("shield_defense", c16), ("fire_shield_defense", c16), ("spell_shield_defense", c16),
        ("scent_count", u8), ("freeze_life_ticks", u8), ("first_scent_index", u8), ("last_scent_index", u8),
        ("scents", u16 * 24), ("scent_strengths", u8 * 24), ("invisibility", u8),
    ]


class TimelineEvent(ctypes.Structure):
    _fields_ = [("map_time", ctypes.c_uint32), ("type", u8), ("priority", u8), ("b", u16), ("c", u16)]


class Timeline(ctypes.Structure):
    _fields_ = [
        ("events", ctypes.c_void_p), ("heap", ctypes.c_void_p), ("capacity", ctypes.c_int16),
        ("count", u16), ("high_water", u16), ("first_unused", ctypes.c_int16), ("watchdog_time", ctypes.c_int32),
    ]


class Location(ctypes.Structure):
    _fields_ = [("x", c16), ("y", c16)]


class MeleeAttack(ctypes.Structure):
    _fields_ = [("champion_index", c16), ("group", u16), ("creature_index", c16), ("at", Location),
                ("hit_probability", u16), ("damage_factor", u16), ("skill_index", c16)]


class DirectionPair(ctypes.Structure):
    _fields_ = [("primary", c16), ("secondary", c16)]


class Position(ctypes.Structure):
    _fields_ = [("map", c16), ("x", c16), ("y", c16)]


class RecruitData(ctypes.Structure):
    _fields_ = [
        ("name", ctypes.c_char * 8), ("title", ctypes.c_char * 20), ("male", ctypes.c_uint8),
        ("current_health", ctypes.c_int16), ("maximum_health", ctypes.c_int16),
        ("current_stamina", ctypes.c_int16), ("maximum_stamina", ctypes.c_int16),
        ("current_mana", ctypes.c_int16), ("maximum_mana", ctypes.c_int16),
        ("statistics", (ctypes.c_uint8 * 3) * 7), ("experience", ctypes.c_int32 * 20),
    ]


def champions(lib):
    return (Champion * 4).in_dll(lib, "g_champions")


def pack(lib, ch):
    buf = ctypes.create_string_buffer(0x140)
    lib.champion_pack(ctypes.byref(ch), buf)
    return buf.raw


class World:
    """The same champions, things, and globals in the C library and in the emulator."""

    def __init__(self, lib, seed):
        need(lib)
        self.lib, self.rng = lib, random.Random(seed)
        self.emu = kaos_emu()
        for addr, code in STUBS.items():
            self.emu.write(addr, code)
        self.buffers, self.things = [], []
        thing_data = (ctypes.c_void_p * 16).in_dll(lib, "g_thing_data")
        addr = THING_BUF
        for t, n, size in THINGS:
            data = bytearray()
            for i in range(n):
                rec = bytearray(size)
                struct.pack_into(">H", rec, 0, 0xFFFE)
                r = self.rng.getrandbits(8)
                if t == 8:
                    rec[2], rec[3] = (r & 0x80) | i, self.rng.getrandbits(8)
                elif t == 9:
                    struct.pack_into(">H", rec, 2, 0x1400 | 40)
                elif t != 7:
                    rec[2], rec[3] = self.rng.getrandbits(8), (r & 0x80) | i
                data += rec
                self.things.append(t << 10 | i)
            if t == 5:  # chest contents: weapons 40 -> 41
                struct.pack_into(">H", data, 40 * 4, 0x1400 | 41)
            buf = ctypes.create_string_buffer(bytes(data))
            self.buffers.append(buf)
            thing_data[t] = ctypes.addressof(buf)
            self.emu.write(addr, data)
            self.emu.write(THING_DATA + 4 * t, struct.pack(">I", addr))
            addr += 0x400
        self.map = ctypes.create_string_buffer(16)
        ctypes.c_void_p.in_dll(lib, "g_current_map").value = ctypes.addressof(self.map)
        self.emu.write(CURRENT_MAP, struct.pack(">I", MAP_BUF))
        ctypes.memset(ctypes.addressof(var(lib, Party, "g_party")), 0, ctypes.sizeof(Party))
        for name in ("g_candidate_champion_ordinal", "g_inventory_champion_ordinal"):
            var(lib, c16, name).value = 0
        self.globals(sleeping=0, time=1000, attack=0, move=0, difficulty=0)

    def globals(self, sleeping, time, attack, move, difficulty, direction=0):
        lib, emu = self.lib, self.emu
        var(lib, ctypes.c_bool, "g_party_is_sleeping").value = sleeping
        var(lib, ctypes.c_uint32, "g_game_time").value = time
        var(lib, ctypes.c_int32, "g_last_creature_attack_time").value = attack
        var(lib, ctypes.c_int32, "g_last_party_movement_time").value = move
        var(lib, c16, "g_party_direction").value = direction
        self.map[12] = difficulty << 4
        emu.write(SLEEPING, struct.pack(">h", sleeping))
        emu.write(GAME_TIME, struct.pack(">I", time))
        emu.write(LAST_ATTACK, struct.pack(">II", attack & 0xFFFFFFFF, move & 0xFFFFFFFF))
        emu.write(DIRECTION, struct.pack(">h", direction))
        emu.write(MAP_BUF + 12, bytes([difficulty << 4]))

    def thing(self, empty=0.3):
        return 0xFFFF if self.rng.random() < empty else self.rng.choice(self.things)

    def champion(self):
        rng, ch = self.rng, Champion()
        ch.name = b"TEST"
        ch.direction, ch.cell = rng.randrange(4), rng.randrange(4)
        ch.wounds = rng.choice([0, 0, rng.getrandbits(6)])
        ch.maximum_health = rng.randint(1, 999)
        ch.current_health = rng.choice([0, rng.randint(1, ch.maximum_health), ch.maximum_health])
        ch.maximum_stamina = rng.randint(2, 9999)
        ch.current_stamina = rng.randint(0, ch.maximum_stamina)
        ch.maximum_mana = rng.randint(0, 900)
        ch.current_mana = rng.randint(0, ch.maximum_mana + 20)
        ch.action_defense, ch.shield_defense = rng.randint(-20, 40), rng.randint(0, 60)
        ch.food, ch.water = rng.randint(-1100, 2100), rng.randint(-1100, 2100)
        for s in range(7):
            mx = rng.randint(1, 255)
            ch.statistics[s][:] = [mx, rng.choice([mx, rng.randint(0, 255)]), rng.randint(0, mx)]
        for s in range(20):
            ch.skills[s].temporary_experience = rng.choice([0, rng.randint(-5, 33000)])
            ch.skills[s].experience = rng.choice([0, rng.randint(0, 1 << rng.randint(8, 24))])
        for s in range(30):
            ch.slots[s] = self.thing()
        ch.load = rng.randint(0, 900)
        return ch

    def put_champions(self, count):
        lib, emu = self.lib, self.emu
        cs = champions(lib)
        for i in range(4):
            cs[i] = self.champion()
            emu.write(CHAMPIONS + 0x140 * i, pack(lib, cs[i]))
        var(lib, u16, "g_party_champion_count").value = count
        emu.write(COUNT, struct.pack(">H", count))
        self.zero_pending()

    def zero_pending(self):
        (c16 * 4).in_dll(self.lib, "g_champion_pending_damage")[:] = [0] * 4
        (u16 * 4).in_dll(self.lib, "g_champion_pending_wounds")[:] = [0] * 4
        self.emu.write(PENDING_DAMAGE, bytes(16))

    def seed(self, value):
        var(self.lib, ctypes.c_uint32, "g_random_seed").value = value
        self.emu.write(SEED, struct.pack(">I", value))

    def check_state(self):
        lib, emu = self.lib, self.emu
        cs = champions(lib)
        for i in range(4):
            assert pack(lib, cs[i]) == bytes(emu.read(CHAMPIONS + 0x140 * i, 0x140)), f"champion {i}"
        assert var(lib, ctypes.c_uint32, "g_random_seed").value == struct.unpack(">I", emu.read(SEED, 4))[0]
        assert var(lib, ctypes.c_bool, "g_party_is_sleeping").value == struct.unpack(">h", emu.read(SLEEPING, 2))[0]
        assert list((c16 * 4).in_dll(lib, "g_champion_pending_damage")) == list(struct.unpack(">4h", emu.read(PENDING_DAMAGE, 8)))
        assert list((u16 * 4).in_dll(lib, "g_champion_pending_wounds")) == list(struct.unpack(">4H", emu.read(PENDING_WOUNDS, 8)))


class Game:
    def __init__(self, lib):
        self.lib = lib
        self.boxes = (Box * 6).in_dll(lib, "g_dungeon_view_clickable_boxes")
        self.champions = (Champion * 4).in_dll(lib, "g_champions")
        self.origin = lib.kaos_s16(VIEWPORT_ORIGIN), lib.kaos_s16(VIEWPORT_ORIGIN + 2)

    def value(self, name, kind=c16):
        return global_type(name, kind).in_dll(self.lib, name).value

    def position(self):
        return [self.value(n) for n in ("g_party_map_x", "g_party_map_y", "g_party_direction")]

    def run(self, frames=13):
        for _ in range(frames):
            if self.lib.gameloop_run_frame():
                self.lib.gameloop_tick()

    def until(self, condition, limit=400):
        for _ in range(limit):
            if condition():
                return True
            self.run(1)
        return condition()

    def key(self, key):
        self.lib.command_process_key(key)

    def click(self, x, y):
        self.lib.command_process_click(x, y, C.CLICK_LEFT_PRESS)
        self.lib.command_process_click(x, y, C.CLICK_LEFT_RELEASE)
        self.run()

    def click_box(self, box, viewport=True):
        x, y = box.x + box.w // 2, box.y + box.h // 2
        if viewport:
            x, y = x + self.origin[0], y + self.origin[1]
        self.click(x, y)

    def click_zone(self, zone):
        box = Box()
        assert self.lib.layout_get_zone_box(zone, 10000, 10000, ctypes.byref(box))
        self.click_box(box)

    def face(self, direction):
        while self.position()[2] != direction:
            current = self.position()[2]
            self.key(C.KEY_HELP if (direction - current) & 3 == 1 else C.KEY_DEL)
            assert self.until(lambda: self.position()[2] != current)

    def walk(self, path):
        for (x0, y0), (x1, y1) in zip(path, path[1:]):
            self.face(next(d for d in range(4) if (x0 + STEP_EAST[d], y0 + STEP_NORTH[d]) == (x1, y1)))
            assert self.until(lambda: self.position()[:2] == [x0, y0])
            self.key(C.KEY_UP)
            assert self.until(lambda: self.position()[:2] == [x1, y1])

    def mirror_cell(self, x, y):
        thing = self.lib.map_get_square_first_thing(x, y)
        while thing not in (0xFFFE, 0xFFFF):
            data = self.lib.rec_write(thing)
            if thing >> 10 & 15 == C.THING_SENSOR and (data[2] << 8 | data[3]) & 0x7F == C.SENSOR_WALL_CHAMPION_PORTRAIT:
                return thing >> 14
            thing = self.lib.rec_thing_next(thing)
        return None

    def path_to_mirror(self, skip=()):
        """Shortest corridor path to a square whose wall ahead holds a mirror facing it, and that direction."""
        start = tuple(self.position()[:2])
        previous, queue = {start: None}, deque([start])
        while queue:
            x, y = queue.popleft()
            for d in range(4):
                nx, ny = x + STEP_EAST[d], y + STEP_NORTH[d]
                if (nx, ny) not in skip and self.mirror_cell(nx, ny) == (d + 2) & 3:
                    path, square = [], (x, y)
                    while square:
                        path.append(square)
                        square = previous[square]
                    return path[::-1], d
                if self.lib.rec_square_type(self.lib.map_get_square(nx, ny)) == C.SQUARE_CORRIDOR and (nx, ny) not in previous:
                    previous[nx, ny] = (x, y)
                    queue.append((nx, ny))
        raise AssertionError("no mirror reachable")


def start_game(lib):
    """A new game of game/ in lib, with the types that Game needs."""
    lib.gameloop_load_data.restype = ctypes.c_bool
    lib.gameloop_start_new_game.restype = ctypes.c_bool
    lib.gameloop_get_pointer.restype = ctypes.c_bool
    lib.ui_get_slot_box_zone.restype = ctypes.c_bool
    lib.rec_write.restype = ctypes.POINTER(ctypes.c_uint8)
    lib.map_get_square_first_thing.restype = u16
    lib.rec_thing_next.restype = u16
    lib.map_get_square.restype = ctypes.c_uint8
    lib.kaos_s16.restype = c16
    assert lib.gameloop_load_data(str(GAME).encode())
    assert lib.gameloop_start_new_game(str(GAME).encode())
    return Game(lib)


def recruit_first_champion(game):
    """Walks to the nearest mirror and resurrects its champion."""
    game.run(26)
    path, direction = game.path_to_mirror()
    game.walk(path)
    game.face(direction)
    game.run()
    game.click_box(game.boxes[5])
    game.click_zone(ZONE_RESURRECT)
    assert game.value("g_party_champion_count", u16) == 1


def recruit_and_hold_object(game):
    path, direction = game.path_to_mirror()
    game.walk(path)
    game.face(direction)
    game.run()
    game.click_box(game.boxes[5])
    game.click_zone(ZONE_RESURRECT)
    assert game.value("g_party_champion_count", u16) == 1
    box = Box()
    assert game.lib.ui_get_slot_box_zone(C.SLOT_HAND_ACTION, 0, ctypes.byref(box))
    game.click_box(box, viewport=False)
    assert game.value("g_leader_hand_object", u16) != 0xFFFF
    game.run(50)


# The saved globals of the game state, with their ctypes types.
GLOBALS = [
    ("g_game_time", u32), ("g_random_seed", u32), ("g_leader_hand_object", u16), ("g_party_champion_count", u16),
    ("g_party_map_x", c16), ("g_party_map_y", c16), ("g_party_direction", c16), ("g_party_map_index", c16),
    ("g_leader_index", c16), ("g_magic_caster_champion_index", c16), ("g_active_group_count", u16),
    ("g_last_creature_attack_time", ctypes.c_int32), ("g_last_party_movement_time", ctypes.c_int32),
    ("g_disabled_movement_ticks", c16), ("g_projectile_disabled_movement_ticks", c16),
    ("g_last_projectile_disabled_movement_direction", c16), ("g_max_active_group_count", u16), ("g_game_id", u32),
]


def raw(lib, name, size):
    return ctypes.string_at(ctypes.addressof(ctypes.c_uint8.in_dll(lib, name)), size)


def raw_array(lib, name, size):
    return ctypes.string_at(ctypes.c_void_p.in_dll(lib, name).value, size)


def state(game):
    """The saved part of the game state: globals, champions, party, timeline, active groups, dungeon."""
    lib = game.lib
    values = {name: kind.in_dll(lib, name).value for name, kind in GLOBALS}
    timeline = Timeline.in_dll(lib, "g_timeline")
    events = timeline.capacity
    values.update(g_timeline_count=timeline.count, g_first_unused_event_index=timeline.first_unused, g_event_max_count=events,
                  g_event_high_water=timeline.high_water)
    packed = ctypes.create_string_buffer(CHAMPION_BYTES)
    champions = []
    for i in range(4):
        lib.champion_pack(ctypes.byref(game.champions[i]), packed)
        champions.append(packed.raw)
    size = u32()
    p = lib.dungeon_save(ctypes.byref(size))
    dungeon = ctypes.string_at(p, size.value)
    lib.free(p)
    return {
        **values,
        "champions": champions,
        "portraits": bytes((ctypes.c_uint8 * (4 * 32 * 29)).in_dll(lib, "g_champion_portraits"))[: values["g_party_champion_count"] * 32 * 29],
        "party": raw(lib, "g_party", PARTY_BYTES),
        "events": ctypes.string_at(timeline.events, events * EVENT_BYTES),
        "timeline": ctypes.string_at(timeline.heap, events * 2),
        "active_groups": raw_array(lib, "g_active_groups", values["g_max_active_group_count"] * ACTIVE_GROUP_BYTES),
        "dungeon": dungeon,
    }


def scrub_free_groups(lib):
    """Zero the bytes of free active-group entries: the port leaves them uninitialized, and the save copies them."""
    base = ctypes.c_void_p.in_dll(lib, "g_active_groups").value
    for i in range(ctypes.c_uint16.in_dll(lib, "g_max_active_group_count").value):
        entry = base + i * ACTIVE_GROUP_BYTES
        if c16.from_address(entry).value == -1:
            ctypes.memset(entry + 2, 0, ACTIVE_GROUP_BYTES - 2)


HOOK = ctypes.CFUNCTYPE(None)


class Frames:
    """A frame hook: counts the frames of the blocking sequences and runs an action in each."""

    def __init__(self, lib, action):
        self.lib, self.action, self.count = lib, action, 0
        self.hook = HOOK(self.frame)
        ctypes.c_void_p.in_dll(lib, "g_frame_hook").value = ctypes.cast(self.hook, ctypes.c_void_p).value

    def frame(self):
        self.count += 1
        self.action(self)
        if self.count > 5000:  # a sequence that does not end
            ctypes.c_bool.in_dll(self.lib, "g_quit_requested").value = 1

    def remove(self):
        ctypes.c_void_p.in_dll(self.lib, "g_frame_hook").value = None


def set_mode(game, mode, features=None):
    c16.in_dll(game.lib, "g_game_mode").value = mode
    ctypes.c_bool.in_dll(game.lib, "g_advanced_features").value = int(mode != VANILLA) if features is None else features


def press(game, key, frames=13):
    game.key(key)
    game.run(frames)


def setup_creatures(lib):
    """The types of the creature, projectile and spell functions."""
    event_p = ctypes.POINTER(TimelineEvent)
    for name, res, args in [
        ("projectile_get_impact_attack", u16, [u16, u16]), ("projectile_get_object_for_launcher_or_generator", u16, [c16]),
        ("projectile_create", None, [u16, c16, c16, c16, c16, u8, u8, u8]), ("map_get_square", u8, [c16, c16]),
        ("map_get_square_first_thing", u16, [c16, c16]), ("rec_thing_next", u16, [u16]), ("map_find_thing", u16, [Location, c16]),
        ("dungeon_map_width", c16, [c16]), ("dungeon_map_height", c16, [c16]), ("rec_square_byte", u8, [c16, c16, c16]),
        ("rec_square_type", c16, [u8]),
        ("map_get_first_thing_at", u16, [Position]),
        ("timeline_is_first_event_expired", ctypes.c_bool, []), ("timeline_extract_first_event", None, [event_p]),
        ("projectile_process_move_event", None, [event_p]), ("explosion_process_step_event", None, [event_p]),
        ("map_set_current_map", None, [c16]), ("rec_write", ctypes.POINTER(u8), [u16]),
        ("group_get_champion_damage", c16, [u16, u16]), ("group_get_directions_to_destination", DirectionPair, [Location, Location]),
        ("group_is_destination_visible_from_source", ctypes.c_bool, [c16, c16, c16, c16, c16]),
        ("group_get_resistance_adjusted_poison_attack", u16, [u16, c16]),
        ("group_get_generated", u16, [u16, c16, u16, c16, Location]),
        ("group_get_damage_all_creatures_outcome", c16, [u16, c16, c16, c16, ctypes.c_bool]),
        ("dungeon_load", ctypes.c_bool, [ctypes.c_char_p]),
        ("magic_get_champion_spell_cast_result", c16, [u16]), ("rec_find_unused_thing", u16, [u16]),
        ("object_get_icon_index", c16, [u16]), ("group_is_creature_allowed_on_map", ctypes.c_bool, [u16, c16]), ("champion_pack", None, [ctypes.POINTER(Champion), ctypes.c_char_p]),
    ]:
        f = getattr(lib, name)
        f.restype, f.argtypes = res, args
    return lib


def new_game(lib):
    need(lib, "Dungeon.DAT")
    assert lib.dungeon_load(str(GAME / "Dungeon.DAT").encode()) == 1
    lib.map_reset()
    lib.game_start_new()
    var(lib, c16, "g_party_light_level").value = 0  # the view sets it in the game; a bright view here


def tick(lib, seen=None):
    """One game loop tick: the map change, the timeline, then the game time."""
    lib.move_process_new_party_map()
    if seen is not None:
        timeline = ctypes.cast(Timeline.in_dll(lib, "g_timeline").events, ctypes.POINTER(TimelineEvent))
        heap = ctypes.cast(Timeline.in_dll(lib, "g_timeline").heap, ctypes.POINTER(u16))
        if Timeline.in_dll(lib, "g_timeline").count:
            seen.add(timeline[heap[0]].type)
    lib.event_process_expired()
    var(lib, ctypes.c_uint32, "g_game_time").value += 1


def things_of_type(lib, x, y, t):
    found, thing = [], lib.map_get_square_first_thing(x, y)
    while thing != 0xFFFE:
        if thing >> 10 & 15 == t:
            found.append(thing)
        thing = lib.rec_thing_next(thing)
    return found


def add_test_champion(lib):
    """Champion 0 as the only party member: TEST with fixed values and empty slots."""
    c = (Champion * 4).in_dll(lib, "g_champions")[0]
    c.name = b"TEST"
    c.current_health = c.maximum_health = 100
    c.current_stamina = c.maximum_stamina = 1000
    c.current_mana = c.maximum_mana = 100
    c.enable_action_event_index = c.hide_damage_received_event_index = -1
    c.statistics[1][:] = [50, 50, 30]
    c.statistics[3][:] = [100, 100, 30]
    for s in range(30):
        c.slots[s] = 0xFFFF
    var(lib, u16, "g_party_champion_count").value = 1
    var(lib, c16, "g_leader_index").value = 0
    return c


def place_party(lib, map_index, x, y, direction):
    for name, value in zip(("g_party_map_index", "g_party_map_x", "g_party_map_y", "g_party_direction"), (map_index, x, y, direction)):
        var(lib, c16, name).value = value
    lib.move_party_enter_dungeon()


def skeleton_map_and_squares(lib):
    """A map that allows skeletons, and two empty corridor squares on it, two squares apart in a straight line."""
    probe = lib.rec_find_unused_thing(C.THING_GROUP)
    lib.rec_group_set_creature_type(probe, C.CREATURE_SKELETON)
    maps = [m for m in range(lib.dungeon_map_count()) if lib.group_is_creature_allowed_on_map(probe, m)]
    data = lib.rec_write(probe)
    data[0] = data[1] = 0xFF  # free the probe record again
    for m in maps:
        empty = lambda x, y: (0 <= x < lib.dungeon_map_width(m) and 0 <= y < lib.dungeon_map_height(m)
                              and lib.rec_square_type(lib.rec_square_byte(m, x, y)) == 1 and lib.map_get_first_thing_at(Position(m, x, y)) == 0xFFFE)
        for x in range(lib.dungeon_map_width(m)):
            for y in range(lib.dungeon_map_height(m)):
                if empty(x, y) and empty(x, y + 1) and empty(x, y + 2):
                    return m, (x, y), (x, y + 2)
    return None


def events(lib, event_type):
    timeline = ctypes.cast(Timeline.in_dll(lib, "g_timeline").events, ctypes.POINTER(TimelineEvent))
    return [timeline[i] for i in range(Timeline.in_dll(lib, "g_timeline").high_water) if timeline[i].type == event_type]


def things(lib, m, x, y):
    t = lib.map_get_first_thing_at(Position(m, x, y))
    while t != 0xFFFE:
        yield t
        d = lib.rec_write(t)
        t = d[0] << 8 | d[1]


def mirrors(lib):
    """(portrait index, text thing) for each wall sensor of type 127; the text is on the square in front."""
    for m in range(lib.dungeon_map_count()):
        for x in range(lib.dungeon_map_width(m)):
            for y in range(lib.dungeon_map_height(m)):
                for t in things(lib, m, x, y):
                    if t >> 10 & 15 == 3 and (p := lib.recruit_portrait_index(t)) >= 0:
                        c = t >> 14
                        yield p, lib.recruit_text_thing(m, x + STEP_EAST[c], y + STEP_NORTH[c])


def zone_records(raw):
    ranges = struct.unpack_from(">H", raw, 2)[0]
    pairs = [struct.unpack_from(">hh", raw, 4 + 4 * i) for i in range(ranges)]
    off, out = 4 + 4 * ranges, []
    for first, last in pairs:
        n = (last - first + 1) * 8
        out.append((first, last, raw[off:off + n]))
        off += n
    return out


def setup_view(lib):
    """The types of the view and draw functions."""
    lib.layout_get_zone.restype = ctypes.c_bool
    lib.view_render.restype = ctypes.c_bool
    lib.draw_blit.argtypes = [ctypes.POINTER(Bitmap), ctypes.c_int16, ctypes.c_int16, ctypes.POINTER(Bitmap), ctypes.POINTER(Box), ctypes.c_int16, ctypes.c_int]
    lib.draw_shrink.argtypes = [ctypes.POINTER(Bitmap), ctypes.c_int16, ctypes.c_int16, ctypes.c_char_p, ctypes.POINTER(Bitmap)]
    lib.layout_get_zone.argtypes = [ctypes.c_int16, ctypes.c_int16, ctypes.c_int16, ctypes.POINTER(Box), ctypes.POINTER(ctypes.c_int16), ctypes.POINTER(ctypes.c_int16)]
    lib.gfx_get_item.restype = ctypes.POINTER(ctypes.c_uint8)
    lib.dungeon_load.restype = ctypes.c_bool
    lib.gfx_get_item.argtypes = [ctypes.c_int, ctypes.POINTER(ctypes.c_uint16)]
    lib.view_render.argtypes = [ctypes.c_int16] * 4
    return lib


def start_view(lib):
    """The dungeon of game/ loaded for the view, and the party start (map, x, y, direction)."""
    if not all((GAME / f).exists() for f in ("KAOS.FTL", "Graphics.DAT", "Dungeon.DAT")):
        pytest.skip("game files missing")
    assert lib.tables_load(str(GAME / "KAOS.FTL").encode())
    assert lib.gfx_open(str(GAME / "Graphics.DAT").encode())
    assert lib.layout_init() == 0
    assert lib.dungeon_load(str(GAME / "Dungeon.DAT").encode())
    party = [ctypes.c_int16.in_dll(lib, n).value for n in ("g_party_map_index", "g_party_map_x", "g_party_map_y", "g_party_direction")]
    return lib, party


def setup_ui(lib):
    """The types of the interface functions."""
    lib.view_render.restype = ctypes.c_bool
    lib.layout_get_zone_box.argtypes = [c16, c16, c16, ctypes.POINTER(Box)]
    lib.ui_command_from_mouse.argtypes = [c16, c16, c16, u16]
    lib.ui_command_from_mouse.restype = c16
    lib.view_render.argtypes = [c16] * 4
    lib.gfx_get_bitmap.restype = ctypes.POINTER(Bitmap)
    lib.ui_zone_box.argtypes = [c16, ctypes.POINTER(Box)]
    lib.message_print.argtypes = [c16, ctypes.c_char_p]
    lib.recruit_parse.argtypes = [u16, ctypes.POINTER(RecruitData)]
    lib.recruit_parse.restype = ctypes.c_bool
    lib.dungeon_load.restype = ctypes.c_bool
    lib.map_get_first_thing_at.restype = u16
    lib.rec_write.restype = ctypes.POINTER(u8)
    lib.recruit_portrait_index.argtypes = [u16]
    lib.recruit_text_thing.restype = u16
    return lib


def start_ui(lib):
    """A new game of game/ with the interface ready to draw."""
    if not all((GAME / f).exists() for f in ("KAOS.FTL", "Graphics.DAT", "Dungeon.DAT")):
        pytest.skip("game files missing")
    assert lib.tables_load(str(GAME / "KAOS.FTL").encode())
    assert lib.gfx_open(str(GAME / "Graphics.DAT").encode())
    assert lib.layout_init() == 0 and lib.text_init() == 0
    assert lib.dungeon_load(str(GAME / "Dungeon.DAT").encode())
    lib.game_start_new()
    assert lib.ui_init() == 0
    return lib


def recruit_from_mirrors(lib, n):
    """Fills champions 0..n-1 from real mirror texts, with objects in hands and body slots."""
    champions = (Champion * 4).in_dll(lib, "g_champions")
    found = sorted(mirrors(lib))[:n]
    weapons = list(range(8))
    for i, (portrait, text) in enumerate(found):
        r = RecruitData()
        assert lib.recruit_parse(text, ctypes.byref(r)) == 1
        c = Champion()
        c.name, c.title = r.name, r.title
        c.cell, c.direction = i, 0
        for f in ("current_health", "maximum_health", "current_stamina", "maximum_stamina", "current_mana", "maximum_mana"):
            setattr(c, f, getattr(r, f))
        c.current_health = c.maximum_health * (4 - i) // 4
        c.current_mana = c.maximum_mana // (i + 1)
        c.statistics = r.statistics
        for s in range(20):
            c.skills[s].experience = r.experience[s]
        c.slots[:] = [0xFFFF] * 30
        c.slots[0] = 5 << 10 | weapons[2 * i]
        if i != 2:
            c.slots[1] = 5 << 10 | weapons[2 * i + 1]
        c.slots[3] = 6 << 10 | i
        c.slots[13] = 10 << 10 | i
        c.food, c.water = 1500 - 900 * i, 1800 - 1000 * i
        c.load = 123 + 100 * i
        c.action_index = 0xFF
        c.enable_action_event_index = c.hide_damage_received_event_index = -1
        champions[i] = c
        lib.champion_set_portrait(i, portrait)
    c16.in_dll(lib, "g_leader_index").value = 0 if n else -1
    u16.in_dll(lib, "g_party_champion_count").value = n
    c16.in_dll(lib, "g_magic_caster_champion_index").value = -1
    return champions


def render(lib, name):
    """The dungeon view at the party start and the whole interface; saves a PNG in the scratchpad."""
    ctypes.memset(ctypes.addressof(ctypes.c_uint8.in_dll(lib, "fb")), 0, 320 * 200)
    party = [c16.in_dll(lib, n).value for n in ("g_party_map_index", "g_party_map_x", "g_party_map_y", "g_party_direction")]
    assert lib.view_render(*party)
    lib.ui_set_palette(None)
    lib.ui_draw_interface()
    fb = bytes((ctypes.c_uint8 * (320 * 200)).in_dll(lib, "fb"))
    return fb


def reset(lib):
    c16.in_dll(lib, "g_inventory_champion_ordinal").value = 0
    c16.in_dll(lib, "g_acting_champion_ordinal").value = 0
    ctypes.c_bool.in_dll(lib, "g_action_area_contains_icons").value = 1
    lib.message_clear()
