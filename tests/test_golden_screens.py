"""Golden snapshot of the champion screens: status boxes, the inventory panel, the action and spell areas."""

import ctypes
import hashlib
import json
import os

from test_golden_rules import (ZONE_ACTION_ICON_FIRST, ZONE_SPELL_SYMBOL_BOX_FIRST, add_second_champion, game,  # noqa: F401
                               needs_game, recruit, zone_box)
from test_port import ROOT

GOLDEN = ROOT / "tests" / "golden" / "screens.json"
CHAMPION_ATTR_REDRAW_MASK = 0xFF80
WOUND_READY_AND_ACTION_HAND = 0x03


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


@needs_game
def test_status_boxes(game):
    recruit(game)
    add_second_champion(game)
    second = game.champions[1]
    second.wounds = WOUND_READY_AND_ACTION_HAND
    second.current_health //= 2
    game.lib.ui_draw_all_champion_states(CHAMPION_ATTR_REDRAW_MASK)
    got = [screen(game)]
    game.lib.ui_draw_champion_damage(1, 37)
    got.append(screen(game))
    check("status", got)


@needs_game
def test_inventory_panel(game):
    recruit(game)
    game.lib.command_toggle_inventory(0)
    game.run()
    check("inventory", [screen(game)])


@needs_game
def test_action_area(game):
    recruit(game)
    game.click_box(zone_box(game, ZONE_ACTION_ICON_FIRST), viewport=False)
    got = [screen(game)]
    game.lib.menus_draw_action_damage(25)
    got.append(screen(game))
    check("action", got)


@needs_game
def test_spell_area(game):
    recruit(game)
    game.champions[game.value("g_magic_caster_champion_index")].current_mana = 100
    got = []
    for symbol in (0, 1):
        game.click_box(zone_box(game, ZONE_SPELL_SYMBOL_BOX_FIRST + symbol), viewport=False)
        got.append(screen(game))
    check("spell", got)
