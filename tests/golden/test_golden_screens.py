# SPDX-License-Identifier: GPL-3.0-or-later
"""Golden snapshot of the champion screens: status boxes, the inventory panel, the action and spell areas."""

import ctypes
import hashlib
import json
import os
from pathlib import Path

import pytest

from dmtest import constants as C
from dmtest.golden import add_second_champion, golden_game, recruit, zone_box
from dmtest.markers import needs_dm

GOLDEN = Path(__file__).with_name("screens.json")
WOUND_READY_AND_ACTION_HAND = 0x03


@pytest.fixture
def game():
    return golden_game()


def screen(game):
    """SHA-256 of the frame buffer."""
    return hashlib.sha256(bytes((ctypes.c_uint8 * (320 * 200)).in_dll(game.lib, "fb"))).hexdigest()


def check(name, got):
    """Compares the screens of a scenario with the golden file; DM_UPDATE_GOLDEN writes them."""
    golden = json.loads(GOLDEN.read_text()) if GOLDEN.exists() else {}
    if os.environ.get("DM_UPDATE_GOLDEN"):
        golden[name] = got
        GOLDEN.write_text(json.dumps(golden, indent=1, sort_keys=True) + "\n")
    assert got == golden[name]


@needs_dm
def test_status_boxes(game):
    recruit(game)
    add_second_champion(game)
    second = game.champions[1]
    second.wounds = WOUND_READY_AND_ACTION_HAND
    second.current_health //= 2
    game.lib.status_draw_all(C.CHAMPION_ATTR_REDRAW_MASK)
    got = [screen(game)]
    game.lib.status_draw_damage(1, 37)
    got.append(screen(game))
    check("status", got)


@needs_dm
def test_inventory_panel(game):
    recruit(game)
    game.lib.command_toggle_inventory(0)
    game.run()
    check("inventory", [screen(game)])


@needs_dm
def test_action_area(game):
    recruit(game)
    game.click_box(zone_box(game, C.ZONE_ACTION_ICON_BOX_FIRST), viewport=False)
    got = [screen(game)]
    game.lib.menus_draw_action_damage(25)
    got.append(screen(game))
    check("action", got)


@needs_dm
def test_spell_area(game):
    recruit(game)
    game.champions[game.value("g_magic_caster_champion_index")].current_mana = 100
    got = []
    for symbol in (0, 1):
        game.click_box(zone_box(game, C.ZONE_SPELL_SYMBOL_BOX_FIRST + symbol), viewport=False)
        got.append(screen(game))
    check("spell", got)
