# SPDX-License-Identifier: GPL-3.0-or-later
import ctypes
import struct

import pytest

from dmtest.lib import load_lib
from dmtest.markers import ROOT, needs_dm

from dmtools.ftl import load_image, parse

KAOS = ROOT / "game" / "KAOS.FTL"
BASE = 0x100000

pytestmark = [needs_dm]

u8, s8, u16, s16, u32 = ctypes.c_uint8, ctypes.c_int8, ctypes.c_uint16, ctypes.c_int16, ctypes.c_uint32


def S(*names):
    return type("S", (ctypes.Structure,), {"_fields_": list(names)})


CreatureInfo = S(("aspect_index", u8), ("attack_sound_byte", u8), ("attributes", u16), ("graphic_info", u16),
                 ("movement_ticks", u8), ("attack_ticks", u8), ("defense", u8), ("base_health", u8), ("attack", u8),
                 ("poison_attack", u8), ("dexterity", u8), ("ranges", u16), ("properties", u16), ("resistances", u16),
                 ("animation_ticks", u16), ("wound_probabilities", u16), ("attack_type", u8))
CreatureSound = S(("attack", s8), ("movement", s8))
ObjectInfo = S(("type", u16), ("aspect_index", u8), ("action_set_index", u8), ("allowed_slots", u16))
WeaponInfo = S(("weight", u8), ("weapon_class", u8), ("strength", u8), ("kinetic_energy", u8), ("attributes", u16))
ArmourInfo = S(("weight", u8), ("defense", u8), ("attributes", u8))
Spell = S(("symbols", u32), ("base_required_skill_level", u8), ("skill_index", u8), ("kind", u8), ("type", u8), ("disabled_ticks", u8))
ActionSet = S(("action_indices", u8 * 3), ("minimum_skill_levels", u8 * 2))
ActionInfo = S(("disabled_ticks", u8), ("damage_factor", u8), ("hit_probability", u8), ("stamina", u8), ("defense", s8),
               ("skill_index", u8), ("experience_gain", u8))

# accessor -> (record type, count, address, stride, decoder of one record from raw bytes)
TABLES = {
    "creature_info": (CreatureInfo, 27, 0x10192E, 0x1A, ">BBHHBBBBBBBxHHHHHB"),
    "creature_sound": (CreatureSound, 18, 0x101BEC, 2, ">bb"),
    "object_info": (ObjectInfo, 180, 0x101068, 6, ">HBBH"),
    "weapon_info": (WeaponInfo, 46, 0x1014A0, 6, ">BBBBH"),
    "armour_info": (ArmourInfo, 58, 0x1015B4, 4, ">BBB"),
    "junk_weight": (u8, 53, 0x10169C, 1, ">B"),
    "food_value": (s16, 8, 0x1016D2, 2, ">h"),
    "spell": (Spell, 25, 0x103C84, 8, ">IBBH"),
    "mana_multiplier": (u8, 6, 0x103C7E, 1, ">B"),
    "action_set": (ActionSet, 44, 0x103D4C, 8, ">BBBxBB"),
    "light_amount": (s16, 16, 0x100CB4, 2, ">h"),
    "light_threshold": (s16, 6, 0x100CD4, 2, ">h"),
    "slot_mask": (u16, 38, 0x100C68, 2, ">H"),
    "wound_defense_factor": (u8, 6, 0x100CE4, 1, ">B"),
    "fixed_possession": (u16, 40, 0x101C10, 2, ">H"),
    "action_info": (ActionInfo, 44, None, 1, None),
}


@pytest.fixture(scope="module")
def lib():
    lib = load_lib()
    for name, (rec, *_rest) in TABLES.items():
        f = getattr(lib, "tables_get_" + name)
        scalar = not issubclass(rec, ctypes.Structure)
        f.argtypes, f.restype = [ctypes.c_int16], rec if scalar else ctypes.POINTER(rec)
    lib.tables_get_base_mana_cost.argtypes = [ctypes.c_int16, ctypes.c_int16]
    lib.tables_get_base_mana_cost.restype = u8
    assert not lib.tables_load(b"/nonexistent/KAOS.FTL")
    assert lib.tables_load(str(KAOS).encode())
    return lib


@pytest.fixture(scope="module")
def image():
    return load_image(parse(KAOS.read_bytes()), BASE).data


def get(lib, name, i):
    p = getattr(lib, "tables_get_" + name)(i)
    if not issubclass(TABLES[name][0], ctypes.Structure):
        return p
    return p[0] if p else None


def as_tuple(rec):
    if not isinstance(rec, ctypes.Structure):
        return (rec,)
    out = ()
    for name, _ in rec._fields_:
        v = getattr(rec, name)
        out += tuple(v) if isinstance(v, ctypes.Array) else (v,)
    return out


@pytest.mark.parametrize("name", [n for n in TABLES if TABLES[n][2] is not None])
def test_table_matches_big_endian_image(lib, image, name):
    _, count, addr, stride, fmt = TABLES[name]
    for i in range(count):
        o = addr - BASE + i * stride
        raw = struct.unpack_from(fmt, image, o)
        if name == "spell":
            sym, lvl, skill, w = raw
            raw = (sym, lvl, skill, w & 0xF, (w >> 4) & 0x3F, w >> 10)
        assert as_tuple(get(lib, name, i)) == raw, (name, i)


def test_action_info_matches_per_action_tables(lib, image):
    tables = (0x103EAC, 0x103ED8, 0x103F04, 0x103F30, 0x103F5C, 0x103F88, 0x103FB4)
    for i in range(44):
        raw = tuple(image[a - BASE + i] for a in tables)
        raw = raw[:4] + (raw[4] - 256 if raw[4] > 127 else raw[4],) + raw[5:]
        assert as_tuple(get(lib, "action_info", i)) == raw


def test_symbol_base_mana_cost_matches_image(lib, image):
    for step in range(4):
        for sym in range(6):
            assert lib.tables_get_base_mana_cost(step, sym) == image[0x103C66 - BASE + step * 6 + sym]

