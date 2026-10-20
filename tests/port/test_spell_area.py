# SPDX-License-Identifier: GPL-3.0-or-later
"""The click highlight of the spell area: the original removes the highlight before the redraw of the runes;
a redraw inside the highlight would be inverted when the highlight ends (cyan blocks after a cast)."""

import pytest

from dmtest import constants as C
from dmtest.game import screen
from dmtest.golden import golden_game, recruit, zone_box
from dmtest.markers import needs_dm

pytestmark = [needs_dm]

LO, FUL = 0, 3  # rune indexes of the light spell


@pytest.fixture
def game():
    game = golden_game()
    recruit(game)
    game.champions[game.value("g_magic_caster_champion_index")].current_mana = 100
    return game


def box_pixels(game, zone):
    b = zone_box(game, zone)
    px = screen(game.lib).px
    return [px[y * 320 + x] for y in range(b.y, b.y + b.h) for x in range(b.x, b.x + b.w)]


def click_rune(game, rune):
    game.click_box(zone_box(game, C.ZONE_SPELL_SYMBOL_BOX_FIRST + rune), viewport=False)


def test_cast_leaves_the_rune_row_black(game):
    click_rune(game, LO)
    click_rune(game, FUL)
    game.click_box(zone_box(game, C.ZONE_SPELL_CAST_BOX), viewport=False)
    assert set(box_pixels(game, C.ZONE_SPELL_CAST_BOX)) == {C.COLOR_BLACK}


def test_clicked_rune_box_shows_the_next_rune(game):
    click_rune(game, LO)
    clicked = box_pixels(game, C.ZONE_SPELL_SYMBOL_BOX_FIRST + LO)
    game.lib.menus_draw_available_symbols(1)
    assert clicked == box_pixels(game, C.ZONE_SPELL_SYMBOL_BOX_FIRST + LO)
