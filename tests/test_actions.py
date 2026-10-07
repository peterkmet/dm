"""Champion actions and the champion timeline events (port/src/action.c, timeline_process.c)."""

import ctypes
import struct

import pytest

from test_champion import CHAMPIONS, COUNT, Champion, World, pack
from test_creatures import (THING_EXPLOSION, THING_PROJECTILE, TimelineEvent, events, lib, new_game,  # noqa: F401 (lib: fixture)
                            place_party, recruit, s16, skeleton_map_and_squares, things_of_type, tick, var)

c16, u16, u8 = ctypes.c_int16, ctypes.c_uint16, ctypes.c_uint8

# Original addresses (docs/kaos-symbols.csv).
ACTION_PERFORMED, MELEE_DAMAGE, DISABLE_ACTION = 0x122892, 0x11D348, 0x111F82
SOUND_REQUEST_PLAY, GET_SQUARE, GROUP_GET_THING, GROUP_EVENTS = 0x108DF4, 0x10A8AC, 0x10BB5C, 0x10D7E4
ACTION_DAMAGE, PALETTE_INDEX, MAP_X = 0x103FF6, 0x1001E6, 0x1001EC
ACTION_INFO = 0x103EAC
RTS = b"\x4e\x75"
TICKS_AT, GROUP_BUF = 0x2F0000, 0x230000
# move.w 6(a7),TICKS_AT.l; rts: keeps the ticks argument of magic_disable_action.
CAPTURE_TICKS = b"\x33\xef\x00\x06" + struct.pack(">I", TICKS_AT) + RTS

THING_GROUP, THING_WEAPON, THING_JUNK = 4, 5, 10
WEAPON_DAGGER, WEAPON_SWORD, JUNK_BONES = 8, 10, 5
SLOT_HAND_READY, SLOT_HAND_ACTION, SLOT_QUIVER_LINE1_1 = 0, 1, 12
ACTION_BLOCK, ACTION_SWING, ACTION_SHOOT, ACTION_THROW = 1, 13, 32, 42
SKILL_SWING = 4
ATTR_ACTION_DISABLED, ATTR_ACTION_HAND = 0x0008, 0x8000
REDRAW_ACTION_AREA = 1
EVENT_ENABLE_ACTION, EVENT_HIDE_DAMAGE, EVENT_REBIRTH, EVENT_POISON = 11, 12, 13, 75
FAR = 60  # a square outside every map: a wall without things

# Actions whose result depends only on the champion and the action tables when the square in front is an empty wall.
PURE_ACTIONS = [0, 1, 2, 3, 4, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19, 24, 25, 26, 28, 29, 30, 31, 32, 36, 37, 41]
MELEE_ACTIONS = [2, 6, 7, 9, 13, 14, 15, 16, 17, 18, 19, 24, 25, 28, 29, 30, 31]


@pytest.fixture
def alib(lib):  # noqa: F811
    for name, res, args in [
        ("action_is_action_performed", ctypes.c_bool, [u16, c16]), ("champion_poison", None, [c16, u16]),
        ("group_get_melee_action_damage", c16, [c16, u16, c16, c16, c16, u16, u16, c16]),
        ("timeline_delete_event", None, [u16]), ("timeline_add_event", c16, [ctypes.POINTER(TimelineEvent)]),
        ("link_thing_to_list", None, [u16, u16, c16, c16]), ("tables_action_info", ctypes.c_void_p, [ctypes.c_int]),
    ]:
        f = getattr(lib, name)
        f.restype, f.argtypes = res, args
    return lib


def action_world(lib, seed):
    """A World whose square in front of the party is an empty wall, in the C library and in the emulator."""
    new_game(lib)  # the C timeline for magic_disable_action
    w = World(lib, seed)
    for addr, code in ((SOUND_REQUEST_PLAY, RTS), (GET_SQUARE, b"\x70\x00" + RTS), (GROUP_GET_THING, b"\x70\xfe" + RTS),
                       (GROUP_EVENTS, RTS), (DISABLE_ACTION, CAPTURE_TICKS)):
        w.emu.write(addr, code)
    for name in ("g_party_map_x", "g_party_map_y"):
        var(lib, c16, name).value = FAR
    w.emu.write(MAP_X, struct.pack(">hh", FAR, FAR))
    return w


def put_living_champions(w):
    w.put_champions(4)
    cs = (Champion * 4).in_dll(w.lib, "g_champions")
    for i in range(4):
        cs[i].current_health = max(1, cs[i].current_health)
        cs[i].enable_action_event_index = -1
        cs[i].slots[SLOT_HAND_READY] = 0xFFFF
        w.emu.write(CHAMPIONS + 0x140 * i, pack(w.lib, cs[i]))
    return cs


def same_champion(lib, w, i):
    """The C and emulator records are equal, except the event 11 index and the redraw flags."""
    cs = (Champion * 4).in_dll(lib, "g_champions")
    got, want = bytearray(pack(lib, cs[i])), bytearray(w.emu.read(CHAMPIONS + 0x140 * i, 0x140))
    for raw in (got, want):
        raw[0x2C:0x2E] = b"\0\0"
        raw[0x30] = 0
        raw[0x31] &= 0x77
    return got == want


def disabled_ticks(lib, i):
    """Ticks until the event 11 of the C champion, 0 without one. The event is deleted again."""
    c = (Champion * 4).in_dll(lib, "g_champions")[i]
    if c.enable_action_event_index < 0:
        return 0
    timeline = ctypes.cast(ctypes.c_void_p.in_dll(lib, "g_timeline_events"), ctypes.POINTER(TimelineEvent))
    ticks = (timeline[c.enable_action_event_index].map_time & 0xFFFFFF) - var(lib, ctypes.c_uint32, "g_game_time").value
    lib.timeline_delete_event(c.enable_action_event_index)
    c.enable_action_event_index = -1
    return ticks


@pytest.mark.parametrize("seed", range(4))
def test_action_matches_original(alib, seed):
    """Stamina, experience, disabled ticks, heal, and the action damage of each action without a target."""
    lib = alib
    w = action_world(lib, 60 + seed)
    for round_ in range(6):
        put_living_champions(w)
        w.globals(0, 5000, 0, 0, w.rng.randrange(4), w.rng.randrange(4))
        for action in PURE_ACTIONS:
            i = w.rng.randrange(4)
            var(lib, c16, "g_action_damage").value = 0
            w.emu.write(ACTION_DAMAGE, b"\0\0")
            w.emu.write(TICKS_AT, b"\0\0")
            w.seed(seed * 10007 + round_ * 101 + action)
            got = lib.action_is_action_performed(i, action)
            want = s16(w.emu.call(ACTION_PERFORMED, [(2, i), (2, action)]))
            assert got == want, (round_, action)
            assert disabled_ticks(lib, i) == struct.unpack(">H", w.emu.read(TICKS_AT, 2))[0], (round_, action)
            assert var(lib, c16, "g_action_damage").value == struct.unpack(">h", w.emu.read(ACTION_DAMAGE, 2))[0]
            assert var(lib, ctypes.c_uint32, "g_random_seed").value == struct.unpack(">I", w.emu.read(0x100EF8, 4))[0]
            assert same_champion(lib, w, i), (round_, action)


@pytest.mark.parametrize("seed", range(4))
def test_melee_damage_matches_original(alib, seed):
    """Hit probability and damage of a melee action on a creature that survives."""
    lib = alib
    w = action_world(lib, 70 + seed)
    group = ctypes.create_string_buffer(16)
    (ctypes.c_void_p * 16).in_dll(lib, "g_thing_data")[THING_GROUP] = ctypes.addressof(group)
    hits = 0
    for round_ in range(160):
        if round_ % 20 == 0:
            put_living_champions(w)
        w.globals(w.rng.choice([0, 0, 0, 1]), 5000, 0, 0, w.rng.randrange(6))
        palette = w.rng.randrange(6)
        var(lib, c16, "g_dungeon_view_palette_index").value = palette
        w.emu.write(PALETTE_INDEX, struct.pack(">h", palette))
        group[4], group[15] = w.rng.randrange(27), 3 << 5
        for creature in range(4):
            struct.pack_into(">H", group, 6 + 2 * creature, 0xF000)
        w.emu.write(GROUP_BUF, group.raw)
        action = w.rng.choice(MELEE_ACTIONS)
        hit = w.emu.read(ACTION_INFO + 2 * 44 + action, 1)[0] | w.rng.choice([0, 0x8000])
        factor = w.emu.read(ACTION_INFO + 44 + action, 1)[0]
        skill = w.emu.read(ACTION_INFO + 5 * 44 + action, 1)[0]
        i, creature = w.rng.randrange(4), w.rng.randrange(4)
        w.seed(seed * 7907 + round_)
        got = lib.group_get_melee_action_damage(i, THING_GROUP << 10, creature, FAR, FAR, hit, factor, skill)
        want = s16(w.emu.call(MELEE_DAMAGE, [(4, CHAMPIONS + 0x140 * i), (2, i), (4, GROUP_BUF), (2, creature), (2, FAR), (2, FAR),
                                             (2, hit), (2, factor), (2, skill)]))
        assert got == want, (round_, group[4], action)
        hits += got > 0
        assert group.raw == bytes(w.emu.read(GROUP_BUF, 16))
        assert var(lib, ctypes.c_uint32, "g_random_seed").value == struct.unpack(">I", w.emu.read(0x100EF8, 4))[0]
        assert same_champion(lib, w, i), round_
    assert hits > 0


# Scenarios on the loaded dungeon.


def new_object(lib, thing_type, object_type):
    thing = lib.get_unused_thing(thing_type)
    lib.dungeon_get_thing_data(thing)[3] = object_type
    return thing


@pytest.fixture
def fighter(alib):
    """A champion with a sword who faces a skeleton on the next square."""
    lib = alib
    new_game(lib)
    c = recruit(lib)
    m, party_square, _ = skeleton_map_and_squares(lib)
    x, y = party_square
    place_party(lib, m, x, y, 2)  # south
    assert lib.group_get_generated(12, 1, 0, 0, x, y + 1) == 0xFFFF  # a skeleton, active on the party map
    c.direction, c.cell = 2, 2  # front row when the party faces south
    c.slots[SLOT_HAND_ACTION] = new_object(lib, THING_WEAPON, WEAPON_SWORD)
    for s in range(20):
        c.skills[s].experience = 1 << 12
    return lib, c, (x, y)


@pytest.fixture
def party(alib):
    """A champion on the start square, without creatures nearby."""
    new_game(alib)
    c = recruit(alib)
    alib.move_party_enter_dungeon()
    x, y = (var(alib, c16, name).value for name in ("g_party_map_x", "g_party_map_y"))
    return alib, c, (x, y)


def test_sword_attack_hurts_group_and_gains_experience(fighter):
    lib, c, (x, y) = fighter
    group = lib.get_square_group(x, y + 1)
    health = struct.unpack(">H", bytes(lib.dungeon_get_thing_data(group)[6:8]))[0]
    experience = c.skills[SKILL_SWING].experience
    damage = []
    for _ in range(20):
        lib.action_is_action_performed(0, ACTION_SWING)
        damage.append(var(lib, c16, "g_action_damage").value)
        group = lib.get_square_group(x, y + 1)
        if group == 0xFFFE or struct.unpack(">H", bytes(lib.dungeon_get_thing_data(group)[6:8]))[0] < health:
            break
    assert any(d > 0 for d in damage)
    assert c.skills[SKILL_SWING].experience > experience
    assert c.enable_action_event_index >= 0 and c.attributes & ATTR_ACTION_DISABLED


def test_back_row_champion_cannot_reach(fighter):
    lib, c, _ = fighter
    c.cell = 0  # back right when the party faces south; champion 1 stands in front of it in cell 3
    other = (Champion * 4).in_dll(lib, "g_champions")[1]
    ctypes.memmove(ctypes.byref(other), ctypes.byref(c), ctypes.sizeof(Champion))
    other.cell = 3
    var(lib, u16, "g_party_champion_count").value = 2
    found = False
    for _ in range(10):
        lib.action_is_action_performed(0, ACTION_SWING)
        if var(lib, c16, "g_action_damage").value == -1:
            found = True
            break
    assert found


def test_throw_creates_projectile_and_event_11_refills_hand(fighter):
    lib, c, (x, y) = fighter
    c.slots[SLOT_HAND_ACTION] = new_object(lib, THING_WEAPON, WEAPON_DAGGER)
    c.slots[SLOT_QUIVER_LINE1_1] = second = new_object(lib, THING_WEAPON, WEAPON_DAGGER)
    assert lib.action_is_action_performed(0, ACTION_THROW) == 1
    assert things_of_type(lib, x, y, THING_PROJECTILE)
    assert c.slots[SLOT_HAND_ACTION] == 0xFFFF
    timeline = ctypes.cast(ctypes.c_void_p.in_dll(lib, "g_timeline_events"), ctypes.POINTER(TimelineEvent))
    event = timeline[c.enable_action_event_index]
    assert event.type == EVENT_ENABLE_ACTION and event.b >> 8 == SLOT_HAND_ACTION + 1
    for _ in range(5):
        tick(lib)
    assert c.enable_action_event_index == -1 and not c.attributes & ATTR_ACTION_DISABLED
    assert c.slots[SLOT_HAND_ACTION] == second and c.slots[SLOT_QUIVER_LINE1_1] == 0xFFFF


def test_event_11_ends_action_defense(party):
    lib, c, _ = party
    info = ctypes.cast(lib.tables_action_info(ACTION_BLOCK), ctypes.POINTER(u8 * 7)).contents
    defense = ctypes.c_int8(info[4]).value
    c.action_defense, c.action_index = defense, ACTION_BLOCK
    lib.action_is_action_performed(0, ACTION_BLOCK)
    ticks = info[0]
    assert c.attributes & ATTR_ACTION_DISABLED
    for _ in range(ticks):
        tick(lib)
    assert c.attributes & ATTR_ACTION_DISABLED  # still disabled until the last tick runs
    tick(lib)
    assert not c.attributes & ATTR_ACTION_DISABLED and c.action_defense == 0 and c.action_index == 255


def test_triggered_action_closes_menu_and_asks_for_action_area(party):
    lib, c, _ = party
    lib.action_did_click_trigger_action.restype, lib.action_did_click_trigger_action.argtypes = c16, [c16]
    redraw = ctypes.c_uint16.in_dll(lib, "g_redraw")
    redraw.value = 0
    var(lib, c16, "g_acting_champion_ordinal").value = 1
    (u8 * 3).in_dll(lib, "g_action_list_indices")[0] = ACTION_BLOCK
    lib.action_did_click_trigger_action(0)
    assert var(lib, c16, "g_acting_champion_ordinal").value == 0
    assert redraw.value & REDRAW_ACTION_AREA
    assert c.attributes & ATTR_ACTION_HAND


def test_poison_event_deals_damage_each_step(party):
    lib, c, _ = party
    pending = (c16 * 4).in_dll(lib, "g_champion_pending_damage")
    lib.champion_poison(0, 200)
    assert pending[0] == 200 >> 6 and c.poison_event_count == 1
    pending[0] = 0
    for _ in range(37):
        tick(lib)
    assert pending[0] == 199 >> 6 and c.poison_event_count == 1
    poison = events(lib, EVENT_POISON)
    assert len(poison) == 1 and poison[0].b == 198


def test_event_12_hides_damage(party):
    lib, c, _ = party
    now = var(lib, ctypes.c_uint32, "g_game_time").value
    event = TimelineEvent(map_time=var(lib, c16, "g_party_map_index").value << 24 | (now + 1), type=EVENT_HIDE_DAMAGE, priority=0)
    c.hide_damage_received_event_index = lib.timeline_add_event(ctypes.byref(event))
    tick(lib)
    tick(lib)
    assert c.hide_damage_received_event_index == -1


def test_vi_altar_rebirth(party):
    lib, c, (x, y) = party
    bones = new_object(lib, THING_JUNK, JUNK_BONES)
    lib.dungeon_get_thing_data(bones)[2] = 0  # charges: champion 0
    lib.link_thing_to_list(bones | 1 << 14, 0, x, y)
    c.current_health = 0
    now = var(lib, ctypes.c_uint32, "g_game_time").value
    event = TimelineEvent(map_time=var(lib, c16, "g_party_map_index").value << 24 | (now + 1), type=EVENT_REBIRTH, priority=0,
                          b=x << 8 | y, c=1 << 8 | 2)
    lib.timeline_add_event(ctypes.byref(event))
    tick(lib)
    tick(lib)
    assert things_of_type(lib, x, y, THING_EXPLOSION)  # the rebirth explosion
    for _ in range(5):
        tick(lib)
    assert all(t & 0x3FFF != bones for t in things_of_type(lib, x, y, THING_JUNK))
    for _ in range(2):
        tick(lib)
    assert c.current_health > 0
