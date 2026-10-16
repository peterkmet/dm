"""Creature groups, projectiles, explosions, and spell effects (src/group.c, projectile.c, magic.c)."""

import ctypes
import random
import shutil
import struct
from pathlib import Path

import pytest

from dmtest.emu import need, s16, var
from dmtest.game import (Party, World, add_test_champion, events, new_game, place_party, setup_creatures, skeleton_map_and_squares,
                         things_of_type, tick)
from dmtest.lib import load_lib

from dmtools.emu import Emu
from dmtools.ftl import load_image, parse

ROOT = Path(__file__).parent.parent
GAME = ROOT / "game"
c16, u16, u8 = ctypes.c_int16, ctypes.c_uint16, ctypes.c_uint8

# Original addresses (docs/kaos-symbols.csv).
SEED, THING_DATA, DUNGEON_HEADER = 0x100EF8, 0x101CEA, 0x101CD4
ATTACK_TYPE, POISON_ATTACK, SECONDARY_DIRECTION = 0x10315E, 0x10315C, 0x103156
PALETTE_INDEX, INVISIBILITY, CREATURE_INFO = 0x1001E6, 0x101F2A, 0x10192E
IMPACT_ATTACK, LAUNCHER_OBJECT = 0x11BF0A, 0x10B24E
CHAMPION_DAMAGE, DIRECTIONS, VISIBLE, POISON_RESISTANCE = 0x11D10A, 0x11CFBA, 0x11CF42, 0x10C99A
SOUND_REQUEST_PLAY, CHAMPION_POISON = 0x108DF4, 0x111B32
RTS = b"\x4e\x75"
THING_BUF, HEADER_BUF, GROUP_BUF = 0x200000, 0x220000, 0x230000
FIREBALL = 0xFF80
THING_GROUP, THING_WEAPON, THING_ARMOUR, THING_PROJECTILE, THING_EXPLOSION = 4, 5, 6, 14, 15
CREATURE_SKELETON, CREATURE_MUMMY = 12, 10
BEHAVIOR_APPROACH, BEHAVIOR_ATTACK = 7, 6
THING_POTION, POTION_EMPTY_FLASK, ICON_EMPTY_FLASK = 8, 20, 195
SLOT_HAND_ACTION = 1
EVENT_LIGHT, EVENT_PARTY_SHIELD_END = 70, 74

pytestmark = pytest.mark.skipif(not shutil.which("clang"), reason="clang not installed")


@pytest.fixture(scope="module")
def lib():
    return setup_creatures(load_lib())


# Differential tests against the original code in dmtools.emu.


@pytest.mark.parametrize("seed", range(3))
def test_directions_to_destination_match_original(lib, seed):
    need(lib)
    emu = Emu(load_image(parse((GAME / "KAOS.FTL").read_bytes()), 0x100000))
    rng = random.Random(seed)
    for round_ in range(400):
        sx, sy, dx, dy = (rng.randrange(-2, 34) for _ in range(4))
        value = rng.getrandbits(32)
        var(lib, ctypes.c_uint32, "g_random_seed").value = value
        emu.write(SEED, struct.pack(">I", value))
        want = s16(emu.call(DIRECTIONS, [(2, sx & 0xFFFF), (2, sy & 0xFFFF), (2, dx & 0xFFFF), (2, dy & 0xFFFF)]))
        assert lib.group_get_directions_to_destination(sx, sy, dx, dy) == want, (sx, sy, dx, dy)
        assert var(lib, c16, "g_group_secondary_direction").value == s16(struct.unpack(">H", emu.read(SECONDARY_DIRECTION, 2))[0])
        assert var(lib, ctypes.c_uint32, "g_random_seed").value == struct.unpack(">I", emu.read(SEED, 4))[0]
        for direction in range(4):
            args = (direction, sx, sy, dx, dy)
            assert lib.group_is_destination_visible_from_source(*args) == s16(emu.call(VISIBLE, [(2, a & 0xFFFF) for a in args])), args


def test_poison_resistance_matches_original(lib):
    need(lib)
    emu = Emu(load_image(parse((GAME / "KAOS.FTL").read_bytes()), 0x100000))
    for creature_type in range(27):
        for poison in (0, 1, 7, 30, 120, 255):
            var(lib, ctypes.c_uint32, "g_random_seed").value = creature_type * 1000 + poison
            emu.write(SEED, struct.pack(">I", creature_type * 1000 + poison))
            want = emu.call(POISON_RESISTANCE, [(2, creature_type), (2, poison)]) & 0xFFFF
            assert lib.group_get_resistance_adjusted_poison_attack(creature_type, poison) == want, (creature_type, poison)


@pytest.mark.parametrize("seed", range(4))
def test_champion_damage_matches_original(lib, seed):
    new_game(lib)  # the timeline for the delayed damage sound
    w = World(lib, 40 + seed)
    for addr in (SOUND_REQUEST_PLAY, CHAMPION_POISON):
        w.emu.write(addr, RTS)
    group = ctypes.create_string_buffer(16)
    thing_data = (ctypes.c_void_p * 16).in_dll(lib, "g_thing_data")
    thing_data[THING_GROUP] = ctypes.addressof(group)
    creatures = [t for t in range(27) if w.emu.read(CREATURE_INFO + 26 * t + 11, 1)[0] == 0]  # no poison: champion_poison is stubbed
    for round_ in range(120):
        if round_ % 20 == 0:
            w.put_champions(w.rng.randint(1, 4))
        sleeping = w.rng.choice([0, 0, 0, 1])
        w.globals(sleeping, 5000, 0, 0, w.rng.randrange(6))
        palette, invisible = w.rng.randrange(6), w.rng.choice([0, 0, 1])
        var(lib, c16, "g_dungeon_view_palette_index").value = palette
        var(lib, Party, "g_party").invisibility = invisible
        w.emu.write(PALETTE_INDEX, struct.pack(">h", palette))
        w.emu.write(INVISIBILITY, bytes([invisible]))
        group[4] = w.rng.choice(creatures)
        w.emu.write(GROUP_BUF, group.raw)
        champion = w.rng.randrange(4)
        w.seed(seed * 997 + round_)
        got = lib.group_get_champion_damage(THING_GROUP << 10, champion)
        want = s16(w.emu.call(CHAMPION_DAMAGE, [(4, GROUP_BUF), (2, champion)]))
        assert got == want, (round_, group[4], champion)
        w.check_state()


@pytest.mark.parametrize("seed", range(4))
def test_projectile_impact_attack_matches_original(lib, seed):
    w = Things(lib, 100 + seed)
    explosions = [FIREBALL + t for t in (0, 1, 2, 3, 4, 6, 7, 40, 50)]
    for round_ in range(300):
        kinetic, attack = w.rng.getrandbits(8), w.rng.getrandbits(8)
        w.write_record(14, 0, struct.pack(">HHBBH", 0xFFFE, 0, kinetic, attack, 0))
        thing = w.rng.choice(w.things + explosions)
        w.seed(seed * 1000 + round_)
        got = lib.projectile_get_impact_attack(14 << 10, thing)
        want = w.emu.call(IMPACT_ATTACK, [(4, w.records[14][1]), (2, thing)]) & 0xFFFF
        assert got == want, (hex(thing), kinetic, attack)
        assert c16.in_dll(lib, "g_projectile_attack_type").value == s16(struct.unpack(">H", w.emu.read(ATTACK_TYPE, 2))[0])
        assert c16.in_dll(lib, "g_projectile_poison_attack").value == s16(struct.unpack(">H", w.emu.read(POISON_ATTACK, 2))[0])
        assert ctypes.c_uint32.in_dll(lib, "g_random_seed").value == struct.unpack(">I", w.emu.read(SEED, 4))[0]


def test_object_for_launcher_matches_original(lib):
    w = Things(lib, 7)
    for index in list(range(-3, 201)) + [0x7FFF]:
        for t in (5, 10):  # one free weapon and one free junk record
            w.write_record(t, 0, b"\xff\xff" + bytes(2))
        got = lib.projectile_get_object_for_launcher_or_generator(index)
        want = w.emu.call(LAUNCHER_OBJECT, [(2, index & 0xFFFF)]) & 0xFFFF
        assert got == want, index
        for t in (5, 10):
            c, e = w.record(t, 0)
            assert c == e, (index, t)


class Things:
    """Object records of types 5-10 and one projectile record, the same in the C library and in the emulator."""

    TYPES = [(5, 46, 4), (6, 58, 4), (7, 1, 4), (8, 21, 4), (9, 1, 8), (10, 53, 4)]

    def __init__(self, lib, seed):
        need(lib)
        self.lib, self.rng = lib, random.Random(seed)
        self.emu = Emu(load_image(parse((GAME / "KAOS.FTL").read_bytes()), 0x100000))
        thing_data = (ctypes.c_void_p * 16).in_dll(lib, "g_thing_data")
        self.keep, self.things, self.records = [], [], {}
        header = bytearray(44)
        addr = THING_BUF
        for t, n, size in self.TYPES + [(14, 1, 8)]:
            data = bytearray()
            for i in range(n):
                rec = bytearray(size)
                struct.pack_into(">H", rec, 0, 0xFFFE)
                if t == 8:
                    rec[2], rec[3] = i, self.rng.getrandbits(8)
                elif t == 9:  # an empty chest
                    struct.pack_into(">H", rec, 2, 0xFFFE)
                elif t != 14:
                    rec[2], rec[3] = self.rng.getrandbits(8) & 0x7F, i
                data += rec
                if t != 14:
                    self.things.append(t << 10 | i)
            struct.pack_into(">H", header, 12 + 2 * t, n)
            buf = ctypes.create_string_buffer(bytes(data), len(data))
            self.keep.append(buf)
            self.records[t] = (buf, addr, size)
            thing_data[t] = ctypes.addressof(buf)
            self.emu.write(addr, data)
            self.emu.write(THING_DATA + 4 * t, struct.pack(">I", addr))
            addr += 0x400
        self.header = ctypes.create_string_buffer(bytes(header), 44)
        ctypes.c_void_p.in_dll(lib, "g_dungeon_header").value = ctypes.addressof(self.header)
        self.emu.write(HEADER_BUF, bytes(header))
        self.emu.write(DUNGEON_HEADER, struct.pack(">I", HEADER_BUF))  # g_dungeon_header is a pointer

    def write_record(self, t, i, data):
        buf, addr, size = self.records[t]
        ctypes.memmove(ctypes.addressof(buf) + i * size, bytes(data), len(data))
        self.emu.write(addr + i * size, bytes(data))

    def record(self, t, i):
        buf, addr, size = self.records[t]
        return buf.raw[i * size:(i + 1) * size], bytes(self.emu.read(addr + i * size, size))

    def seed(self, value):
        ctypes.c_uint32.in_dll(self.lib, "g_random_seed").value = value
        self.emu.write(SEED, struct.pack(">I", value))


# Scenarios on the loaded dungeon.


def adjacent_group(lib, x, y):
    """A group on a square next to (x, y) on the current map, or None."""
    for nx, ny in ((x, y - 1), (x + 1, y), (x, y + 1), (x - 1, y)):
        group = lib.map_get_square_group(nx, ny)
        if group != 0xFFFE:
            return group
    return None


def mummy_in_corridor(lib, m):
    """A single mummy group on map m and a party square 3 squares away in a straight empty corridor."""
    is_corridor = lambda x, y: lib.dungeon_get_square(m, x, y) >> 5 == 1 and lib.dungeon_get_square_first_thing(m, x, y) == 0xFFFE
    for x in range(lib.dungeon_map_width(m)):
        for y in range(lib.dungeon_map_height(m)):
            t = lib.dungeon_get_square_first_thing(m, x, y)
            while t != 0xFFFE and t >> 10 & 15 != THING_GROUP:
                t = lib.map_get_next_thing(t)
            if t == 0xFFFE or lib.dungeon_get_thing_data(t)[4] != CREATURE_MUMMY:
                continue
            for direction, (sx, sy) in enumerate(((0, -1), (1, 0), (0, 1), (-1, 0))):
                squares = [(x + sx * k, y + sy * k) for k in (1, 2, 3)]
                if all(0 <= px < lib.dungeon_map_width(m) and 0 <= py < lib.dungeon_map_height(m) and is_corridor(px, py) for px, py in squares):
                    return (x, y), squares[-1], (direction + 2) % 4
    return None


def test_group_activates_and_approaches_party(lib):
    new_game(lib)
    found = mummy_in_corridor(lib, 1)
    assert found, "no mummy in a straight corridor on map 1"
    _, party_square, facing = found
    add_test_champion(lib)
    place_party(lib, 1, *party_square, facing)
    assert var(lib, u16, "g_active_group_count").value > 0
    seen, group = set(), None
    for _ in range(300):
        tick(lib, seen)
        group = adjacent_group(lib, *party_square)
        if group is not None:
            break
    assert group is not None, "no group came next to the party"
    assert seen & {32, 37}  # group aspect and behavior events ran
    assert lib.dungeon_get_thing_data(group)[15] & 15 in (BEHAVIOR_APPROACH, BEHAVIOR_ATTACK)


def test_group_attacks_adjacent_party(lib):
    new_game(lib)
    _, party_square, facing = mummy_in_corridor(lib, 1)
    champion = add_test_champion(lib)
    place_party(lib, 1, *party_square, facing)
    pending = (c16 * 4).in_dll(lib, "g_champion_pending_damage")
    hit = False
    for _ in range(600):
        tick(lib)
        if pending[0] or champion.current_health < 100:
            hit = True
            break
    assert hit


def test_creature_dies_and_drops_possessions(lib):
    new_game(lib)
    add_test_champion(lib)
    found = skeleton_map_and_squares(lib)
    assert found, "no map allows skeletons"
    m, party_square, (x, y) = found
    place_party(lib, m, *party_square, 2)
    assert lib.map_get_square_group(x, y) == 0xFFFE
    # Like the original, the result is THING_NONE when the new group becomes active on the party map.
    assert lib.group_get_generated(CREATURE_SKELETON, 1, 0, 0, x, y) == 0xFFFF
    group = lib.map_get_square_group(x, y)
    assert group != 0xFFFE and lib.dungeon_get_thing_data(group)[4] == CREATURE_SKELETON
    active = var(lib, u16, "g_active_group_count").value
    assert lib.group_get_damage_all_creatures_outcome(group, x, y, 10, 1) == 0  # 10 damage: the skeleton survives
    assert lib.map_get_square_group(x, y) == group
    assert lib.group_get_damage_all_creatures_outcome(group, x, y, 2000, 1) == 2
    assert lib.map_get_square_group(x, y) == 0xFFFE
    assert var(lib, u16, "g_active_group_count").value == active - 1
    drops = things_of_type(lib, x, y, THING_WEAPON) + things_of_type(lib, x, y, THING_ARMOUR)
    assert drops  # the skeleton drops its falchion and helmet
    assert things_of_type(lib, x, y, THING_EXPLOSION)  # the smoke of the dead creature
    for _ in range(10):
        tick(lib)
    assert not things_of_type(lib, x, y, THING_EXPLOSION)


def test_fireball_flies_and_explodes_on_wall(lib):
    new_game(lib)
    lib.move_party_enter_dungeon()
    px, py = c16.in_dll(lib, "g_party_map_x").value, c16.in_dll(lib, "g_party_map_y").value
    corridor = lambda x, y: lib.map_get_square(x, y) >> 5 == 1 and lib.map_get_square_first_thing(x, y) == 0xFFFE and (x, y) != (px, py)
    start = next((x, y) for y in range(lib.dungeon_map_height(0)) for x in range(lib.dungeon_map_width(0))
                 if corridor(x, y) and corridor(x + 1, y) and corridor(x + 2, y) and lib.map_get_square(x + 3, y) >> 5 == 0)
    x, y = start
    lib.projectile_create(FIREBALL, x, y, 0, 1, 100, 100, 1)  # east, back cell
    path, seen = [], set()
    for _ in range(12):
        where = [(sx, y) for sx in range(x, x + 3) if things_of_type(lib, sx, y, THING_PROJECTILE)]
        explosions = [(sx, y) for sx in range(x, x + 3) if things_of_type(lib, sx, y, THING_EXPLOSION)]
        path.append((tuple(where), tuple(explosions)))
        tick(lib, seen)
    flight = [p[0][0] for p in path if p[0]]
    assert flight[0] == start and flight[-1] == (x + 2, y)
    assert sorted(set(flight)) == [(x, y), (x + 1, y), (x + 2, y)] and flight == sorted(flight)
    exploded = [i for i, p in enumerate(path) if p[1]]
    assert exploded and all(path[i][1] == ((x + 2, y),) for i in exploded)
    assert not path[exploded[-1]][0] and path[-1] == ((), ())
    assert {48, 49, 25} <= seen


# Spell casting.


@pytest.fixture
def caster(lib):
    if not all((GAME / f).exists() for f in ("KAOS.FTL", "Graphics.DAT", "Dungeon.DAT")):
        pytest.skip("missing game files")
    assert lib.kaos_load(str(GAME / "KAOS.FTL").encode()) == 0
    assert lib.gfx_open(str(GAME / "Graphics.DAT").encode()) > 0
    assert lib.layout_init() == 0 and lib.text_init() == 0 and lib.tables_init() == 0
    new_game(lib)
    assert lib.ui_init() == 0
    lib.move_party_enter_dungeon()
    c = add_test_champion(lib)
    for s in range(20):
        c.skills[s].experience = 1 << 22  # every skill high enough for every spell
    return lib, c


def cast(lib, c, symbols):
    c.symbols = bytes(symbols)
    return lib.magic_get_champion_spell_cast_result(0)


def test_light_spell_adds_light_event_and_experience(caster):
    lib, c = caster
    party = Party.in_dll(lib, "g_party")
    light, mana, exp = party.magical_light_amount, c.current_mana, c.skills[17].experience
    assert cast(lib, c, [0x63, 0x68, 0x6F, 0x76]) == 1  # power 4, LIGHT
    assert party.magical_light_amount > light
    assert len(events(lib, EVENT_LIGHT)) == 1 and c16(events(lib, EVENT_LIGHT)[0].b).value < 0
    assert c.current_mana == mana and c.skills[17].experience > exp
    assert c.enable_action_event_index >= 0


def test_potion_spell_fills_empty_flask(caster):
    lib, c = caster
    assert cast(lib, c, [0x62, 0x66]) == 3  # power 3, no flask in hand
    flask = lib.map_get_unused_thing(THING_POTION)
    lib.dungeon_get_thing_data(flask)[2] = POTION_EMPTY_FLASK
    assert lib.object_get_icon_index(flask) == ICON_EMPTY_FLASK
    c.slots[SLOT_HAND_ACTION] = flask
    assert cast(lib, c, [0x62, 0x66]) == 1
    data = lib.dungeon_get_thing_data(flask)
    assert data[2] & 0x7F == 11 and 3 * 40 <= data[3] < 3 * 40 + 16


def test_meaningless_spell_fails(caster):
    lib, c = caster
    assert cast(lib, c, [0x60, 0x61]) == 0
    assert c.enable_action_event_index == -1


def test_spell_level_and_experience_rule(caster):
    lib, c = caster
    lib.random16.restype = u16
    lib.champion_get_skill_level.restype, lib.champion_get_skill_level.argtypes = c16, [c16, u16]
    seed = var(lib, ctypes.c_uint32, "g_random_seed")
    var(lib, ctypes.c_uint32, "g_game_time").value = 100000
    var(lib, ctypes.c_int32, "g_last_creature_attack_time").value = 0  # no recent attack: no double experience
    wizard, skill = 3, 0x12  # spell 1: 00667073 01 12 4863, skill 0x12 under the wizard skill
    c.statistics[3][1] = 0  # wisdom 0: a draw fails above 15

    def cast_at(power, skill_level, start):
        experience = 0 if skill_level == 1 else 500 << (skill_level - 2)
        for s in (wizard, skill):
            c.skills[s].experience, c.skills[s].temporary_experience = experience, 0
        assert lib.champion_get_skill_level(0, skill) == skill_level
        c.enable_action_event_index = -1
        seed.value = start
        result = cast(lib, c, [power, 0x66, 0x70, 0x73])
        return result, c.skills[skill].experience - experience

    failures = 0
    for power in range(0x60, 0x66):
        level = power - 0x5F + 1
        for start in (1, 2, 3):
            seed.value = start
            r, draw = lib.random16(), lib.random16()
            gain = (level * level + (power - 0x60) * 8 + level * 16 + (r & 7)) & 0xFFFF
            assert cast_at(power, level, start) == (1, gain)  # the required level: no draw
            if draw % 128 > 15:
                assert cast_at(power, level - 1, start) == (0, gain >> 1)  # one level missing: the draw fails
                failures += 1
    assert failures


def test_party_shield_adds_defense_and_event(caster):
    lib, c = caster
    party = Party.in_dll(lib, "g_party")
    assert cast(lib, c, [0x61, 0x66, 0x6F]) == 1  # power 2, YA IR
    assert party.shield_defense == 2 * 4 + 4
    shield = events(lib, EVENT_PARTY_SHIELD_END)
    assert len(shield) == 1 and shield[0].b == 12
    assert shield[0].map_time & 0xFFFFFF == ctypes.c_uint32.in_dll(lib, "g_game_time").value + 12 * 12


def test_fireball_spell_shoots_projectile(caster):
    lib, c = caster
    mana = c.current_mana
    assert cast(lib, c, [0x63, 0x69, 0x6F]) == 1  # power 4, FUL IR: fireball
    x, y = c16.in_dll(lib, "g_party_map_x").value, c16.in_dll(lib, "g_party_map_y").value
    projectiles = things_of_type(lib, x, y, THING_PROJECTILE)
    assert len(projectiles) == 1 and struct.unpack(">H", bytes(lib.dungeon_get_thing_data(projectiles[0])[2:4]))[0] == FIREBALL
    assert c.current_mana == mana
