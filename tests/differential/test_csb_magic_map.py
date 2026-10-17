# SPDX-License-Identifier: GPL-3.0-or-later
"""CSB magic map (docs/csb/engine-diff.md "Magic map"): the map spells, their timeline events, the CREANAME scroll
text, and the map panel. DM keeps its scroll panel."""

import ctypes
import shutil

import pytest

from dmtest import constants as C
from dmtest.game import Bitmap, Box, Champion, global_type, Party, Spell, Timeline, TimelineEvent
from dmtest.lib import load_lib
from dmtest.markers import GAME, GAME_CSB, needs_csb, needs_csb_mini, needs_dm

MINI = GAME_CSB / "disk-b" / "MINI.DAT"
DM, CSB = 0, 1
STEP_EAST, STEP_NORTH = (0, 1, 0, -1), (-1, 0, 1, 0)
DAIN, ROS, KU, NETA, POSITION = range(5)
EVENT_ENABLE_ACTION, EVENT_MAGIC_MAP_FIRST = 11, 80
SPELL_MAGIC_MAP_FIRST = 25
CAST_SUCCESS, CAST_KEEP_SYMBOLS = 1, 3
POWER_SYMBOL_BEFORE_FIRST = 0x5F
MAP_CELL = 8  # tile 7 + gap 1
TEXT_MODE_SCROLL_ANY = -0x7FFE  # 0x8002
CODE_SPACE, CODE_SEPARATOR, CODE_END = 26, 28, 31
GFX_TILES, ZONE_RUNE_FIRST = 12, 615
TILE_W, TILES_PER_ROW, TILE_TRANSPARENT = 7, 16, 10
TILE_FAKE_WALL_BLINK, TILE_PIT_INVISIBLE, TILE_PIT_BLINK, TILE_EXPLOSION_FIRE, TILE_CREATURE_FIRST = 1, 4, 5, 66, 76
RUNE_ORDER = (KU, ROS, DAIN, NETA)  # rune zones 615-618
DOOR_BUTTON = 64
EXPLOSION_FIREBALL_THING = 65408
TILE_DOOR_FRONT, TILE_DOOR_SIDE, TILE_DOOR_NO_BUTTON, TILE_DOOR_OPEN, TILE_STAIRS_FIRST = 10, 11, 2, 4, 18
TILE_ZOKATHRA_FIRST, TILE_IN_WALL, TILE_FIREBALL_FIRST, TILE_FLOOR_ORNAMENT_FIRST = 34, 4, 50, 78
ORNAMENT_ALCOVE, ORNAMENT_FOUNTAIN, ORNAMENT_VI_ALTAR = 3, 4, 5  # view_get_wall_ornament
TILE_WALL_ORNAMENT_FIRST = {ORNAMENT_ALCOVE: 22, ORNAMENT_VI_ALTAR: 26, ORNAMENT_FOUNTAIN: 30}
WALL_ORNAMENT_MAP = 5  # has alcoves, a fountain, and vi altars
TIME_MASK = 0xFFFFFF
SEED = 0x12345
c16, u16, u32 = ctypes.c_int16, ctypes.c_uint16, ctypes.c_uint32



@pytest.fixture(scope="module")
def lib():
    """A private copy of libdm."""
    lib = load_lib()
    lib.gameloop_load_data.restype = ctypes.c_bool
    lib.gameloop_start_new_game.restype = ctypes.c_bool
    for name, res, args in [
        ("rec_write", ctypes.POINTER(ctypes.c_uint8), [u16]), ("map_get_square_first_thing", u16, [c16, c16]),
        ("rec_thing_next", u16, [u16]), ("map_get_square", ctypes.c_uint8, [c16, c16]), ("rec_find_unused_thing", u16, [u16]),
        ("map_link_thing_to_list", None, [u16, u16, c16, c16]), ("map_unlink_thing_from_list", None, [u16, u16, c16, c16]),
        ("dungeon_decode_text", ctypes.c_bool, [ctypes.c_char_p, c16, u16, c16]), ("tables_get_spell", ctypes.POINTER(Spell), [ctypes.c_int]),
        ("magic_cast_champion_spell", c16, [c16]), ("map_get_creature_attributes", u16, [u16]),
        ("ui_get_viewport", ctypes.POINTER(Bitmap), []), ("gfx_get_string", ctypes.c_char_p, [c16]),
        ("layout_get_zone", ctypes.c_bool, [c16, c16, c16, ctypes.POINTER(Box), ctypes.POINTER(c16), ctypes.POINTER(c16)]),
        ("savegame_load_from", ctypes.c_int, [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_void_p]),
        ("panel_open", None, [c16]), ("gfx_get_bitmap", ctypes.POINTER(Bitmap), [c16]),
        ("map_get_stairs_exit_direction", c16, [c16, c16]), ("view_get_wall_ornament", c16, [c16, c16, c16]),
        ("view_get_floor_ornament", c16, [c16, c16]), ("map_set_party_map", None, [c16]),
        ("text_get_size", ctypes.c_bool, [ctypes.c_char_p, ctypes.POINTER(ctypes.c_int16), ctypes.POINTER(ctypes.c_int16)]), ("dungeon_map_width", c16, [c16]), ("dungeon_map_height", c16, [c16]),
    ]:
        f = getattr(lib, name)
        f.restype, f.argtypes = res, args
    yield lib
    c16.in_dll(lib, "g_adventure").value = DM


def value(lib, name, kind=c16):
    return global_type(name, kind).in_dll(lib, name)


def things(lib, x, y):
    thing = lib.map_get_square_first_thing(x, y)
    while thing != C.THING_ENDOFLIST:
        yield thing
        thing = lib.rec_thing_next(thing)


def text_thing(lib, scroll):
    rec = lib.rec_write(scroll)
    return 2 << 10 | ((rec[2] << 8 | rec[3]) & 0x3FF)


def scroll_text(lib, scroll):
    buf = ctypes.create_string_buffer(200)
    lib.dungeon_decode_text(buf, 200, text_thing(lib, scroll), TEXT_MODE_SCROLL_ANY)
    return buf.value


def set_scroll_text(lib, scroll, text):
    """Writes text (letters and spaces) as 5-bit codes over the text words of the scroll."""
    codes = [CODE_SPACE if ch == " " else CODE_SEPARATOR if ch == "\n" else ord(ch) - ord("A") for ch in text] + [CODE_END]
    codes += [CODE_END] * (-len(codes) % 3)
    rec = lib.rec_write(text_thing(lib, scroll))
    offset = (rec[2] << 8 | rec[3]) >> 3
    data = ctypes.cast(ctypes.c_void_p.in_dll(lib, "g_dungeon_text_data"), ctypes.POINTER(ctypes.c_uint8))
    for i in range(0, len(codes), 3):
        word = codes[i] << 10 | codes[i + 1] << 5 | codes[i + 2]
        data[2 * (offset + i // 3)], data[2 * (offset + i // 3) + 1] = word >> 8, word & 0xFF


def map_scrolls(lib):
    """Every scroll on the floor of the current map: {text: (thing, x, y)}."""
    found = {}
    for x in range(value(lib, "g_current_map_width").value):
        for y in range(value(lib, "g_current_map_height").value):
            for t in things(lib, x, y):
                if t >> 10 & 15 == C.THING_SCROLL:
                    found[scroll_text(lib, t)] = (t, x, y)
    return found


def take(lib, found):
    thing, x, y = found
    lib.map_unlink_thing_from_list(thing, 0, x, y)
    return thing & 0x3FFF


def setup_champion(lib, index=0):
    """Champion index alive in a party of index + 1: empty hands, every skill high, no map spells."""
    value(lib, "g_party_champion_count", c16).value = index + 1
    c = (Champion * 4).in_dll(lib, "g_champions")[index]
    c.name = b"TESTER"
    c.current_health = c.maximum_health = 100
    c.current_stamina = c.maximum_stamina = 1000
    c.current_mana = c.maximum_mana = 900
    c.enable_action_event_index = c.hide_damage_received_event_index = -1
    for s in range(C.SLOT_COUNT):
        c.slots[s] = C.THING_NONE
    for skill in c.skills:
        skill.experience = 1 << 20
    for i in range(6):
        c.magic_map[i] = 0
    return c


@pytest.fixture
def csb(lib, tmp_path):
    """The CSB adventure (MINI.DAT) with one champion; the scrolls of the party map."""
    c16.in_dll(lib, "g_adventure").value = CSB
    assert lib.gameloop_load_data(str(GAME_CSB).encode())
    shutil.copy(MINI, tmp_path / "MINI.DAT")
    assert lib.savegame_load_from(str(tmp_path).encode(), b"MINI.DAT", None) == C.SAVEGAME_LOADED
    lib.gameloop_start_loaded_game()
    return setup_champion(lib), map_scrolls(lib)


def timeline(lib):
    events = ctypes.cast(Timeline.in_dll(lib, "g_timeline").events, ctypes.POINTER(TimelineEvent))
    heap = ctypes.cast(Timeline.in_dll(lib, "g_timeline").heap, ctypes.POINTER(u16))
    return [events[heap[i]] for i in range(Timeline.in_dll(lib, "g_timeline").count)]


def message_rows(lib):
    text = ((ctypes.c_char * C.MESSAGE_COLS) * C.MESSAGE_ROWS).in_dll(lib, "g_message")
    return [bytes(row).replace(b"\0", b" ").decode().strip() for row in text]


def cast(lib, c, spell_index, power, index=0):
    s = lib.tables_get_spell(spell_index).contents
    c.symbols = bytes([POWER_SYMBOL_BEFORE_FIRST + power, s.symbols >> 16 & 0xFF, s.symbols >> 8 & 0xFF, s.symbols & 0xFF, 0])
    return lib.magic_cast_champion_spell(index)


def panel(lib):
    """The viewport after the inventory of champion 0 opens; the same random seed for the teleporter tiles."""
    value(lib, "g_random_seed", u32).value = SEED
    lib.panel_open(0)
    vp = lib.ui_get_viewport().contents
    return bytes(vp.px[:vp.w * vp.h])


def map_origin(lib):
    box, w, h = Box(), c16(1), c16(1)
    assert lib.layout_get_zone(C.ZONE_MAGIC_MAP, 0, 0, ctypes.byref(box), ctypes.byref(w), ctypes.byref(h))
    return box.x, box.y


def changed_cells(lib, before, after):
    """The map cells (column, row) where the two viewports differ; -1 for a pixel outside the grid."""
    width = lib.ui_get_viewport().contents.w
    ox, oy = map_origin(lib)
    cells = set()
    for i, (a, b) in enumerate(zip(before, after)):
        if a != b:
            col, row = (i % width - ox + 1) // MAP_CELL, (i // width - oy + 1) // MAP_CELL
            cells.add((col, row) if 0 <= col < 7 and 0 <= row < 7 else -1)
    return cells


def calm(lib, x, y):
    """No teleporter and no explosion in the 7 x 7 squares around (x, y): the map draws no random tile."""
    return all(lib.rec_square_type(lib.map_get_square(x + dx, y + dy)) != C.SQUARE_TELEPORTER
               and not any(t >> 10 & 15 == C.THING_EXPLOSION for t in things(lib, x + dx, y + dy))
               for dx in range(-3, 4) for dy in range(-3, 4))


def face_free_square(lib, accept):
    """Puts the party on an empty corridor square in front of a square (x, y) where accept(x, y) is true."""
    width, height = value(lib, "g_current_map_width").value, value(lib, "g_current_map_height").value
    for x in range(width):
        for y in range(height):
            for d in range(4):
                fx, fy = x + STEP_EAST[d], y + STEP_NORTH[d]
                if 0 <= fx < width and 0 <= fy < height and lib.rec_square_type(lib.map_get_square(x, y)) == C.SQUARE_CORRIDOR \
                        and not list(things(lib, x, y)) and accept(fx, fy) and calm(lib, x, y):
                    for name, v in zip(("g_party_map_x", "g_party_map_y", "g_party_direction"), (x, y, d)):
                        value(lib, name).value = v
                    return fx, fy
    raise AssertionError("no square found")


@needs_csb
@needs_csb_mini
def test_magic_map_scrolls_in_the_dungeon(lib, csb):
    _, scrolls = csb
    assert value(lib, "g_party_map_index").value == 4
    assert scrolls[b"MAGICMAP"][1:] == (20, 11) and scrolls[b"CREANAME"][1:] == (12, 13)
    assert all(lib.tables_get_spell(i).contents.kind == C.SPELL_KIND_MAGIC_MAP for i in range(SPELL_MAGIC_MAP_FIRST, SPELL_MAGIC_MAP_FIRST + 4))


@needs_csb
@needs_csb_mini
@pytest.mark.parametrize("hand", ["empty", "CREANAME"])
def test_spell_needs_magic_map_in_action_hand(lib, csb, hand):
    c, scrolls = csb
    if hand != "empty":
        c.slots[C.SLOT_HAND_ACTION] = take(lib, scrolls[hand.encode()])
    lib.message_clear()
    count = len(timeline(lib))
    assert cast(lib, c, SPELL_MAGIC_MAP_FIRST, 3) == CAST_KEEP_SYMBOLS
    assert bytes(c.magic_map) == bytes(6)
    assert len(timeline(lib)) == count and c.enable_action_event_index == -1
    assert "TESTER NEEDS A MAGIC MAP IN ACTION HAND FOR THIS SPELL." in " ".join(row for row in message_rows(lib) if row)


@needs_csb
@needs_csb_mini
@pytest.mark.parametrize("power", [1, 6])
def test_spells_with_magic_map(lib, csb, power):
    c, scrolls = csb
    c.slots[C.SLOT_HAND_ACTION] = take(lib, scrolls[b"MAGICMAP"])
    d = (power + 1) * 4
    durations = (2 * d * d, 2 * (d - 2) ** 2, 2 * (d - ((d >> 2) - 1)) ** 2)
    for spell_type in (DAIN, ROS, KU, NETA):
        now = value(lib, "g_game_time", u32).value
        c.enable_action_event_index = -1
        assert cast(lib, c, SPELL_MAGIC_MAP_FIRST + spell_type, power) == CAST_SUCCESS
        events = timeline(lib)
        mine = [e for e in events if e.type == EVENT_MAGIC_MAP_FIRST + spell_type]
        assert len(mine) == 1 and mine[0].priority == 0 and mine[0].map_time >> 24 == 4
        ticks = lib.tables_get_spell(SPELL_MAGIC_MAP_FIRST + spell_type).contents.disabled_ticks
        if spell_type == NETA:
            ticks = d * d >> 1
            assert mine[0].map_time & TIME_MASK == now + 4 * ticks
            assert c.magic_map[NETA] == value(lib, "g_party_direction").value + 1
            assert c.magic_map[POSITION] << 8 | c.magic_map[POSITION + 1] == 22 << 11 | 18 << 6 | 4
        else:
            assert mine[0].map_time & TIME_MASK == now + durations[spell_type]
            assert c.magic_map[spell_type] == 1
        enable = [e for e in events if e.type == EVENT_ENABLE_ACTION]
        assert len(enable) == 1 and enable[0].map_time & TIME_MASK == now + ticks
        lib.timeline_delete_event(c.enable_action_event_index)
    c.enable_action_event_index = -1
    value(lib, "g_party_direction").value = 3
    assert cast(lib, c, SPELL_MAGIC_MAP_FIRST + NETA, power) == CAST_SUCCESS
    assert len([e for e in timeline(lib) if e.type == EVENT_MAGIC_MAP_FIRST + NETA]) == 1  # the first NETA event is gone
    assert c.magic_map[NETA] == 4


@needs_csb
@needs_csb_mini
def test_events_end_the_map_spells(lib, csb):
    c, scrolls = csb
    c.slots[C.SLOT_HAND_ACTION] = take(lib, scrolls[b"MAGICMAP"])
    for spell_type in (DAIN, DAIN, ROS, KU, NETA):
        c.enable_action_event_index = -1
        assert cast(lib, c, SPELL_MAGIC_MAP_FIRST + spell_type, 1) == CAST_SUCCESS
    expected = [2, 1, 1, value(lib, "g_party_direction").value + 1]
    assert list(c.magic_map[:4]) == expected
    ends = sorted((e.map_time & TIME_MASK, e.type - EVENT_MAGIC_MAP_FIRST) for e in timeline(lib) if e.type >= EVENT_MAGIC_MAP_FIRST)
    assert sorted(spell for _, spell in ends) == [DAIN, DAIN, ROS, KU, NETA]
    for time in sorted({time for time, _ in ends}):
        value(lib, "g_game_time", u32).value = time
        lib.event_process_expired()
        for spell in (spell for t, spell in ends if t == time):
            expected[spell] = 0 if spell == NETA else expected[spell] - 1
        assert list(c.magic_map[:4]) == expected
    assert list(c.magic_map[:4]) == [0, 0, 0, 0] and c.magic_map[POSITION:] != bytes(2)  # the kept position stays


@needs_csb
@needs_csb_mini
def test_panel_draws_the_map_grid(lib, csb):
    c, scrolls = csb
    plain_scroll = take(lib, scrolls[b"CREANAME"])
    set_scroll_text(lib, plain_scroll, "PRESS WESTWARD")
    c.slots[C.SLOT_HAND_ACTION] = plain_scroll
    plain = panel(lib)
    c.slots[C.SLOT_HAND_ACTION] = take(lib, scrolls[b"MAGICMAP"])
    drawn = panel(lib)
    assert {(col, row) for col in range(7) for row in range(7)} <= changed_cells(lib, plain, drawn)
    assert panel(lib) == drawn


@needs_csb
@needs_csb_mini
def test_ku_shows_the_creature_ahead(lib, csb):
    c, scrolls = csb
    c.slots[C.SLOT_HAND_ACTION] = take(lib, scrolls[b"MAGICMAP"])
    face_free_square(lib, lambda x, y: any(t >> 10 & 15 == C.THING_GROUP and not lib.map_get_creature_attributes(t) & C.CREATURE_NOT_ON_MAP
                                           for t in things(lib, x, y)))
    before = panel(lib)
    c.magic_map[KU] = 1
    assert changed_cells(lib, before, panel(lib)) == {(3, 2), -1}  # the square ahead and the rune


@needs_csb
@needs_csb_mini
def test_dain_shows_an_explosion(lib, csb):
    c, scrolls = csb
    c.slots[C.SLOT_HAND_ACTION] = take(lib, scrolls[b"MAGICMAP"])
    fx, fy = face_free_square(lib, lambda x, y: lib.rec_square_type(lib.map_get_square(x, y)) == C.SQUARE_CORRIDOR and not list(things(lib, x, y)))
    explosion = lib.rec_find_unused_thing(C.THING_EXPLOSION)
    rec = lib.rec_write(explosion)
    rec[2], rec[3] = 100, 0  # attack, fireball
    lib.map_link_thing_to_list(explosion, 0, fx, fy)
    before = panel(lib)
    c.magic_map[DAIN] = 1
    assert changed_cells(lib, before, panel(lib)) == {(3, 2), -1}  # the square ahead and the rune


@needs_csb
@needs_csb_mini
def test_ros_shows_an_invisible_pit(lib, csb):
    c, scrolls = csb
    c.slots[C.SLOT_HAND_ACTION] = take(lib, scrolls[b"MAGICMAP"])
    fx, fy = face_free_square(lib, lambda x, y: lib.rec_square_type(lib.map_get_square(x, y)) == C.SQUARE_CORRIDOR and not list(things(lib, x, y)))
    data = ctypes.cast(ctypes.c_void_p.in_dll(lib, "g_dungeon_map_data"), ctypes.POINTER(ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8))))
    data[4][fx][fy] = C.SQUARE_PIT << 5 | C.PIT_OPEN | C.PIT_INVISIBLE
    value(lib, "g_game_time", u32).value |= 1  # the blinking time
    before = panel(lib)
    c.magic_map[ROS] = 1
    assert changed_cells(lib, before, panel(lib)) == {(3, 2), -1}  # the square ahead and the rune


@needs_csb
@needs_csb_mini
def test_runes_light_with_the_spells(lib, csb):
    c, scrolls = csb
    c.slots[C.SLOT_HAND_ACTION] = take(lib, scrolls[b"MAGICMAP"])
    before = panel(lib)
    c.enable_action_event_index = -1
    assert cast(lib, c, SPELL_MAGIC_MAP_FIRST + NETA, 1) == CAST_SUCCESS  # keeps the map that the party sees now
    assert changed_cells(lib, before, panel(lib)) == {-1}  # only the NETA rune


@needs_csb
@needs_csb_mini
def test_neta_keeps_the_map(lib, csb):
    c, scrolls = csb
    c.slots[C.SLOT_HAND_ACTION] = take(lib, scrolls[b"MAGICMAP"])
    assert cast(lib, c, SPELL_MAGIC_MAP_FIRST + NETA, 1) == CAST_SUCCESS
    kept = panel(lib)
    c.magic_map[NETA] = 0
    value(lib, "g_party_direction").value = (value(lib, "g_party_direction").value + 1) % 4
    turned = panel(lib)
    c.magic_map[NETA] = (value(lib, "g_party_direction").value + 3) % 4 + 1  # the direction before the turn
    assert panel(lib) == kept != turned


def grid_after_tick(lib):
    """The grid pixels after the viewport is cleared and one game tick runs."""
    vp = lib.ui_get_viewport().contents
    ctypes.memset(vp.px, 0, vp.w * vp.h)
    lib.gameloop_tick()
    ox, oy = map_origin(lib)
    return [vp.px[y * vp.w + x] for y in range(oy, oy + 7 * MAP_CELL) for x in range(ox, ox + 7 * MAP_CELL)]


@needs_csb
@needs_csb_mini
@pytest.mark.parametrize("text", ["MAGICMAP", "CREANAME", "PRESS WESTWARD"])
def test_panel_draws_again_each_tick(lib, csb, text):
    c, scrolls = csb
    scroll = take(lib, scrolls[b"CREANAME"])
    set_scroll_text(lib, scroll, text)
    c.slots[C.SLOT_HAND_ACTION] = scroll
    panel(lib)
    assert any(grid_after_tick(lib)) == (text != "PRESS WESTWARD")


def face_with_creaname_scroll(lib, c, scrolls, group_ahead):
    scroll = take(lib, scrolls[b"CREANAME"])
    c.slots[C.SLOT_HAND_ACTION] = scroll
    if group_ahead:
        face_free_square(lib, lambda x, y: any(t >> 10 & 15 == C.THING_GROUP for t in things(lib, x, y)))
    else:
        face_free_square(lib, lambda x, y: not any(t >> 10 & 15 == C.THING_GROUP for t in things(lib, x, y)))
    return scroll


@needs_csb
@needs_csb_mini
@pytest.mark.parametrize("group_ahead", [False, True])
def test_creaname_becomes_the_creature_name(lib, csb, group_ahead):
    c, scrolls = csb
    scroll = face_with_creaname_scroll(lib, c, scrolls, group_ahead)
    x, y = value(lib, "g_party_map_x").value, value(lib, "g_party_map_y").value
    d = value(lib, "g_party_direction").value
    group = next((t for t in things(lib, x + STEP_EAST[d], y + STEP_NORTH[d]) if t >> 10 & 15 == C.THING_GROUP), None)
    name = lib.gfx_get_string(107 if group is None else 108 + lib.rec_write(group)[4]).decode()
    replaced = panel(lib)
    set_scroll_text(lib, scroll, name)
    assert panel(lib) == replaced


@needs_dm
@pytest.mark.parametrize("text", ["CREANAME", "MAGICMAP"])
def test_dm_scroll_panel_unchanged(lib, text):
    c16.in_dll(lib, "g_adventure").value = DM
    assert lib.gameloop_load_data(str(GAME).encode())
    assert lib.gameloop_start_new_game(str(GAME).encode())
    c = setup_champion(lib)
    scroll = next(t for x in range(lib.dungeon_map_width(0)) for y in range(lib.dungeon_map_height(0))
                  for t in things(lib, x, y) if t >> 10 & 15 == C.THING_SCROLL)
    c.slots[C.SLOT_HAND_ACTION] = scroll & 0x3FFF
    set_scroll_text(lib, scroll, text)
    face_free_square(lib, lambda x, y: lib.rec_square_type(lib.map_get_square(x, y)) == C.SQUARE_CORRIDOR)
    plain = panel(lib)
    for i in range(4):
        c.magic_map[i] = 1
    assert panel(lib) == plain


def bitmap(lib, graphic):
    return lib.gfx_get_bitmap(graphic).contents


def cell_origin(lib, col, row):
    ox, oy = map_origin(lib)
    return ox + col * MAP_CELL, oy + row * MAP_CELL


def assert_tile(lib, view, col, row, tile):
    """The cell shows the opaque pixels of the tile of graphic 12."""
    tiles, width = bitmap(lib, GFX_TILES), lib.ui_get_viewport().contents.w
    px, py = cell_origin(lib, col, row)
    sx, sy = tile % TILES_PER_ROW * TILE_W, tile // TILES_PER_ROW * TILE_W
    for y in range(TILE_W):
        for x in range(TILE_W):
            source = tiles.px[(sy + y) * tiles.w + sx + x]
            if source != TILE_TRANSPARENT:
                assert view[(py + y) * width + px + x] == source, (tile, x, y)


def border(lib, view, col, row):
    """The colors of the 1 pixel ring around a cell."""
    width = lib.ui_get_viewport().contents.w
    px, py = cell_origin(lib, col, row)
    ring = [(x, y) for x in range(px - 1, px + TILE_W + 1) for y in (py - 1, py + TILE_W)]
    ring += [(x, y) for x in (px - 1, px + TILE_W) for y in range(py, py + TILE_W)]
    return {view[y * width + x] for x, y in ring}


def empty_corridor(lib, x, y):
    return lib.rec_square_type(lib.map_get_square(x, y)) == C.SQUARE_CORRIDOR and not list(things(lib, x, y))


@needs_csb
@needs_csb_mini
def test_map_spells_of_another_champion(lib, csb):
    c0, scrolls = csb
    c1 = setup_champion(lib, 1)
    c1.slots[C.SLOT_HAND_ACTION] = take(lib, scrolls[b"MAGICMAP"])
    for spell_type in (DAIN, ROS, KU, NETA):
        c1.enable_action_event_index = -1
        assert cast(lib, c1, SPELL_MAGIC_MAP_FIRST + spell_type, 1, index=1) == CAST_SUCCESS
    mine = [e for e in timeline(lib) if e.type >= EVENT_MAGIC_MAP_FIRST]
    assert len(mine) == 4 and {e.priority for e in mine} == {1}
    assert list(c1.magic_map[:4]) == [1, 1, 1, value(lib, "g_party_direction").value + 1] and bytes(c0.magic_map) == bytes(6)
    value(lib, "g_game_time", u32).value = max(e.map_time & TIME_MASK for e in mine)
    lib.event_process_expired()
    assert list(c1.magic_map[:4]) == [0, 0, 0, 0] and bytes(c0.magic_map) == bytes(6)


@needs_csb
@needs_csb_mini
def test_panel_shows_the_magic_map_graphic(lib, csb):
    c, scrolls = csb
    c.slots[C.SLOT_HAND_ACTION] = take(lib, scrolls[b"MAGICMAP"])
    view = panel(lib)
    graphic, scroll = bitmap(lib, C.GFX_PANEL_MAGIC_MAP), bitmap(lib, C.GFX_PANEL_SCROLL)
    box, w, h = Box(), c16(0), c16(0)
    assert lib.layout_get_zone(C.ZONE_PANEL, graphic.w, graphic.h, ctypes.byref(box), ctypes.byref(w), ctypes.byref(h))
    ox, oy = map_origin(lib)
    width = lib.ui_get_viewport().contents.w
    checked = 0
    for y in range(box.h):
        for x in range(box.w):
            vx, vy, color = box.x + x, box.y + y, graphic.px[(h.value + y) * graphic.w + w.value + x]
            in_grid = ox - 2 <= vx < ox + 7 * MAP_CELL + 1 and oy - 2 <= vy < oy + 7 * MAP_CELL + 1
            if color != C.COLOR_RED and not in_grid and (y >= scroll.h or x >= scroll.w or color != scroll.px[y * scroll.w + x]):
                if all(abs(vx - rx) > 10 or abs(vy - ry) > 10 for rx, ry in rune_points(lib)):
                    assert view[vy * width + vx] == color, (x, y)
                    checked += 1
    assert checked > 100


def rune_points(lib):
    points = []
    for i in range(4):
        box, w, h = Box(), c16(1), c16(1)
        assert lib.layout_get_zone(ZONE_RUNE_FIRST + i, 0, 0, ctypes.byref(box), ctypes.byref(w), ctypes.byref(h))
        points.append((box.x, box.y))
    return points


def rune_colors(lib, view, i):
    """The colors in the box of rune zone i (the box of ui_print_zone)."""
    tw, th = c16(), c16()
    assert lib.text_get_size(bytes([0x72 + i]), ctypes.byref(tw), ctypes.byref(th))
    box, w, h = Box(), c16(tw.value), c16(th.value)
    assert lib.layout_get_zone(ZONE_RUNE_FIRST + i, 0, 0, ctypes.byref(box), ctypes.byref(w), ctypes.byref(h))
    width = lib.ui_get_viewport().contents.w
    return {view[y * width + x] for y in range(box.y, box.y + box.h) for x in range(box.x, box.x + box.w)}


@needs_csb
@needs_csb_mini
@pytest.mark.parametrize("spell", [DAIN, ROS, KU, NETA])
def test_rune_of_each_spell(lib, csb, spell):
    c, scrolls = csb
    c.slots[C.SLOT_HAND_ACTION] = take(lib, scrolls[b"MAGICMAP"])
    c.magic_map[spell] = 1
    view = panel(lib)
    for i, rune_spell in enumerate(RUNE_ORDER):
        colors = rune_colors(lib, view, i)
        lit = rune_spell == spell
        assert (C.COLOR_RED in colors, C.COLOR_BLACK in colors) == (lit, not lit), (i, colors)


@needs_csb
@needs_csb_mini
def test_ku_creature_tile(lib, csb):
    c, scrolls = csb
    c.slots[C.SLOT_HAND_ACTION] = take(lib, scrolls[b"MAGICMAP"])
    face_free_square(lib, lambda x, y: any(t >> 10 & 15 == C.THING_GROUP and not lib.map_get_creature_attributes(t) & C.CREATURE_NOT_ON_MAP
                                       for t in things(lib, x, y)))
    c.magic_map[KU] = 1
    for time in (0, 1):
        value(lib, "g_game_time", u32).value = time
        assert_tile(lib, panel(lib), 3, 2, TILE_CREATURE_FIRST + time)


@needs_csb
@needs_csb_mini
def test_dain_explosion_tile(lib, csb):
    c, scrolls = csb
    c.slots[C.SLOT_HAND_ACTION] = take(lib, scrolls[b"MAGICMAP"])
    fx, fy = face_free_square(lib, lambda x, y: empty_corridor(lib, x, y))
    explosion = lib.rec_find_unused_thing(C.THING_EXPLOSION)
    rec = lib.rec_write(explosion)
    rec[2], rec[3] = 100, 0  # attack, fireball
    lib.map_link_thing_to_list(explosion, 0, fx, fy)
    c.magic_map[DAIN] = 1
    assert_tile(lib, panel(lib), 3, 2, TILE_EXPLOSION_FIRE)


def set_square(lib, x, y, square):
    data = ctypes.cast(ctypes.c_void_p.in_dll(lib, "g_dungeon_map_data"), ctypes.POINTER(ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8))))
    data[value(lib, "g_party_map_index").value][x][y] = square


@needs_csb
@needs_csb_mini
def test_ros_pit_and_fake_wall_tiles(lib, csb):
    c, scrolls = csb
    c.slots[C.SLOT_HAND_ACTION] = take(lib, scrolls[b"MAGICMAP"])
    fx, fy = face_free_square(lib, lambda x, y: empty_corridor(lib, x, y))
    set_square(lib, fx, fy, C.SQUARE_PIT << 5 | C.PIT_OPEN | C.PIT_INVISIBLE)
    value(lib, "g_game_time", u32).value = 1  # bit 0: the pit blinks
    assert_tile(lib, panel(lib), 3, 2, TILE_PIT_INVISIBLE)
    c.magic_map[ROS] = 1
    assert_tile(lib, panel(lib), 3, 2, TILE_PIT_BLINK)
    value(lib, "g_game_time", u32).value = 2
    assert_tile(lib, panel(lib), 3, 2, TILE_PIT_INVISIBLE)
    set_square(lib, fx, fy, C.SQUARE_FAKE_WALL << 5 | C.FAKE_WALL_IMAGINARY)  # bit 1 of the time: the fake wall blinks
    assert_tile(lib, panel(lib), 3, 2, TILE_FAKE_WALL_BLINK)
    c.magic_map[ROS] = 0
    with pytest.raises(AssertionError):
        assert_tile(lib, panel(lib), 3, 2, TILE_FAKE_WALL_BLINK)


def add_scent(lib, x, y):
    """A footprint of the party on (x, y) of the party map, inside the footprints range."""
    party = Party.in_dll(lib, "g_party")
    party.scents[0] = x << 11 | y << 6 | value(lib, "g_party_map_index").value
    party.scent_count, party.first_scent_index, party.last_scent_index = 1, 0, 1


@needs_csb
@needs_csb_mini
def test_party_and_footprint_borders(lib, csb):
    c, scrolls = csb
    c.slots[C.SLOT_HAND_ACTION] = take(lib, scrolls[b"MAGICMAP"])
    width, height = value(lib, "g_current_map_width").value, value(lib, "g_current_map_height").value
    x, y, d = next((x, y, d) for x in range(width) for y in range(height) for d in range(4)
                   if empty_corridor(lib, x, y) and empty_corridor(lib, x + STEP_EAST[(d + 1) % 4], y + STEP_NORTH[(d + 1) % 4]) and calm(lib, x, y))
    for name, v in zip(("g_party_map_x", "g_party_map_y", "g_party_direction"), (x, y, d)):
        value(lib, name).value = v
    right = (d + 1) % 4
    add_scent(lib, x + STEP_EAST[right], y + STEP_NORTH[right])
    view = panel(lib)
    assert border(lib, view, 3, 3) == {C.COLOR_RED}
    assert C.COLOR_CYAN not in border(lib, view, 4, 3)
    c.magic_map[DAIN] = 1
    view = panel(lib)
    assert border(lib, view, 3, 3) == {C.COLOR_RED}  # the party square comes last and keeps its border
    px, py = cell_origin(lib, 4, 3)
    width = lib.ui_get_viewport().contents.w
    assert {view[yy * width + px + TILE_W] for yy in range(py - 1, py + TILE_W + 1)} == {C.COLOR_CYAN}  # the right side


@needs_csb
@needs_csb_mini
def test_eye_keeps_the_leader_map(lib, csb):
    c, scrolls = csb
    plain = take(lib, scrolls[b"CREANAME"])
    set_scroll_text(lib, plain, "PRESS WESTWARD")
    c.slots[C.SLOT_HAND_ACTION] = plain
    face_free_square(lib, lambda x, y: True)
    panel(lib)
    value(lib, "g_leader_hand_object", u16).value = take(lib, scrolls[b"MAGICMAP"])
    value(lib, "g_leader_empty_handed").value = 0
    lib.panel_draw_eye()
    expected = grid_pixels(lib)
    assert grid_after_tick(lib) == expected


def grid_pixels(lib):
    vp = lib.ui_get_viewport().contents
    ox, oy = map_origin(lib)
    return [vp.px[y * vp.w + x] for y in range(oy, oy + 7 * MAP_CELL) for x in range(ox, ox + 7 * MAP_CELL)]


@needs_csb
@needs_csb_mini
@pytest.mark.parametrize("text, cast_ok, map_shown", [("MAGICMAP", True, True), ("MAGICMAPX", False, False),
                                                       ("MAGICMAP\nMORE", False, True), ("A\nMAGICMAP", False, False)])
def test_only_the_exact_text_is_a_magic_map(lib, csb, text, cast_ok, map_shown):
    c, scrolls = csb
    scroll = take(lib, scrolls[b"CREANAME"])
    set_scroll_text(lib, scroll, text)
    assert scroll_text(lib, scroll) == text.encode()
    c.slots[C.SLOT_HAND_ACTION] = scroll
    plain = take(lib, scrolls[b"MAGICMAP"])
    set_scroll_text(lib, plain, "PRESS WESTWARD")
    assert cast(lib, c, SPELL_MAGIC_MAP_FIRST, 1) == (CAST_SUCCESS if cast_ok else CAST_KEEP_SYMBOLS)
    c.magic_map[DAIN] = 0
    shown = panel(lib)
    c.slots[C.SLOT_HAND_ACTION] = plain
    assert (changed_cells(lib, panel(lib), shown) >= {(col, row) for col in range(7) for row in range(7)}) == map_shown


def party_dir(lib):
    return value(lib, "g_party_direction").value


@needs_csb
@needs_csb_mini
def test_door_tiles(lib, csb):
    c, scrolls = csb
    c.slots[C.SLOT_HAND_ACTION] = take(lib, scrolls[b"MAGICMAP"])
    fx, fy = face_free_square(lib, lambda x, y: lib.rec_square_type(lib.map_get_square(x, y)) == C.SQUARE_DOOR)
    door = lib.rec_write(lib.map_get_square_first_thing(fx, fy))
    square = lib.map_get_square(fx, fy) & ~C.DOOR_STATE_MASK
    side = (party_dir(lib) & 1) == bool(square & C.DOOR_NORTH_SOUTH)
    for state in (C.DOOR_CLOSED, C.DOOR_OPEN, C.DOOR_DESTROYED):
        for button in (True, False):
            set_square(lib, fx, fy, square | state)
            door[3] = door[3] | DOOR_BUTTON if button else door[3] & ~DOOR_BUTTON
            tile = (TILE_DOOR_SIDE if side else TILE_DOOR_FRONT) + (0 if button else TILE_DOOR_NO_BUTTON) + (0 if state == C.DOOR_CLOSED else TILE_DOOR_OPEN)
            assert_tile(lib, panel(lib), 3, 2, tile)


@needs_csb
@needs_csb_mini
def test_stairs_tiles(lib, csb):
    c, scrolls = csb
    c.slots[C.SLOT_HAND_ACTION] = take(lib, scrolls[b"MAGICMAP"])
    fx, fy = face_free_square(lib, lambda x, y: lib.rec_square_type(lib.map_get_square(x, y)) == C.SQUARE_STAIRS)
    square = lib.map_get_square(fx, fy) & ~C.STAIRS_UP
    for up in (0, 2):
        set_square(lib, fx, fy, square | (C.STAIRS_UP if up else 0))
        side = (lib.map_get_stairs_exit_direction(fx, fy) - party_dir(lib) + up) % 4
        assert_tile(lib, panel(lib), 3, 2, TILE_STAIRS_FIRST + side)


def link_new(lib, thing_type, fx, fy, cell, fill):
    thing = lib.rec_find_unused_thing(thing_type)
    fill(lib.rec_write(thing))
    lib.map_link_thing_to_list(thing | cell << 14, 0, fx, fy)


@needs_csb
@needs_csb_mini
def test_fireball_projectile_tile(lib, csb):
    c, scrolls = csb
    c.slots[C.SLOT_HAND_ACTION] = take(lib, scrolls[b"MAGICMAP"])
    fx, fy = face_free_square(lib, lambda x, y: empty_corridor(lib, x, y))
    cell = (party_dir(lib) + 1) % 4  # one side to the right of the map direction

    def fill(rec):
        rec[2], rec[3], rec[4], rec[5], rec[6], rec[7] = EXPLOSION_FIREBALL_THING >> 8, EXPLOSION_FIREBALL_THING & 0xFF, 100, 100, 0, 0
    link_new(lib, C.THING_PROJECTILE, fx, fy, cell, fill)
    c.magic_map[DAIN] = 1
    assert_tile(lib, panel(lib), 3, 2, TILE_FIREBALL_FIRST + 1)


def zokathra(rec):
    rec[2], rec[3] = 0, C.JUNK_ZOKATHRA


@needs_csb
@needs_csb_mini
@pytest.mark.parametrize("in_wall", [False, True])
def test_zokathra_tile(lib, csb, in_wall):
    c, scrolls = csb
    c.slots[C.SLOT_HAND_ACTION] = take(lib, scrolls[b"MAGICMAP"])
    kind = C.SQUARE_WALL if in_wall else C.SQUARE_CORRIDOR
    fx, fy = face_free_square(lib, lambda x, y: lib.rec_square_type(lib.map_get_square(x, y)) == kind and not list(things(lib, x, y)))
    link_new(lib, C.THING_JUNK, fx, fy, (party_dir(lib) + 2) % 4, zokathra)
    c.magic_map[DAIN] = 1
    assert_tile(lib, panel(lib), 3, 2, TILE_ZOKATHRA_FIRST + 2 + (TILE_IN_WALL if in_wall else 0))


@needs_csb
@needs_csb_mini
def test_floor_ornament_tile(lib, csb):
    c, scrolls = csb
    c.slots[C.SLOT_HAND_ACTION] = take(lib, scrolls[b"MAGICMAP"])
    panel(lib)  # the view has the party map
    fx, fy = face_free_square(lib, lambda x, y: lib.rec_square_type(lib.map_get_square(x, y)) == C.SQUARE_CORRIDOR and lib.view_get_floor_ornament(x, y) >= 0)
    assert_tile(lib, panel(lib), 3, 2, TILE_FLOOR_ORNAMENT_FIRST + lib.view_get_floor_ornament(fx, fy))


@needs_csb
@needs_csb_mini
@pytest.mark.parametrize("ornament", [ORNAMENT_ALCOVE, ORNAMENT_VI_ALTAR, ORNAMENT_FOUNTAIN])
def test_wall_ornament_tiles(lib, csb, ornament):
    c, scrolls = csb
    c.slots[C.SLOT_HAND_ACTION] = take(lib, scrolls[b"MAGICMAP"])
    lib.map_set_party_map(WALL_ORNAMENT_MAP)
    panel(lib)  # the view loads the party map
    fx, fy = face_free_square(lib, lambda x, y: any(lib.view_get_wall_ornament(x, y, f) == ornament for f in range(4)))
    view = panel(lib)
    for face in range(4):
        if lib.view_get_wall_ornament(fx, fy, face) == ornament:
            assert_tile(lib, view, 3, 2, TILE_WALL_ORNAMENT_FIRST[ornament] + (face - party_dir(lib)) % 4)
