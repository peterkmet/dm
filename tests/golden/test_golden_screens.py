# SPDX-License-Identifier: GPL-3.0-or-later
"""Golden snapshot of the champion screens: status boxes, the inventory panel, and the action area."""

import hashlib
from pathlib import Path

import pytest

from dmtest import constants as C
from dmtest.golden import add_second_champion, check as check_golden, golden_game, recruit, zone_box
from dmtest.game import screen as video_screen
from dmtest.markers import needs_dm

GOLDEN = Path(__file__).with_name("screens.json")
WOUND_READY_AND_ACTION_HAND = 0x03


@pytest.fixture
def game():
    return golden_game()


def screen(game):
    """SHA-256 of the frame buffer."""
    return hashlib.sha256(bytes(video_screen(game.lib).px)).hexdigest()


def check(name, got):
    """Compares the screens of a scenario with the golden file."""
    check_golden(GOLDEN, got, name)


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

