import ctypes
import struct
import subprocess

import pytest

from dmtools.emu import Emu
from dmtools.ftl import load_image, parse
from test_port import BUILD, ROOT, sdl3_installed
from test_recruit import RecruitData, mirrors
from test_view import SCRATCH, Bitmap, Box, png, zone_records

GAME = ROOT / "game"
GET_ZONE_BOX_CLIPPED, COMMAND_FROM_MOUSE, LAYOUT_ZONES = 0x11686A, 0x120CC6, 0x102990
MOUSE_TABLES = [0x1031F8, 0x1032E8, 0x103354, 0x10351C, 0x103540, 0x103564, 0x1035A0, 0x1035DC, 0x103654, 0x1036F0, 0x10375C, 0x10378C]
DUNGEON_PALETTE, ARROWS_GFX, ARROWS_ZONE = 0x100AA6, 13, 9
ATTR_ALL = 0xFF80
pytestmark = pytest.mark.skipif(not sdl3_installed(), reason="clang or SDL3 not installed")
needs_game = pytest.mark.skipif(not all((GAME / f).exists() for f in ("KAOS.FTL", "Graphics.DAT", "Dungeon.DAT")), reason="game files missing")
u8, u16, c16 = ctypes.c_uint8, ctypes.c_uint16, ctypes.c_int16


class Skill(ctypes.Structure):
    _fields_ = [("temporary_experience", c16), ("experience", ctypes.c_int32)]


class Champion(ctypes.Structure):
    _fields_ = [
        ("name", ctypes.c_char * 8), ("title", ctypes.c_char * 20),
        ("direction", u8), ("cell", u8), ("action_index", u8), ("symbol_step", u8), ("symbols", ctypes.c_char * 5),
        ("direction_maximum_damage_received", u8), ("maximum_damage_received", u8), ("poison_event_count", u8),
        ("enable_action_event_index", c16), ("hide_damage_received_event_index", c16),
        ("attributes", u16), ("wounds", u16),
        ("current_health", c16), ("maximum_health", c16), ("current_stamina", c16), ("maximum_stamina", c16),
        ("current_mana", c16), ("maximum_mana", c16), ("action_defense", c16), ("food", c16), ("water", c16),
        ("statistics", (u8 * 3) * 7), ("skills", Skill * 20), ("slots", u16 * 30), ("load", u16), ("shield_defense", c16), ("magic_map", u8 * 6),
    ]


@pytest.fixture(scope="module")
def lib():
    subprocess.run(["make", "-C", "port"], cwd=ROOT, check=True, capture_output=True)
    lib = ctypes.CDLL(str(BUILD / "libdm.dylib"))
    lib.layout_get_zone_box.argtypes = [c16, c16, c16, ctypes.POINTER(Box)]
    lib.ui_command_from_mouse.argtypes = [c16, c16, c16, u16]
    lib.ui_command_from_mouse.restype = c16
    lib.view_render.argtypes = [c16] * 4
    lib.gfx_bitmap.restype = ctypes.POINTER(Bitmap)
    lib.ui_zone_box.argtypes = [c16, ctypes.POINTER(Box)]
    lib.message_print.argtypes = [c16, ctypes.c_char_p, ctypes.c_uint32]
    lib.recruit_parse.argtypes = [u16, ctypes.POINTER(RecruitData)]
    lib.dungeon_square_first_thing.restype = u16
    lib.get_thing_data.restype = ctypes.POINTER(u8)
    lib.recruit_portrait_index.argtypes = [u16]
    lib.recruit_text_thing.restype = u16
    return lib


@pytest.fixture(scope="module")
def game(lib):
    if not all((GAME / f).exists() for f in ("KAOS.FTL", "Graphics.DAT", "Dungeon.DAT")):
        pytest.skip("game files missing")
    assert lib.kaos_load(str(GAME / "KAOS.FTL").encode()) == 0
    assert lib.gfx_open(str(GAME / "Graphics.DAT").encode()) > 0
    assert lib.layout_init() == 0 and lib.text_init() == 0 and lib.tables_init() == 0
    assert lib.load_dungeon(str(GAME / "Dungeon.DAT").encode())
    lib.game_new()
    assert lib.ui_init() == 0
    return lib


@pytest.fixture(scope="module")
def emu(game):
    """The original code with the layout zones of Graphics.DAT in memory."""
    emu = Emu(load_image(parse((GAME / "KAOS.FTL").read_bytes()), 0x100000), max_cycles=50_000_000)
    size = ctypes.c_uint16()
    game.gfx_item.restype = ctypes.POINTER(ctypes.c_uint8)
    raw = ctypes.string_at(game.gfx_item(696, ctypes.byref(size)), size.value)
    chunks, at = zone_records(raw), 0x700000
    emu.write(LAYOUT_ZONES, struct.pack(">I", at))
    for i, (first, last, recs) in enumerate(chunks):
        nxt = at + 8 + len(recs) if i + 1 < len(chunks) else 0
        emu.write(at, struct.pack(">Ihh", nxt, first, last) + recs)
        at = nxt
    emu.zones = [z for first, last, _ in chunks for z in range(first, last + 1)]
    return emu


def test_palette_split(lib):
    pal = (u16 * 32).in_dll(lib, "palette")
    top, bottom = c16.in_dll(lib, "palette_split_top"), c16.in_dll(lib, "palette_split_bottom")
    saved = list(pal), top.value, bottom.value
    pal[3], pal[19], top.value, bottom.value = 0x123, 0x456, 33, 169
    lib.video_color.restype = u16
    assert [lib.video_color(y, 3) for y in (0, 32, 33, 168, 169, 199)] == [0x456, 0x456, 0x123, 0x123, 0x456, 0x456]
    pal[:], top.value, bottom.value = saved


@needs_game
def test_zone_box_matches_original(game, emu):
    checked = 0
    for zone in emu.zones:
        for wp, hp in ((10000, 10000), (5000, 10000), (2500, 7000)):
            emu.write(0x6F0000, bytes(8))
            found = emu.call(GET_ZONE_BOX_CLIPPED, [(2, zone), (4, 0x6F0000), (2, wp), (2, hp)]) & 0xFFFFFFFF
            box = Box()
            ok = game.layout_get_zone_box(zone, wp, hp, ctypes.byref(box))
            assert bool(ok) == bool(found), zone
            if ok:
                assert (box.x, box.y, box.w, box.h) == struct.unpack(">4h", emu.read(0x6F0000, 8)), (zone, wp, hp)
                checked += 1
    assert checked > 300


def table_points(emu, table):
    """Points around every box of a mouse input table, plus a coarse grid of the screen."""
    pts = {(x, y) for x in range(0, 320, 9) for y in range(0, 200, 7)}
    a = table
    while (entry := struct.unpack(">6h", emu.read(a, 12)))[0]:
        a += 12
        box = Box()
        if entry[1] >= 0:
            x1, x2, y1, y2 = entry[1:5]
        elif emu_zone_box(emu, entry[2], box):
            dx, dy = (0, 33) if entry[1] == -2 else (48, 32) if entry[1] == -3 else (0, 0)
            x1, x2, y1, y2 = box.x + dx, box.x + dx + box.w - 1, box.y + dy, box.y + dy + box.h - 1
        else:
            continue
        pts |= {(x, y) for x in (x1 - 1, x1, x2, x2 + 1) for y in (y1 - 1, y1, (y1 + y2) // 2, y2, y2 + 1)}
    return sorted(p for p in pts if 0 <= p[0] < 320 and 0 <= p[1] < 200)


def emu_zone_box(emu, zone, box):
    emu.write(0x6F0000, bytes(8))
    if not emu.call(GET_ZONE_BOX_CLIPPED, [(2, zone), (4, 0x6F0000), (2, 10000), (2, 10000)]) & 0xFFFFFFFF:
        return False
    box.x, box.y, box.w, box.h = struct.unpack(">4h", emu.read(0x6F0000, 8))
    return True


@needs_game
@pytest.mark.parametrize("table", range(len(MOUSE_TABLES)))
def test_mouse_commands_match_original(game, emu, table):
    hits = 0
    for x, y in table_points(emu, MOUSE_TABLES[table]):
        for mask in (1, 2):
            want = emu.call(COMMAND_FROM_MOUSE, [(4, MOUSE_TABLES[table]), (2, x), (2, y), (2, mask)]) & 0xFFFF
            assert game.ui_command_from_mouse(table, x, y, mask) == ctypes.c_int16(want).value, (table, x, y, mask)
            hits += want != 0
    assert hits > 0


def rgb_rows(lib):
    """The screen as RGB rows, with the palette split of the copper list."""
    fb = (ctypes.c_uint8 * (320 * 200)).in_dll(lib, "fb")
    pal = (ctypes.c_uint16 * 32).in_dll(lib, "palette")
    top, bottom = c16.in_dll(lib, "palette_split_top").value, c16.in_dll(lib, "palette_split_bottom").value
    rows = []
    for y in range(200):
        base = 0 if top <= y < bottom else 16
        rows.append([(pal[base + fb[y * 320 + x]] >> s & 15) * 17 for x in range(320) for s in (8, 4, 0)])
    return bytes(fb), rows


def recruit(lib, n):
    """Fills champions 0..n-1 from real mirror texts, with objects in hands and body slots."""
    champions = (Champion * 4).in_dll(lib, "g_champions")
    found = sorted(mirrors(lib))[:n]
    weapons = list(range(8))
    for i, (portrait, text) in enumerate(found):
        r = RecruitData()
        assert lib.recruit_parse(text, ctypes.byref(r)) == 1
        c = Champion()
        c.name, c.title = r.name, r.title
        c.cell, c.direction = i, 0
        for f in ("current_health", "maximum_health", "current_stamina", "maximum_stamina", "current_mana", "maximum_mana"):
            setattr(c, f, getattr(r, f))
        c.current_health = c.maximum_health * (4 - i) // 4
        c.current_mana = c.maximum_mana // (i + 1)
        c.statistics = r.statistics
        for s in range(20):
            c.skills[s].experience = r.experience[s]
        c.slots[:] = [0xFFFF] * 30
        c.slots[0] = 5 << 10 | weapons[2 * i]
        if i != 2:
            c.slots[1] = 5 << 10 | weapons[2 * i + 1]
        c.slots[3] = 6 << 10 | i
        c.slots[13] = 10 << 10 | i
        c.food, c.water = 1500 - 900 * i, 1800 - 1000 * i
        c.load = 123 + 100 * i
        c.action_index = 0xFF
        c.enable_action_event_index = c.hide_damage_received_event_index = -1
        champions[i] = c
        lib.champion_set_portrait(i, portrait)
    c16.in_dll(lib, "g_leader_index").value = 0 if n else -1
    u16.in_dll(lib, "g_party_champion_count").value = n
    c16.in_dll(lib, "g_magic_caster_champion_index").value = -1
    return champions


def render(lib, name):
    """The dungeon view at the party start and the whole interface; saves a PNG in the scratchpad."""
    ctypes.memset(ctypes.addressof(ctypes.c_uint8.in_dll(lib, "fb")), 0, 320 * 200)
    party = [c16.in_dll(lib, n).value for n in ("g_party_map_index", "g_party_map_x", "g_party_map_y", "g_party_direction")]
    assert lib.view_render(*party)
    lib.ui_set_palette(None)
    lib.ui_draw_interface()
    fb, rows = rgb_rows(lib)
    if SCRATCH.exists():
        png(SCRATCH / f"ui_{name}.png", rows)
    return fb


def reset(lib):
    c16.in_dll(lib, "g_inventory_champion_ordinal").value = 0
    c16.in_dll(lib, "g_acting_champion_ordinal").value = 0
    c16.in_dll(lib, "g_action_area_contains_icons").value = 1
    lib.message_clear()


@needs_game
def test_render_empty_party(game):
    reset(game)
    recruit(game, 0)
    fb = render(game, "empty")
    arrows = game.gfx_bitmap(ARROWS_GFX).contents
    box = Box()
    assert game.ui_zone_box(ARROWS_ZONE, ctypes.byref(box))
    assert all(fb[(box.y + y) * 320 + box.x + x] == arrows.px[y * arrows.w + x] for y in range(arrows.h) for x in range(arrows.w))
    assert set(fb[:320 * 33]) == {0}  # no status boxes


@needs_game
@pytest.mark.parametrize("n", [1, 2, 3, 4])
def test_render_party(game, n):
    reset(game)
    champions = recruit(game, n)
    if n == 4:
        champions[3].current_health = 0
        champions[1].wounds = 0x3
        champions[2].attributes = 0x0008
        game.message_print(15, b"\nWELCOME TO THE DUNGEON.", 0)
        game.message_print(9, b"\nTHE PARTY IS READY.", 0)
        game.menus_set_magic_caster(1)
    fb = render(game, f"party{n}")
    box = Box()
    for i in range(n):
        assert game.ui_zone_box(151 + i, ctypes.byref(box))
        colors = {fb[(box.y + y) * 320 + box.x + x] for y in range(box.h) for x in range(box.w)}
        assert len(colors) > 3
    if n == 4:
        game.ui_draw_champion_damage(1, 37)
        game.menus_draw_action_damage(25)
        _, rows = rgb_rows(game)
        if SCRATCH.exists():
            png(SCRATCH / "ui_damage.png", rows)


@needs_game
def test_render_action_menu(game):
    reset(game)
    recruit(game, 4)
    render(game, "base")
    game.menus_set_acting_champion(0)
    assert c16.in_dll(game, "g_acting_champion_ordinal").value == 1
    assert (ctypes.c_uint8 * 3).in_dll(game, "g_action_list_indices")[0] != 0xFF
    _, rows = rgb_rows(game)
    if SCRATCH.exists():
        png(SCRATCH / "ui_action_menu.png", rows)


@needs_game
def test_render_inventory(game):
    reset(game)
    recruit(game, 2)
    render(game, "base")
    game.inventory_draw_open(0)
    fb, rows = rgb_rows(game)
    if SCRATCH.exists():
        png(SCRATCH / "ui_inventory.png", rows)
    assert c16.in_dll(game, "g_inventory_champion_ordinal").value == 1
    assert len(set(fb[320 * 33:320 * 169])) > 6
    game.draw_champion_skills_and_statistics()
    game.ui_draw_viewport()
    _, rows = rgb_rows(game)
    if SCRATCH.exists():
        png(SCRATCH / "ui_inventory_eye.png", rows)
    game.inventory_draw_close(0)
    assert c16.in_dll(game, "g_inventory_champion_ordinal").value == 0
