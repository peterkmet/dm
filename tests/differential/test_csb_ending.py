# SPDX-License-Identifier: GPL-3.0-or-later
"""The CSB end of the game (docs/csb-engine-diff.md "End of the game", docs/csb-data.md "ENDA.DAT and the endgame"):
the ENDA.DAT player, wall sensor 18, and the death screen with RESTART THIS GAME only."""

import ctypes
import shutil
import struct

import pytest

from dmtest import constants as C
from dmtest.game import TimelineEvent, global_type
from dmtest.lib import clear_folder_hooks, load_lib, set_folder_hooks
from dmtest.markers import GAME_CSB, ROOT, needs_csb, needs_csb_mini, needs_file

ENDA = GAME_CSB / "ENDA.DAT"
MINI = GAME_CSB / "disk-b" / "MINI.DAT"
DM, CSB = 0, 1
PARTY_KILLING_DAMAGE = 10000
ENDGAME_TABLE = 0x1031D4
LEFT_PRESS, LEFT_RELEASE = 2, 4
NO_CHOICE = 99
DEATH_FRAMES, THE_END_FRAMES = 240, 300
c16, u16, u32 = ctypes.c_int16, ctypes.c_uint16, ctypes.c_uint32
HOOK = ctypes.CFUNCTYPE(None)

pytestmark = [needs_csb, needs_csb_mini, needs_file(ENDA)]


@pytest.fixture(scope="module")
def lib():
    """A private copy of libdm."""
    lib = load_lib()
    lib.gameloop_load_data.restype = ctypes.c_bool
    for name, res, args in [
        ("anim_open", ctypes.c_int, [ctypes.c_char_p]), ("anim_next_frame", c16, []), ("anim_close", None, []),
        ("dungeon_get_thing_data", ctypes.POINTER(ctypes.c_uint8), [u16]), ("map_get_square_first_thing", u16, [c16, c16]),
        ("map_get_next_thing", u16, [u16]), ("map_set_current_map", None, [c16]), ("dungeon_map_count", c16, []),
        ("savegame_load_from", c16, [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_void_p]), ("savegame_save", c16, [ctypes.c_char_p]),
        ("event_process_wall", None, [ctypes.POINTER(TimelineEvent)]), ("kaos_s16", c16, [u32]),
        ("startend_run_endgame", None, [c16]), ("startend_set_save_dir", None, [ctypes.c_char_p]),
        ("command_process_click", None, [c16, c16, c16]), ("command_process_key", None, [u16]),
        ("anim_play", ctypes.c_int, [ctypes.c_char_p]), ("startend_run_endgame_sensor", None, [u16]),
        ("sound_mix", None, [ctypes.POINTER(c16), ctypes.c_int]), ("sound_stop_all", None, []),
        ("timeline_add_event", c16, [ctypes.POINTER(TimelineEvent)]), ("gameloop_tick", None, []),
    ]:
        f = getattr(lib, name)
        f.restype, f.argtypes = res, args
    hooks = set_folder_hooks(lib)
    yield lib
    lib.anim_close()
    clear_folder_hooks(lib)
    del hooks
    c16.in_dll(lib, "g_adventure").value = DM


def value(lib, name, kind=c16):
    return global_type(name, kind).in_dll(lib, name)


def screen(lib):
    return bytes((ctypes.c_uint8 * (C.SCREEN_W * C.SCREEN_H)).in_dll(lib, "fb"))


def palette(lib):
    return list((u16 * 16).in_dll(lib, "palette"))


def chunks(data):
    """(offset, tag, length, parameter) of each chunk: tag 2 bytes, payload length 2, parameter 2, payload."""
    result, at = [], 0
    while at + 6 <= len(data):
        tag, length, parameter = data[at : at + 2].decode("latin-1"), *struct.unpack(">HH", data[at + 2 : at + 6])
        result.append((at, tag, length, parameter))
        at += 6 + length
    return result, at


def expand_over(packed, pixels):
    """An independent decoder of the Graphics.DAT packed format over a 320 x 200 screen: skips keep pixels."""
    i = 0
    while packed[i : i + 2] == b"\xff\x81":
        i += 2
    w, h = struct.unpack_from(">HH", packed, i)
    assert (w, h) == (C.SCREEN_W, C.SCREEN_H)
    i, n, pos = i + 4, w * h, 0

    def fill(count, color):
        nonlocal pos
        end = min(pos + count, n)
        pixels[pos:end] = bytes([color]) * (end - pos)
        pos += count

    while pos < n:
        cmd = packed[i]
        i += 1
        color = cmd & 15
        if not cmd & 0x80:
            fill((cmd >> 4) + 1, color)
            continue
        if cmd & 0x30 == 0x20:
            k = color | (16 if cmd & 0x40 else 0)
            if k == 29:
                k, i = packed[i], i + 1
            elif k == 30:
                k, i = 256 + packed[i], i + 1
            elif k == 31:
                k, i = packed[i] << 8 | packed[i + 1], i + 2
            pos += k + 1
            continue
        count, i = packed[i], i + 1
        if cmd & 0x40:
            count, i = count << 8 | packed[i], i + 1
        count += 1
        kind = cmd & 0x30
        if kind == 0:
            fill(count, color)
        elif kind == 0x10:
            if count & 1:
                fill(1, color)
            for b in packed[i : i + count // 2]:
                fill(1, b >> 4)
                fill(1, b & 15)
            i += count // 2
        else:
            for _ in range(count):
                if pos < n:
                    pixels[pos] = pixels[pos - w] if pos >= w else 0
                pos += 1
            fill(1, color)


def play_model(data):
    """The frames of ANIM.FTL as (delay, screen, palette): FO n runs the chunk after it once and the body n times;
    NE ends a pass, BN goes back; the last NE skips the two chunks after it. A PL applies with the next frame."""
    items, _ = chunks(data)
    frames, loops, i = [], [], 0
    pixels, colors = bytearray(C.SCREEN_W * C.SCREEN_H), [0] * 16
    while i < len(items):
        at, tag, length, parameter = items[i]
        i += 1
        if tag in ("EN", "DL"):
            expand_over(data[at + 6 : at + 6 + length], pixels)
            frames.append((parameter, bytes(pixels), list(colors)))
        elif tag == "PL":
            for index, color in palette_entries(data, at).items():
                colors[index] = color
        elif tag == "FO":
            loops.append([i + 1, parameter - 1])
        elif tag == "NE":
            loops[-1][1] -= 1
            if loops[-1][1] < 0:
                loops.pop()
                i += 2
        elif tag == "BN":
            i = loops[-1][0]
    return frames


@pytest.fixture(scope="module")
def model():
    return play_model(ENDA.read_bytes())


def palette_entries(data, offset):
    """The colors of a PL chunk: {index: Amiga color}."""
    count = struct.unpack_from(">H", data, offset + 6)[0]
    entries = [data[offset + 8 + 4 * i : offset + 12 + 4 * i] for i in range(count)]
    return {e[0]: e[1] << 8 | e[2] << 4 | e[3] for e in entries}


class Frames:
    """A frame hook that records the screen and the palette of each frame and runs an action."""

    def __init__(self, lib, action=None, limit=10000):
        self.lib, self.action, self.limit, self.shots = lib, action, limit, []
        self.hook = HOOK(self.frame)
        ctypes.c_void_p.in_dll(lib, "g_frame_hook").value = ctypes.cast(self.hook, ctypes.c_void_p).value

    def frame(self):
        self.shots.append((screen(self.lib), palette(self.lib)))
        if self.action:
            self.action(len(self.shots))
        if len(self.shots) > self.limit:
            value(self.lib, "g_quit_requested").value = 1

    def remove(self):
        ctypes.c_void_p.in_dll(self.lib, "g_frame_hook").value = None


@pytest.fixture
def reset(lib):
    yield
    for name in ("g_game_won", "g_return_to_entrance", "g_quit_requested"):
        value(lib, name).value = 0
    ctypes.c_void_p.in_dll(lib, "g_frame_hook").value = None


def test_chunk_walk_consumes_the_file(lib, tmp_path):
    data = ENDA.read_bytes()
    items, end = chunks(data)
    assert end == len(data)
    tags = [tag for _, tag, _, _ in items]
    assert tags[:2] == ["BR", "AN"] and tags[-1] == "DO"
    assert {t: tags.count(t) for t in ("SD", "PL", "EN", "DL", "SO", "FO", "NE", "BN")} == {
        "SD": 2, "PL": 3, "EN": 2, "DL": 285, "SO": 3, "FO": 1, "NE": 1, "BN": 1}
    assert lib.anim_open(str(ENDA).encode()) == 0
    (tmp_path / "SHORT.DAT").write_bytes(data[:-2])
    assert lib.anim_open(str(tmp_path / "SHORT.DAT").encode()) == -1
    assert lib.anim_next_frame() == C.ANIM_END


def test_first_frame_decodes(lib):
    data = ENDA.read_bytes()
    items, _ = chunks(data)
    first_palette = next(at for at, tag, _, _ in items if tag == "PL")
    frame = next(at for at, tag, _, _ in items if tag == "EN")
    assert struct.unpack_from(">HH", data, frame + 6) == (C.SCREEN_W, C.SCREEN_H)
    assert lib.anim_open(str(ENDA).encode()) == 0
    assert lib.anim_next_frame() == items[[at for at, *_ in items].index(frame)][3]
    pixels = screen(lib)
    assert len(pixels) == C.SCREEN_W * C.SCREEN_H and len(set(pixels)) > 4 and pixels.count(0) < len(pixels) // 2
    colors = palette(lib)
    assert all(colors[i] == c for i, c in palette_entries(data, first_palette).items())
    lib.anim_close()


def test_frame_sequence_and_timing(lib, model):
    expected = [delay for delay, _, _ in model]
    assert lib.anim_open(str(ENDA).encode()) == 0
    delays = []
    while (delay := lib.anim_next_frame()) != C.ANIM_END:
        delays.append(delay)
    assert delays == expected
    assert delays[:3] == [80, 8, 8]
    assert len(delays) == 287 - 19 - 1 + 8 * 19 + 7  # the loop body of 19 frames runs 8 times, the end-of-pass frame 7 times
    lib.anim_close()


def test_every_frame_matches_an_independent_decode(lib, model):
    """Delta frames keep the pixels of the frame before; each palette applies with its frame."""
    assert lib.anim_open(str(ENDA).encode()) == 0
    for k, (delay, pixels, colors) in enumerate(model):
        assert lib.anim_next_frame() == delay
        assert screen(lib) == pixels, k
        assert palette(lib) == colors, k
    assert lib.anim_next_frame() == C.ANIM_END
    lib.anim_close()


def test_palette_changes_with_the_second_full_frame(lib, model):
    data = ENDA.read_bytes()
    items, _ = chunks(data)
    first_pl, second_pl = [at for at, tag, _, _ in items if tag == "PL"][:2]
    second_en = [i for i, (_, tag, _, _) in enumerate(items) if tag == "EN"][1]
    assert items[second_en - 1][0] == second_pl
    frames_before = sum(1 for _, tag, _, _ in items[:second_en] if tag in ("EN", "DL"))
    assert lib.anim_open(str(ENDA).encode()) == 0
    for _ in range(frames_before):
        lib.anim_next_frame()
    assert all(palette(lib)[i] == c for i, c in palette_entries(data, first_pl).items())
    lib.anim_next_frame()
    assert all(palette(lib)[i] == c for i, c in palette_entries(data, second_pl).items())
    assert palette(lib) != model[frames_before - 1][2]
    lib.anim_close()


def test_anim_play_holds_each_frame_for_its_delay(lib, model, reset):
    frames = Frames(lib)
    assert lib.anim_play(str(ENDA).encode()) == 0
    frames.remove()
    expected = [(pixels, colors) for delay, pixels, colors in model for _ in range(delay)]
    assert len(frames.shots) == len(expected) == sum(delay for delay, _, _ in model)
    assert frames.shots == expected


TITL = ROOT / "game" / "TITL.DAT"


@needs_file(TITL)
def test_dm_title_plays_and_stops_on_input(lib, reset):
    """TITL.DAT: 53 frames of one vertical blank; its BR chunk lets a key or a click stop it."""
    frames = Frames(lib)
    assert lib.anim_play(str(TITL).encode()) == 0
    assert len(frames.shots) == 53
    assert len(set(frames.shots[-1][0])) > 4  # the last frame stays on the screen
    press = lambda n: n == 5 and setattr(value(lib, "g_input_received"), "value", 1)
    frames = Frames(lib, press)
    assert lib.anim_play(str(TITL).encode()) == 0
    frames.remove()
    assert len(frames.shots) == 5


def test_enda_ignores_input(lib, model, reset):
    press = lambda n: n == 5 and setattr(value(lib, "g_input_received"), "value", 1)
    frames = Frames(lib, press)
    assert lib.anim_play(str(ENDA).encode()) == 0
    frames.remove()
    assert len(frames.shots) == sum(delay for delay, _, _ in model)

def test_play_sample_volumes_and_period(lib):
    data = ENDA.read_bytes()
    items, _ = chunks(data)
    samples = [at for at, tag, _, _ in items if tag == "SD"]
    so = next(i for i, (_, tag, _, _) in enumerate(items) if tag == "SO")
    number, volume_bytes = items[so][3], data[items[so][0] + 6 : items[so][0] + 8]
    assert number == 2 and volume_bytes == b"DL"
    left, right = ((b + 1) >> 2 for b in volume_bytes)
    assert (left, right) == (17, 19)
    sd = samples[number - 1]
    length = struct.unpack_from(">H", data, sd + 6)[0]
    sample = struct.unpack_from(f"{length}b", data, sd + 8)
    frames_before = sum(1 for _, tag, _, _ in items[:so] if tag in ("EN", "DL"))
    assert lib.anim_open(str(ENDA).encode()) == 0
    for _ in range(frames_before + 1):
        lib.anim_next_frame()
    count = 3000
    out = (c16 * (2 * count))()
    lib.sound_mix(out, count)
    lib.anim_close()
    period = 650
    position = [f * 3546895 // (period * 44100) for f in range(count)]
    assert list(out[0::2]) == [sample[p] * left * 2 for p in position]
    assert list(out[1::2]) == [sample[p] * right * 2 for p in position]


def chunk(tag, parameter, payload=b""):
    return tag.encode() + struct.pack(">HH", len(payload), parameter) + payload


EMPTY_DELTA = struct.pack(">HH", C.SCREEN_W, C.SCREEN_H) + bytes([0xEF, 0xF9, 0xFF, 0])  # skips all 64000 pixels


def test_nested_loops(lib, tmp_path):
    """The last NE of the inner loop skips its end-of-pass chunk and BN, so the outer loop goes on."""
    frame = lambda delay: chunk("DL", delay, EMPTY_DELTA)  # noqa: E731
    data = b"".join([chunk("FO", 2), frame(1), chunk("FO", 2), frame(2), frame(3), chunk("NE", 0), frame(4), chunk("BN", 0),
                     frame(5), chunk("NE", 0), frame(6), chunk("BN", 0), frame(7)])
    (tmp_path / "LOOP.DAT").write_bytes(data)
    expected = [1, 2, 3, 4, 3, 5, 6, 2, 3, 4, 3, 5, 7]
    assert [delay for delay, _, _ in play_model(data)] == expected
    assert lib.anim_open(str(tmp_path / "LOOP.DAT").encode()) == 0
    delays = []
    while len(delays) <= len(expected) and (delay := lib.anim_next_frame()) != C.ANIM_END:
        delays.append(delay)
    lib.anim_close()
    assert delays == expected


def test_palette_waits_for_the_next_frame(lib, tmp_path):
    full = struct.pack(">HH", C.SCREEN_W, C.SCREEN_H) + bytes([0xC0, 0xF9, 0xFF])  # 64000 pixels of color 0
    white = struct.pack(">H", 1) + bytes([0, 15, 15, 15])
    (tmp_path / "PAL.DAT").write_bytes(chunk("PL", 0, white) + chunk("EN", 1, full + b"\0") + chunk("PL", 0, struct.pack(">H", 1) + bytes(4)))
    assert lib.anim_open(str(tmp_path / "PAL.DAT").encode()) == 0
    assert lib.anim_next_frame() == 1 and palette(lib)[0] == 0xFFF
    assert lib.anim_next_frame() == C.ANIM_END and palette(lib)[0] == 0xFFF  # no frame shows the last palette
    lib.anim_close()


def load_mini(lib, tmp_path):
    c16.in_dll(lib, "g_adventure").value = CSB
    assert lib.gameloop_load_data(str(GAME_CSB).encode())
    shutil.copy(MINI, tmp_path / "MINI.DAT")
    assert lib.savegame_load_from(str(tmp_path).encode(), b"MINI.DAT", None) == 1
    lib.gameloop_start_loaded_game()


def endgame_sensor_square(lib):
    """Map, x, y, and sensor of the wall sensor 18 of the CSB dungeon."""
    for m in range(lib.dungeon_map_count()):
        lib.map_set_current_map(m)
        for x in range(value(lib, "g_current_map_width").value):
            for y in range(value(lib, "g_current_map_height").value):
                t = lib.map_get_square_first_thing(x, y)
                while t != C.THING_ENDOFLIST:
                    rec = lib.dungeon_get_thing_data(t)
                    if t >> 10 & 15 == C.THING_SENSOR and (rec[2] << 8 | rec[3]) & 0x7F == C.SENSOR_WALL_ENDGAME:
                        return m, x, y, t
                    t = lib.map_get_next_thing(t)
    raise AssertionError("no endgame sensor")


def test_sensor_18_starts_the_ending(lib, tmp_path, reset):
    load_mini(lib, tmp_path)
    m, x, y, sensor = endgame_sensor_square(lib)
    assert (m, x, y) == (0, 1, 2)
    assert lib.anim_open(str(ENDA).encode()) == 0
    lib.anim_next_frame()
    first_frame, first_palette = screen(lib), palette(lib)
    lib.anim_close()
    ctypes.memset(ctypes.addressof(ctypes.c_uint8.in_dll(lib, "fb")), 0, C.SCREEN_W * C.SCREEN_H)
    lib.map_set_current_map(m)
    event = TimelineEvent(b=x << 8 | y, c=(sensor >> 14) << 8)
    lib.event_process_wall(ctypes.byref(event))
    assert value(lib, "g_game_won").value == 1 and value(lib, "g_return_to_entrance").value == 1
    assert screen(lib) == first_frame and palette(lib) == first_palette


def add_event(lib, event_type, m, time, b, c=0):
    event = TimelineEvent(map_time=m << 24 | time, type=event_type, b=b, c=c)
    lib.timeline_add_event(ctypes.byref(event))


def add_sensor_event(lib, time):
    m, x, y, sensor = endgame_sensor_square(lib)
    add_event(lib, C.EVENT_WALL, m, time, x << 8 | y, (sensor >> 14) << 8)


def test_sensor_18_in_a_tick_starts_the_ending(lib, tmp_path, reset):
    load_mini(lib, tmp_path)
    add_sensor_event(lib, value(lib, "g_game_time", u32).value)
    lib.gameloop_tick()
    assert value(lib, "g_game_won").value == 1 and value(lib, "g_return_to_entrance").value == 1


def test_sensor_18_wins_over_a_party_death_in_the_same_tick(lib, tmp_path, reset):
    load_mini(lib, tmp_path)
    pending = (c16 * 4).in_dll(lib, "g_champion_pending_damage")
    assert value(lib, "g_party_champion_count", u16).value
    for i in range(value(lib, "g_party_champion_count", u16).value):
        pending[i] = PARTY_KILLING_DAMAGE
    add_sensor_event(lib, value(lib, "g_game_time", u32).value)
    lib.gameloop_tick()
    assert value(lib, "g_party_dead").value == 1 and value(lib, "g_game_won").value == 1
    value(lib, "g_return_to_entrance").value = 0
    load_mini(lib, tmp_path)
    lib.gameloop_tick()
    assert value(lib, "g_game_won").value == 0 and value(lib, "g_return_to_entrance").value == 0


def test_an_event_after_sensor_18_in_the_same_tick_sees_the_won_game(lib, tmp_path, reset):
    """The fluxcage stays: the removal event comes after the sensor in the same timeline run."""
    load_mini(lib, tmp_path)
    time = value(lib, "g_game_time", u32).value
    m = endgame_sensor_square(lib)[0]
    lib.map_set_current_map(m)
    thing, x, y = next((t, x, y) for x in range(value(lib, "g_current_map_width").value)
                       for y in range(value(lib, "g_current_map_height").value)
                       for t in [lib.map_get_square_first_thing(x, y)] if t != C.THING_ENDOFLIST and t >> 10 & 15 >= C.THING_WEAPON)
    add_sensor_event(lib, time - 1)
    add_event(lib, C.EVENT_REMOVE_FLUXCAGE, m, time, x << 8 | y, thing)
    lib.gameloop_tick()
    lib.map_set_current_map(m)
    assert value(lib, "g_game_won").value == 1 and lib.map_get_square_first_thing(x, y) == thing


def test_sensor_18_with_frames_plays_at_once_and_waits_for_input(lib, model, tmp_path, reset):
    load_mini(lib, tmp_path)
    value(lib, "g_saved_game_exists").value = 1
    total = sum(delay for delay, _, _ in model)
    entrance = []

    def action(count):
        entrance.append(value(lib, "g_return_to_entrance").value)
        if count == total + 10:
            click(lib, 10, 10)

    frames = Frames(lib, action)
    m, x, y, sensor = endgame_sensor_square(lib)  # the rule sets the won game, then runs the ending
    lib.map_set_current_map(m)
    lib.event_process_wall(ctypes.byref(TimelineEvent(b=x << 8 | y, c=(sensor >> 14) << 8)))
    frames.remove()
    assert frames.shots[0] == (model[0][1], model[0][2])  # no delay before the first frame
    assert len(frames.shots) == total + 10 and not any(entrance)
    assert value(lib, "g_return_to_entrance").value == 1 and value(lib, "g_game_won").value == 1
    assert value(lib, "g_saved_game_exists").value == 1


def click(lib, x, y):
    lib.command_process_click(x, y, LEFT_PRESS)
    lib.command_process_click(x, y, LEFT_RELEASE)


def test_death_shows_restart_only_and_restart_loads_the_save(lib, tmp_path, reset):
    load_mini(lib, tmp_path)
    lib.startend_set_save_dir(str(tmp_path).encode())
    assert lib.savegame_save(str(tmp_path).encode()) == 1
    assert value(lib, "g_saved_game_exists").value == 1
    saved_time = value(lib, "g_game_time", u32).value
    value(lib, "g_game_time", u32).value = saved_time + 1000
    shots, frames = {}, [0]

    def frame():
        frames[0] += 1
        if frames[0] in (DEATH_FRAMES + THE_END_FRAMES, DEATH_FRAMES + THE_END_FRAMES + 1):
            shots[frames[0]] = screen(lib)
        if frames[0] == DEATH_FRAMES + THE_END_FRAMES + 1:
            x1, x2, y1, y2 = (lib.kaos_s16(ENDGAME_TABLE + 2 + 2 * i) for i in range(4))
            click(lib, (x1 + x2) // 2, (y1 + y2) // 2)
        elif frames[0] > DEATH_FRAMES + THE_END_FRAMES + 1 and value(lib, "g_dialog_choice").value == NO_CHOICE:
            lib.command_process_key(C.KEY_RETURN)
        if frames[0] > 5000:
            value(lib, "g_quit_requested").value = 1

    hook = HOOK(frame)
    ctypes.c_void_p.in_dll(lib, "g_frame_hook").value = ctypes.cast(hook, ctypes.c_void_p).value
    lib.startend_run_endgame(1)
    ctypes.c_void_p.in_dll(lib, "g_frame_hook").value = None
    before, after = shots[DEATH_FRAMES + THE_END_FRAMES], shots[DEATH_FRAMES + THE_END_FRAMES + 1]
    bx, by, bw, bh = (lib.kaos_s16(C.CSB_RESTART_BOXES + 2 * i) for i in range(4))  # outer and inner box: x, y, width, height
    ix, iy, iw, ih = (lib.kaos_s16(C.CSB_RESTART_BOXES + 8 + 2 * i) for i in range(4))
    changed = {(i % C.SCREEN_W, i // C.SCREEN_W) for i in range(len(after)) if after[i] != before[i]}
    assert changed and all(bx <= px < bx + bw and by <= py < by + bh for px, py in changed)  # one button, no QUIT
    assert after[by * C.SCREEN_W + bx] == C.COLOR_DARK_GRAY and after[iy * C.SCREEN_W + ix] == C.COLOR_BLACK
    assert C.COLOR_CYAN in {after[py * C.SCREEN_W + px] for px in range(ix, ix + iw) for py in range(iy, iy + ih)}
    assert value(lib, "g_quit_requested").value == 0 and value(lib, "g_return_to_entrance").value == 0
    assert value(lib, "g_game_time", u32).value == saved_time


INTRO = GAME_CSB / "disk-b" / "ANIM.DAT"
NAKED = GAME_CSB / "disk-b" / "MIDI" / "NAKED.AMG"


def mix(lib, frames):
    out = (c16 * (2 * frames))()
    lib.sound_mix(out, frames)
    return list(out[0::2]), list(out[1::2])


@needs_file(INTRO)
@needs_file(NAKED)
def test_csb_intro_frames_and_music(lib):
    """Disk B ANIM.DAT: 851 frames; MI starts NAKED.AMG, the same music on both sides."""
    assert lib.anim_open(str(INTRO).encode()) == 0
    assert lib.anim_next_frame() != C.ANIM_END
    left, right = mix(lib, 4410)
    assert any(left) and left == right
    frames = 1
    while lib.anim_next_frame() != C.ANIM_END:
        frames += 1
    assert frames == 851
    lib.anim_close()
    assert not any(mix(lib, 100)[0])


@needs_file(INTRO)
@needs_file(NAKED)
def test_csb_intro_sample_files_play_beside_the_music(lib):
    assert lib.anim_open(str(INTRO).encode()) == 0
    while lib.anim_next_frame() != C.ANIM_END:
        left, right = mix(lib, 64)
        if left != right:  # an SO sample with other volumes per side on pair A
            break
    else:
        pytest.fail("no SO sample played")
    lib.anim_close()


@needs_file(INTRO)
@needs_file(NAKED)
def test_csb_intro_stops_on_input_and_fades_the_music(lib, reset):
    press = lambda n: n == 3 and setattr(value(lib, "g_input_received"), "value", 1)
    frames = Frames(lib, press)
    assert lib.anim_play(str(INTRO).encode()) == 0
    frames.remove()
    items, _ = chunks(INTRO.read_bytes())
    first_delay = next(param for _, tag, _, param in items if tag in ("EN", "DL"))
    assert first_delay > 3  # the input comes during the first frame: it stays its delay
    assert len(frames.shots) == first_delay + 29 * 3  # the music fades in 29 steps of 3 frames
