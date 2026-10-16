# SPDX-License-Identifier: GPL-3.0-or-later
import ctypes
import struct

import pytest

from dmtest import constants as C
from dmtest.emu import need, s16, var
from dmtest.game import (CHAMPIONS, STEP_EAST, STEP_NORTH, THING_BUF, Bitmap, Champion, Party, RecruitData, World, champions,
                         global_type, pack)
from dmtest.lib import load_lib
from dmtest.markers import GAME, needs_dm

# Original addresses (docs/reference/kaos-symbols.csv).
PARTY_SHIELD, SCENT_COUNT, SCENTS, SCENT_STRENGTHS = 0x101ED8, 0x101EDE, 0x101EE2, 0x101F12
CANDIDATE = 0x1001DC
MAP_X, MAP_Y, MAP_INDEX = 0x1001EC, 0x1001EE, 0x1001F0
LEVEL_UP_ADDR = 0x102490
SKILL_LEVEL, ADD_EXPERIENCE, MAX_LOAD, MOVE_TICKS, DEXTERITY = 0x110B20, 0x110C2E, 0x111056, 0x1110C2, 0x11112E
STRENGTH, WOUND_DEFENSE, PENDING, DAMAGE_ALL, TIME_EFFECTS = 0x1111B6, 0x1112EA, 0x111984, 0x111C3A, 0x11202C
WEIGHT, ICON = 0x10A4AC, 0x1079DC
ATTR_STATUS_BOX = 4096

pytestmark = [needs_dm]
c16, u16, u8 = ctypes.c_int16, ctypes.c_uint16, ctypes.c_uint8


@pytest.fixture(scope="module")
def lib():
    lib = load_lib()
    cp = ctypes.POINTER(Champion)
    sig = {
        "champion_get_skill_level": (c16, [c16, u16]), "champion_add_skill_experience": (None, [u16, u16, u16]),
        "champion_get_maximum_load": (u16, [cp]), "champion_get_movement_ticks": (c16, [cp]),
        "champion_get_dexterity": (c16, [cp]), "champion_get_strength": (c16, [c16, c16]),
        "champion_get_wound_defense": (c16, [c16, u16]), "champion_add_pending_damage_and_wounds": (c16, [c16, c16, u16, c16]),
        "champion_damage_all": (c16, [u16, u16, c16]), "champion_decrement_stamina": (None, [c16, c16]),
        "inventory_get_object_weight": (u16, [u16]), "object_get_icon_index": (c16, [u16]), "rec_find_unused_thing": (u16, [u16]),
        "champion_add_candidate_to_party": (None, [c16]), "champion_click_in_resurrect_reincarnate_panel": (None, [c16]),
        "inventory_put_object_in_leader_hand": (None, [u16]), "champion_kill": (None, [u16]),
        "champion_pack": (None, [cp, ctypes.c_char_p]), "champion_unpack": (None, [ctypes.c_char_p, cp]),
        "recruit_text_thing": (u16, [c16, c16, c16]), "recruit_portrait_index": (c16, [u16]),
        "dungeon_get_square_first_thing": (u16, [c16, c16, c16]), "rec_write": (ctypes.POINTER(u8), [u16]),
        "map_set_current_map_and_party_map": (None, [c16]), "dungeon_load": (ctypes.c_bool, [ctypes.c_char_p]),
    }
    for name, (res, args) in sig.items():
        f = getattr(lib, name)
        f.restype, f.argtypes = res, args
    return lib


def test_pack_layout(lib):
    ch = Champion(name=b"HALK", title=b"THE BARBARIAN", direction=2, cell=3, current_health=-2, maximum_stamina=1234,
                  food=-1024, water=2048, wounds=0x21, load=567, shield_defense=-5)
    ch.statistics[1][:] = [60, 55, 30]
    ch.skills[7].temporary_experience, ch.skills[7].experience = 31999, 0x12345678
    ch.slots[10] = 0x2805
    raw = pack(lib, ch)
    assert raw[:4] == b"HALK" and raw[8:21] == b"THE BARBARIAN" and raw[0x1C:0x1E] == b"\x02\x03"
    assert struct.unpack_from(">HHhhhh", raw, 0x30) == (0, 0x21, -2, 0, 0, 1234)
    assert struct.unpack_from(">hh", raw, 0x42) == (-1024, 2048) and raw[0x49:0x4C] == bytes([60, 55, 30])
    assert struct.unpack_from(">hI", raw, 0x5C + 6 * 7) == (31999, 0x12345678)
    assert struct.unpack_from(">H", raw, 0xD4 + 20)[0] == 0x2805
    assert struct.unpack_from(">Hh", raw, 0x110) == (567, -5)
    back = Champion()
    lib.champion_unpack(raw, ctypes.byref(back))
    assert pack(lib, back) == raw


def test_object_weight_and_icon_match_original(lib):
    w = World(lib, 1)
    for party_direction in range(4):
        w.globals(0, 1000, 0, 0, 0, party_direction)
        for t in w.things + [0xFFFF]:
            assert lib.inventory_get_object_weight(t) == w.emu.call(WEIGHT, [(2, t)]) & 0xFFFF, hex(t)
            assert lib.object_get_icon_index(t) == s16(w.emu.call(ICON, [(2, t)])), hex(t)


@pytest.mark.parametrize("seed", range(6))
def test_champion_values_match_original(lib, seed):
    w = World(lib, seed)
    cs = champions(lib)
    for round_ in range(40):
        w.put_champions(4)
        w.globals(w.rng.choice([0, 0, 1]), w.rng.randint(0, 100000), w.rng.randint(0, 100000), 0, w.rng.randrange(4))
        shields = [w.rng.randint(0, 50) for _ in range(3)]
        party = var(lib, Party, "g_party")
        party.shield_defense, party.fire_shield_defense, party.spell_shield_defense = shields
        w.emu.write(PARTY_SHIELD, struct.pack(">3h", *shields))
        for i in range(4):
            addr = CHAMPIONS + 0x140 * i
            for skill in range(20):
                for flags in (0, 0x4000, 0x8000, 0xC000):
                    assert lib.champion_get_skill_level(i, skill | flags) == s16(w.emu.call(SKILL_LEVEL, [(2, i), (2, skill | flags)]))
            assert lib.champion_get_maximum_load(ctypes.byref(cs[i])) == w.emu.call(MAX_LOAD, [(4, addr)]) & 0xFFFF
            assert lib.champion_get_movement_ticks(ctypes.byref(cs[i])) == s16(w.emu.call(MOVE_TICKS, [(4, addr)]))
            w.seed(round_ * 77 + i)
            assert lib.champion_get_dexterity(ctypes.byref(cs[i])) == s16(w.emu.call(DEXTERITY, [(4, addr)]))
            for slot in range(6):
                assert lib.champion_get_strength(i, slot) == s16(w.emu.call(STRENGTH, [(2, i), (2, slot)]))
                for sharp in (0, 0x8000):
                    assert lib.champion_get_wound_defense(i, slot | sharp) == s16(w.emu.call(WOUND_DEFENSE, [(2, i), (2, slot | sharp)]))
            w.check_state()


@pytest.mark.parametrize("seed", range(4))
def test_damage_matches_original(lib, seed):
    w = World(lib, 10 + seed)
    for round_ in range(150):
        if round_ % 30 == 0:
            w.put_champions(w.rng.randint(1, 4))
            for i in range(4):  # damage needs a living champion
                champions(lib)[i].current_health = max(1, champions(lib)[i].current_health)
                w.emu.write(CHAMPIONS + 0x140 * i, pack(lib, champions(lib)[i]))
        w.globals(w.rng.choice([0, 1]), 5000, 0, 0, 0)
        w.seed(seed * 1000 + round_)
        kind = w.rng.randrange(3)
        if kind == 0:
            args = (w.rng.randrange(4), w.rng.choice([-5, 0, 1, 30, 100, 300, 2000]), w.rng.getrandbits(6), w.rng.randrange(8))
            assert lib.champion_add_pending_damage_and_wounds(*args) == s16(w.emu.call(PENDING, [(2, a) for a in args]))
        elif kind == 1:
            args = (w.rng.choice([0, 5, 20, 120]), w.rng.getrandbits(6), w.rng.randrange(8))
            assert lib.champion_damage_all(*args) == s16(w.emu.call(DAMAGE_ALL, [(2, a) for a in args]))
        else:
            args = (w.rng.randrange(4), w.rng.randint(-300, 3000))
            lib.champion_decrement_stamina(*args)
            w.emu.call(0x111CA2, [(2, a) for a in args])
        w.check_state()


@pytest.mark.parametrize("seed", range(4))
def test_experience_matches_original(lib, seed):
    w = World(lib, 20 + seed)
    level_up = (c16 * 16).in_dll(lib, "g_champion_skill_level_up")
    gained = 0
    for round_ in range(200):
        if round_ % 25 == 0:
            gained += sum(level_up)
            w.put_champions(4)
            level_up[:] = [0] * 16
            w.emu.write(LEVEL_UP_ADDR, bytes(32))
        time = w.rng.randint(0, 3000)
        w.globals(0, time, max(0, time - w.rng.choice([0, 10, 30, 100, 200])), 0, w.rng.choice([0, 0, 1, 3]))
        w.seed(seed * 7919 + round_)
        args = (w.rng.randrange(4), w.rng.randrange(20), w.rng.choice([0, 1, 7, 50, 400, 3000, 20000]))
        lib.champion_add_skill_experience(*args)
        w.emu.call(ADD_EXPERIENCE, [(2, a) for a in args])
        w.check_state()
        assert list(level_up) == list(struct.unpack(">16h", w.emu.read(LEVEL_UP_ADDR, 32)))
    assert gained > 0


@pytest.mark.parametrize("seed", range(4))
def test_time_effects_match_original(lib, seed):
    w = World(lib, 30 + seed)
    party = var(lib, Party, "g_party")
    for round_ in range(60):
        w.put_champions(w.rng.randint(1, 4))
        candidate = w.rng.choice([0, 0, 1, 2])
        var(lib, c16, "g_candidate_champion_ordinal").value = candidate
        w.emu.write(CANDIDATE, struct.pack(">h", candidate))
        time = w.rng.choice([w.rng.randint(0, 70000), 256 * w.rng.randint(0, 200), 64 * w.rng.randint(0, 900)])
        w.globals(w.rng.choice([0, 0, 1]), time, w.rng.randint(0, time), time - w.rng.randint(0, 400), 0, w.rng.randrange(4))
        x, y, m = w.rng.randrange(32), w.rng.randrange(32), w.rng.randrange(14)
        for name, addr, v in (("g_party_map_x", MAP_X, x), ("g_party_map_y", MAP_Y, y), ("g_party_map_index", MAP_INDEX, m)):
            var(lib, c16, name).value = v
            w.emu.write(addr, struct.pack(">h", v))
        n = w.rng.randint(0, 24)
        scents = [w.rng.choice([x << 11 | y << 6 | m, w.rng.getrandbits(16)]) for _ in range(n)]
        strengths = [w.rng.choice([0, 1, 2, w.rng.getrandbits(8)]) for _ in range(n)]
        party.scent_count, party.first_scent_index, party.last_scent_index = n, w.rng.randint(0, n), w.rng.randint(0, n)
        party.scents[:n], party.scent_strengths[:n] = scents, strengths
        w.emu.write(SCENT_COUNT, bytes([n, 0, party.first_scent_index, party.last_scent_index]))
        w.emu.write(SCENTS, struct.pack(f">{n}H", *scents))
        w.emu.write(SCENT_STRENGTHS, bytes(strengths))
        w.seed(seed * 31 + round_)
        lib.champion_apply_time_effects()
        w.emu.call(TIME_EFFECTS)
        w.check_state()
        n = party.scent_count
        assert bytes(w.emu.read(SCENT_COUNT, 4)) == bytes([n, party.freeze_life_ticks, party.first_scent_index, party.last_scent_index])
        assert list(party.scents[:n]) == list(struct.unpack(f">{n}H", w.emu.read(SCENTS, 2 * n)))
        assert list(party.scent_strengths[:n]) == list(w.emu.read(SCENT_STRENGTHS, n))


def square_things(lib, m, x, y):
    t = lib.dungeon_get_square_first_thing(m, x, y)
    while t != 0xFFFE:
        yield t
        d = lib.rec_write(t)
        t = d[0] << 8 | d[1]


def first_mirror(lib):
    """(portrait, wall map, x, y, sensor cell) of the first champion mirror in the dungeon."""
    for m in range(lib.dungeon_map_count()):
        for x in range(lib.dungeon_map_width(m)):
            for y in range(lib.dungeon_map_height(m)):
                for t in square_things(lib, m, x, y):
                    if t >> 10 & 15 == 3 and (p := lib.recruit_portrait_index(t)) >= 0:
                        return p, m, x, y, t >> 14


@pytest.fixture
def party(lib):
    """A new game with the party in front of the first mirror and the candidate recruited."""
    need(lib, "Dungeon.DAT")
    assert lib.dungeon_load(str(GAME / "Dungeon.DAT").encode()) == 1
    lib.map_reset()  # dungeon_load replaced the map buffers
    lib.game_start_new()
    for name in ("g_candidate_champion_ordinal", "g_inventory_champion_ordinal", "g_party_is_sleeping"):
        var(lib, global_type(name, c16), name).value = 0
    var(lib, ctypes.c_bool, "g_leader_empty_handed").value = 1
    var(lib, ctypes.c_uint32, "g_random_seed").value = 12345
    portrait, m, x, y, c = first_mirror(lib)
    lib.map_set_current_map_and_party_map(m)
    var(lib, c16, "g_party_map_x").value = px = x + STEP_EAST[c]
    var(lib, c16, "g_party_map_y").value = py = y + STEP_NORTH[c]
    var(lib, c16, "g_party_direction").value = d = (c + 2) & 3
    possessions = [t for t in square_things(lib, m, x, y) if t >> 10 & 15 >= 4 and t >> 14 == c]
    need(lib, "Graphics.DAT")
    assert lib.gfx_open(str(GAME / "Graphics.DAT").encode())  # the portraits of graphic 26
    lib.champion_add_candidate_to_party(portrait)
    return dict(m=m, wall=(x, y), square=(px, py), direction=d, possessions=possessions, text=lib.recruit_text_thing(m, px, py), portrait=portrait)


def test_recruit_first_mirror_champion(lib, party):
    lib.recruit_parse.argtypes = [u16, ctypes.POINTER(RecruitData)]
    lib.recruit_parse.restype = ctypes.c_bool
    r = RecruitData()
    assert lib.recruit_parse(party["text"], ctypes.byref(r)) == 1
    ch = champions(lib)[0]
    assert var(lib, u16, "g_party_champion_count").value == 1
    assert var(lib, c16, "g_candidate_champion_ordinal").value == 1 and var(lib, c16, "g_leader_index").value == 0
    assert (ch.name, ch.title) == (r.name, r.title)
    assert (ch.current_health, ch.maximum_health, ch.maximum_stamina, ch.maximum_mana) == (r.current_health, r.maximum_health, r.maximum_stamina, r.maximum_mana)
    assert [list(s) for s in ch.statistics] == [list(s) for s in r.statistics]
    assert [s.experience for s in ch.skills] == list(r.experience)
    assert 1500 <= ch.food < 1756 and 1500 <= ch.water < 1756
    assert ch.direction == ch.cell == party["direction"] and ch.action_index == 255 and ch.hide_damage_received_event_index == -1
    slots = [s for s in ch.slots if s != 0xFFFF]
    assert sorted(slots) == sorted(party["possessions"])
    assert ch.load == sum(lib.inventory_get_object_weight(t) for t in slots)
    lib.champion_click_in_resurrect_reincarnate_panel(0xA0)
    assert var(lib, c16, "g_candidate_champion_ordinal").value == 0
    wall = list(square_things(lib, party["m"], *party["wall"]))
    assert not set(t & 0x3FFF for t in party["possessions"]) & set(t & 0x3FFF for t in wall)
    assert all(lib.recruit_portrait_index(t) == -1 for t in wall if t >> 10 & 15 == 3)  # mirror disabled


def test_recruit_copies_the_mirror_portrait(lib, party):
    lib.gfx_get_bitmap.restype = ctypes.POINTER(Bitmap)
    sheet = lib.gfx_get_bitmap(26).contents
    x, y = c16(), c16()
    lib.recruit_portrait_origin(party["portrait"], ctypes.byref(x), ctypes.byref(y))
    expected = [sheet.px[(y.value + r) * sheet.w + x.value + c] for r in range(29) for c in range(32)]
    portraits = (ctypes.c_uint8 * (4 * 32 * 29)).in_dll(lib, "g_champion_portraits")
    assert party["portrait"] > 0 and list(portraits[: 32 * 29]) == expected


def test_eat_food(lib, party):
    lib.champion_click_in_resurrect_reincarnate_panel(0xA0)
    ch = champions(lib)[0]
    var(lib, c16, "g_inventory_champion_ordinal").value = 1
    lib.tables_get_object_info.restype = ctypes.POINTER(u16 * 3)
    lib.tables_get_food_value.restype = c16
    junk = next(j for j in range(53) if 0xA8 <= lib.tables_get_object_info(0x7F + j).contents[0] < 0xB0)
    food_value = lib.tables_get_food_value(lib.tables_get_object_info(0x7F + junk).contents[0] - 0xA8)
    thing = lib.rec_find_unused_thing(10)
    lib.rec_junk_set_type(thing, junk)
    ch.food, load = 100, ch.load
    lib.inventory_put_object_in_leader_hand(thing)
    assert ch.load == load + lib.inventory_get_object_weight(thing)
    lib.inventory_process_click_on_mouth()
    assert ch.food == min(100 + food_value, 2048) and ch.load == load
    assert var(lib, u16, "g_leader_hand_object").value == 0xFFFF and var(lib, ctypes.c_bool, "g_leader_empty_handed").value == 1
    d = lib.rec_write(thing)
    assert d[0] << 8 | d[1] == 0xFFFF  # record freed


def test_damage_until_death(lib, party):
    lib.champion_click_in_resurrect_reincarnate_panel(0xA0)
    ch = champions(lib)[0]
    slots = [s for s in ch.slots if s != 0xFFFF]
    hits = 0
    while ch.current_health:
        lib.champion_damage_all(50, 0x3F, 3)
        lib.champion_apply_and_draw_pending_damage_and_wounds()
        hits += 1
        assert hits < 200
    assert var(lib, ctypes.c_bool, "g_party_dead").value == 1
    assert all(s == 0xFFFF for s in ch.slots) and ch.load == 0
    dropped = list(square_things(lib, party["m"], *party["square"]))
    assert {t & 0x3FFF for t in slots} <= {t & 0x3FFF for t in dropped}
    bones = [t for t in dropped if t >> 10 & 15 == 10 and lib.rec_write(t)[3] & 0x7F == 5]
    assert len(bones) == 1 and lib.rec_write(bones[0])[2] >> 6 == 0 and bones[0] >> 14 == ch.cell


def test_death_sets_redraw_flags(lib, party):
    lib.champion_click_in_resurrect_reincarnate_panel(0xA0)
    cs = champions(lib)
    ctypes.memmove(ctypes.byref(cs[1]), ctypes.byref(cs[0]), ctypes.sizeof(Champion))
    cs[1].cell = (cs[0].cell + 1) & 3
    var(lib, u16, "g_party_champion_count").value = 2
    var(lib, c16, "g_leader_index").value = 1
    var(lib, c16, "g_magic_caster_champion_index").value = 0
    redraw = var(lib, u16, "g_redraw")
    redraw.value = 0
    lib.champion_kill(0)
    assert cs[0].attributes & ATTR_STATUS_BOX  # the user interface draws the dead champion after the tick
    assert var(lib, c16, "g_magic_caster_champion_index").value == 1 and redraw.value == C.REDRAW_SPELL_AREA



def test_death_marks_the_champion_for_release(lib, party):
    lib.champion_click_in_resurrect_reincarnate_panel(0xA0)
    cs = champions(lib)
    ctypes.memmove(ctypes.byref(cs[1]), ctypes.byref(cs[0]), ctypes.sizeof(Champion))
    cs[1].cell = (cs[0].cell + 1) & 3
    var(lib, u16, "g_party_champion_count").value = 2
    dead = var(lib, u16, "g_dead_champions")
    dead.value = 0
    lib.champion_kill(1)
    assert dead.value == 1 << 1  # the user interface releases the champion after the tick
    dead.value = 0

SET_DUNGEON_VIEW_PALETTE, PALETTE_INDEX, PARTY = 0x10EDD6, 0x1001E6, 0x101ED4
BRIGHT_THRESHOLD = 99  # g_palette_light_thresholds[0]


@pytest.mark.parametrize("seed", range(3))
def test_party_light_matches_original(lib, seed):
    need(lib, "Dungeon.DAT")
    assert lib.dungeon_load(str(GAME / "Dungeon.DAT").encode()) == 1
    w = World(lib, 300 + seed)
    lib.party_get_light.restype = c16
    w.put_champions(4)
    w.globals(0, 1000, 0, 0, 1)  # a map with darkness
    var(lib, c16, "g_party_map_index").value = 1
    weapons = [t for t in w.things if t >> 10 == 5]
    data = ctypes.cast((ctypes.c_void_p * 16).in_dll(lib, "g_thing_data")[5], ctypes.POINTER(u8))
    torch_type = next(i for i in range(len(weapons)) if lib.object_get_type(weapons[i]) in range(4, 8))
    torch_subtype = data[4 * torch_type + 3] & 0x7F
    torches = weapons[:8]
    cs = champions(lib)

    def emu_index(magic):
        w.emu.write(PARTY, struct.pack(">h", magic))
        w.emu.call(SET_DUNGEON_VIEW_PALETTE)
        return struct.unpack(">h", w.emu.read(PALETTE_INDEX, 2))[0]

    for round_ in range(40):
        for i in range(8):  # weapon records 0-7: torches with random charges
            rec = bytes([w.rng.getrandbits(8), w.rng.getrandbits(1) << 7 | torch_subtype])
            ctypes.memmove(ctypes.addressof(data.contents) + 4 * i + 2, rec, 2)
            w.emu.write(THING_BUF + 4 * i + 2, rec)
        for c in range(4):
            for hand in range(2):
                cs[c].slots[hand] = w.rng.choice(torches + [0xFFFF, w.rng.choice(w.things)])
                w.emu.write(CHAMPIONS + 0x140 * c + 0xD4 + 2 * hand, struct.pack(">H", cs[c].slots[hand]))
        lo, hi = -2000, 2000  # the least magical light that is fully bright gives the torch light
        while lo < hi:
            mid = (lo + hi) // 2
            lo, hi = (lo, mid) if emu_index(mid) == 0 else (mid + 1, hi)
        want = BRIGHT_THRESHOLD - lo
        var(lib, Party, "g_party").magical_light_amount = 0
        got = lib.party_get_light()
        assert got == want, (round_, [list(cs[c].slots[:2]) for c in range(4)])
        for magic in (lo - 1, lo, -want, -want + 1, 40 - want):
            var(lib, Party, "g_party").magical_light_amount = magic
            lib.party_set_light_level(1, lib.party_get_light())
            assert var(lib, c16, "g_party_light_level").value == emu_index(magic), (round_, magic)
