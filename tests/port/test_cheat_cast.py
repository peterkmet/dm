# SPDX-License-Identifier: GPL-3.0-or-later
"""The spell cast of the cheat screen: power MON, no mana, no skill draw, no experience."""

import ctypes

import pytest

from dmtest import constants as C
from dmtest.emu import var
from dmtest.game import Spell, add_test_champion, new_game, setup_creatures
from dmtest.lib import load_lib
from dmtest.markers import GAME, needs_dm

c16, u16 = ctypes.c_int16, ctypes.c_uint16

pytestmark = [needs_dm]

POWER_MON = 6
POTION_POWER_RANDOM = 16


@pytest.fixture(scope="module")
def lib():
    lib = setup_creatures(load_lib())
    assert lib.tables_load(str(GAME / "KAOS.FTL").encode())
    assert lib.gfx_open(str(GAME / "Graphics.DAT").encode())
    lib.layout_init.restype = lib.text_init.restype = ctypes.c_bool
    assert lib.layout_init() and lib.text_init()
    lib.tables_get_spell.restype, lib.tables_get_spell.argtypes = ctypes.POINTER(Spell), [c16]
    lib.magic_cast_cheat_spell.restype, lib.magic_cast_cheat_spell.argtypes = c16, [c16, c16]
    return lib


@pytest.fixture
def caster(lib):
    """Champion 0 with no mana, no experience and no wisdom: a normal cast of a MON spell fails."""
    new_game(lib)
    assert lib.ui_init()
    lib.move_party_enter_dungeon()
    c = add_test_champion(lib)
    c.current_mana = 0
    c.statistics[C.STAT_WISDOM][:] = [0, 0, 0]
    for s in range(20):
        c.skills[s].experience = c.skills[s].temporary_experience = 0
    return c


def potion_spell(lib):
    """The index and the spell of the first potion spell with any power."""
    i = 0
    while lib.tables_get_spell(i):
        spell = lib.tables_get_spell(i).contents
        if spell.kind == C.SPELL_KIND_POTION and not spell.symbols >> 24:
            return i, spell
        i += 1
    raise AssertionError("no potion spell")


def empty_flask(lib):
    flask = lib.rec_find_unused_thing(C.THING_POTION)
    lib.rec_potion_set_type(flask, C.POTION_EMPTY_FLASK)
    return flask


def test_cheat_cast_at_mon_without_mana_skill_or_experience(lib, caster):
    index, spell = potion_spell(lib)
    for seed in (1, 2, 3):
        flask = empty_flask(lib)
        caster.slots[C.SLOT_HAND_ACTION] = flask
        var(lib, ctypes.c_uint32, "g_random_seed").value = seed
        assert lib.magic_cast_cheat_spell(0, index) == C.SPELL_CAST_SUCCESS
        data = lib.rec_write(flask)
        assert data[2] & 0x7F == spell.type
        assert POWER_MON * C.POTION_POWER_PER_LEVEL <= data[3] < POWER_MON * C.POTION_POWER_PER_LEVEL + POTION_POWER_RANDOM
    assert caster.current_mana == 0
    assert all(caster.skills[s].experience == 0 for s in range(20))
    assert caster.enable_action_event_index >= 0  # the action hand is disabled as after a normal cast


def test_cheat_cast_needs_the_flask_and_a_living_caster(lib, caster):
    index, _ = potion_spell(lib)
    assert lib.magic_cast_cheat_spell(0, index) == C.SPELL_CAST_FAILURE_NEEDS_FLASK
    caster.slots[C.SLOT_HAND_ACTION] = empty_flask(lib)
    caster.current_health = 0
    assert lib.magic_cast_cheat_spell(0, index) == C.SPELL_CAST_FAILURE
    caster.current_health = 100
    assert lib.magic_cast_cheat_spell(-1, index) == C.SPELL_CAST_FAILURE  # no spell caster
    assert lib.magic_cast_cheat_spell(0, index) == C.SPELL_CAST_SUCCESS
