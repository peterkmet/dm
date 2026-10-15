"""CSB rule differences (docs/csb-engine-diff.md "Actions", "Sensors and timeline", "Champions"): each rule in both
adventures. DM keeps its behaviour, CSB has the new one."""

import ctypes
import shutil
import subprocess
from pathlib import Path

import pytest

from conftest import PORT_INCLUDES, game_sources
from test_champion import Champion
from test_creatures import THING_EXPLOSION, TimelineEvent
from test_gameloop import Game

ROOT = Path(__file__).parent.parent
DIRS = {"dm": ROOT / "game", "csb": ROOT / "game-csb"}
ADVENTURE = {"dm": 0, "csb": 1}
STEP_EAST, STEP_NORTH = (0, 1, 0, -1), (-1, 0, 1, 0)
CORRIDOR, THING_SENSOR, THING_ENDOFLIST = 1, 3, 0xFFFE
ACTION_NONE_IN_BOTH, ACTION_INVOKE, ACTION_FLUXCAGE, ACTION_FUSE = 3, 27, 35, 43
EVENT_ENABLE_ACTION, EVENT_REMOVE_FLUXCAGE = 11, 24
EFFECT_SET, EFFECT_CLEAR, EFFECT_TOGGLE = 0, 1, 2
SENSOR_FLOOR_VERSION_CHECKER = 9
PANEL_REINCARNATE = 0xA1
STAT_STRENGTH, STAT_COUNT, STAT_MAXIMUM, STAT_CURRENT, STAT_MINIMUM = 1, 7, 0, 1, 2
SEED = 0x12345
c16, u16, u32 = ctypes.c_int16, ctypes.c_uint16, ctypes.c_uint32

class ActionInfo(ctypes.Structure):
    _fields_ = [("disabled_ticks", ctypes.c_uint8), ("damage_factor", ctypes.c_uint8), ("hit_probability", ctypes.c_uint8),
                ("stamina", ctypes.c_uint8), ("defense", ctypes.c_int8), ("skill_index", ctypes.c_uint8),
                ("experience_gain", ctypes.c_uint8)]


pytestmark = pytest.mark.skipif(not shutil.which("clang"), reason="clang not installed")


@pytest.fixture(scope="module")
def lib(tmp_path_factory):
    """A private build of libdm."""
    out = tmp_path_factory.mktemp("lib") / "libdm.dylib"
    sources = game_sources()
    subprocess.run(["clang", "-std=c11", "-Wall", "-Wextra", "-Werror", "-O1", "-dynamiclib", *PORT_INCLUDES, "-o", str(out), *sources], check=True)
    lib = ctypes.CDLL(str(out))
    lib.gameloop_load_data.restype = ctypes.c_bool
    lib.gameloop_new_game.restype = ctypes.c_bool
    for name, res, args in [
        ("dungeon_get_thing_data", ctypes.POINTER(ctypes.c_uint8), [u16]), ("get_square_first_thing", u16, [c16, c16]),
        ("get_next_thing", u16, [u16]), ("get_square", ctypes.c_uint8, [c16, c16]), ("kaos_s16", c16, [ctypes.c_uint32]),
        ("action_is_action_performed", ctypes.c_bool, [u16, c16]), ("random_bit", u16, []), ("random16", u16, []),
        ("sensor_add_event", None, [c16, c16, c16, c16, c16, u32]),
        ("sensor_process_thing_addition_or_removal", None, [c16, c16, u16, ctypes.c_bool, ctypes.c_bool]),
        ("champion_click_in_resurrect_reincarnate_panel", None, [c16]), ("unlink_thing_from_list", None, [u16, u16, c16, c16]),
        ("tables_action_info", ctypes.POINTER(ActionInfo), [ctypes.c_int]),
        ("champion_add_skill_experience", None, [u16, u16, u16]),
        ("link_thing_to_list", None, [u16, u16, c16, c16]),
    ]:
        f = getattr(lib, name)
        f.restype, f.argtypes = res, args
    yield lib
    c16.in_dll(lib, "g_adventure").value = ADVENTURE["dm"]


@pytest.fixture(params=["dm", "csb"])
def world(request, lib):
    """A new game of the adventure: the DM entrance or the CSB prison."""
    folder = DIRS[request.param]
    if not all((folder / f).exists() for f in ("Graphics.DAT", "Dungeon.DAT", "KAOS.FTL" if request.param == "dm" else "APPB.FTL")):
        pytest.skip(f"{folder.name} files missing")
    c16.in_dll(lib, "g_adventure").value = ADVENTURE[request.param]
    assert lib.gameloop_load_data(str(folder).encode())
    assert lib.gameloop_new_game(str(folder).encode())
    game = Game(lib)
    game.run(26)
    return request.param, game


def value(lib, name, kind=c16):
    return kind.in_dll(lib, name)


def things(lib, x, y):
    thing = lib.get_square_first_thing(x, y)
    while thing != THING_ENDOFLIST:
        yield thing
        thing = lib.get_next_thing(thing)


def timeline(lib):
    events = ctypes.cast(ctypes.c_void_p.in_dll(lib, "g_timeline_events"), ctypes.POINTER(TimelineEvent))
    heap = ctypes.cast(ctypes.c_void_p.in_dll(lib, "g_timeline"), ctypes.POINTER(u16))
    return [events[heap[i]] for i in range(value(lib, "g_timeline_count", u16).value)]


def face_empty_corridor(lib):
    """Puts the party on an empty corridor square in front of another empty corridor square. Returns the front square."""
    width, height = value(lib, "g_current_map_width").value, value(lib, "g_current_map_height").value
    for x in range(width):
        for y in range(height):
            for d in range(4):
                fx, fy = x + STEP_EAST[d], y + STEP_NORTH[d]
                if not (0 <= fx < width and 0 <= fy < height):
                    continue
                if all(lib.get_square(a, b) >> 5 == CORRIDOR and not list(things(lib, a, b)) for a, b in ((x, y), (fx, fy))):
                    for name, v in zip(("g_party_map_x", "g_party_map_y", "g_party_direction"), (x, y, d)):
                        value(lib, name).value = v
                    return fx, fy
    raise AssertionError("no empty corridor")


def put_champion(lib):
    """Champion 0 alive, with empty hands, facing the party direction."""
    c = (Champion * 4).in_dll(lib, "g_champions")[0]
    c.current_health = c.maximum_health = 100
    c.current_stamina = c.maximum_stamina = 1000
    c.current_mana = c.maximum_mana = 0
    c.enable_action_event_index = c.hide_damage_received_event_index = -1
    c.direction = value(lib, "g_party_direction").value
    for s in range(30):
        c.slots[s] = 0xFFFF
    for skill in c.skills:
        skill.experience = 1 << 20  # high experience: the action gives no level and draws no random number
    value(lib, "g_party_champion_count", u16).value = 1
    value(lib, "g_leader_index").value = 0
    return c


def seed_after_random_bit(lib):
    value(lib, "g_random_seed", u32).value = SEED
    lib.random_bit()
    return value(lib, "g_random_seed", u32).value


@pytest.mark.parametrize("action", [ACTION_INVOKE, ACTION_FLUXCAGE, ACTION_FUSE])
def test_removed_actions(world, action):
    """CSB: invoke, fluxcage, and fuse only disable the hand, take stamina, give experience; no explosion, no fluxcage
    event, and only the stamina random bit. DM: invoke draws more random numbers; fluxcage and fuse make explosions."""
    adventure, game = world
    lib = game.lib
    front = face_empty_corridor(lib)
    c = put_champion(lib)
    skills = [(s.temporary_experience, s.experience) for s in c.skills]
    fluxcages = sum(e.type == EVENT_REMOVE_FLUXCAGE for e in timeline(lib))
    value(lib, "g_random_seed", u32).value = SEED
    assert lib.action_is_action_performed(0, action) == 1
    seed = value(lib, "g_random_seed", u32).value
    explosions = [t for t in things(lib, *front) if t >> 10 & 15 == THING_EXPLOSION]
    new_fluxcages = sum(e.type == EVENT_REMOVE_FLUXCAGE for e in timeline(lib)) - fluxcages
    if adventure == "csb":
        assert seed == seed_after_random_bit(lib)
        assert not explosions and not new_fluxcages
        info = lib.tables_action_info(action).contents
        value(lib, "g_random_seed", u32).value = SEED
        assert c.current_stamina == 1000 - (info.stamina + lib.random_bit())
        events = ctypes.cast(ctypes.c_void_p.in_dll(lib, "g_timeline_events"), ctypes.POINTER(TimelineEvent))
        enable = events[c.enable_action_event_index]
        assert enable.type == EVENT_ENABLE_ACTION
        assert (enable.map_time & 0xFFFFFF) - value(lib, "g_game_time", u32).value == info.disabled_ticks
        gained = [(s.temporary_experience, s.experience) for s in c.skills]
        for skill, (temporary, total) in zip(c.skills, skills):
            skill.temporary_experience, skill.experience = temporary, total
        lib.champion_add_skill_experience(0, info.skill_index, info.experience_gain)  # the full experience
        assert gained == [(s.temporary_experience, s.experience) for s in c.skills]
    elif action == ACTION_INVOKE:
        assert seed != seed_after_random_bit(lib)
    else:
        assert explosions
        assert new_fluxcages == (action == ACTION_FLUXCAGE)


def test_action_without_branch_draws_only_the_stamina_bit(world):
    """The reference of the CSB check above: an action without own code draws only the stamina random bit."""
    _, game = world
    lib = game.lib
    face_empty_corridor(lib)
    put_champion(lib)
    value(lib, "g_random_seed", u32).value = SEED
    assert lib.action_is_action_performed(0, ACTION_NONE_IN_BOTH) == 1
    assert value(lib, "g_random_seed", u32).value == seed_after_random_bit(lib)


def test_sensor_event_priority(world):
    """DM: set 1, clear 3, toggle 2. CSB: 0 for each effect."""
    adventure, game = world
    lib = game.lib
    priorities = {}
    for effect, time in ((EFFECT_SET, 900001), (EFFECT_CLEAR, 900002), (EFFECT_TOGGLE, 900003)):
        lib.sensor_add_event(1, 1, 1, 0, effect, time)
        priorities[effect] = next(e.priority for e in timeline(lib) if e.map_time & 0xFFFFFF == time)
    assert priorities == ({EFFECT_SET: 0, EFFECT_CLEAR: 0, EFFECT_TOGGLE: 0} if adventure == "csb"
                          else {EFFECT_SET: 1, EFFECT_CLEAR: 3, EFFECT_TOGGLE: 2})


def version_checker(lib, data):
    """A sensor of the map moves to an empty corridor square and becomes a version checker: every time, effect set,
    silent, an event to its own square. The dungeon has no free sensor record."""
    sensor, sx, sy = next((t, sx, sy) for sx in range(value(lib, "g_current_map_width").value)
                          for sy in range(value(lib, "g_current_map_height").value)
                          for t in things(lib, sx, sy) if t >> 10 & 15 == THING_SENSOR)
    lib.unlink_thing_from_list(sensor, 0, sx, sy)
    x, y = face_empty_corridor(lib)
    record = lib.dungeon_get_thing_data(sensor)
    word = data << 7 | SENSOR_FLOOR_VERSION_CHECKER
    target = y << 11 | x << 6
    record[2], record[3], record[4], record[5], record[6], record[7] = word >> 8, word & 0xFF, 0, 0, target >> 8, target & 0xFF
    lib.link_thing_to_list(sensor, 0, x, y)
    return x, y


@pytest.mark.parametrize("data", [31, 32, 36, 37])
def test_version_checker_limit(world, data):
    """The version checker floor sensor triggers for data <= 36 in DM and <= 31 in CSB."""
    adventure, game = world
    lib = game.lib
    x, y = version_checker(lib, data)
    value(lib, "g_party_champion_count", u16).value = 1
    count = value(lib, "g_timeline_count", u16).value
    lib.sensor_process_thing_addition_or_removal(x, y, 0xFFFF, 0, 1)
    triggered = value(lib, "g_timeline_count", u16).value - count
    assert triggered == (data <= (31 if adventure == "csb" else 36))


def test_reincarnation(world):
    """DM: 12 random statistic points. CSB: first each statistic but luck loses 1/8 (not below its minimum), and
    health, stamina, and mana drop to 1/4."""
    adventure, game = world
    lib = game.lib
    path, direction = game.path_to_mirror()
    game.walk(path)
    game.face(direction)
    game.run()
    game.click_box(game.boxes[5])
    assert value(lib, "g_candidate_champion_ordinal").value == 1
    c = game.champions[0]
    stats = [list(c.statistics[s]) for s in range(STAT_COUNT)]
    bars = [c.current_health, c.maximum_health, c.current_stamina, c.maximum_stamina, c.current_mana, c.maximum_mana]
    if adventure == "csb":
        for s in range(STAT_STRENGTH, STAT_COUNT):
            v = stats[s][STAT_CURRENT]
            stats[s][STAT_CURRENT] = stats[s][STAT_MAXIMUM] = max(stats[s][STAT_MINIMUM], v - v // 8)
        bars = [b >> 2 for b in bars]
    value(lib, "g_random_seed", u32).value = SEED
    lib.champion_click_in_resurrect_reincarnate_panel(PANEL_REINCARNATE)
    seed = value(lib, "g_random_seed", u32).value
    value(lib, "g_random_seed", u32).value = SEED
    for _ in range(12):
        s = lib.random16() % STAT_COUNT
        stats[s][STAT_CURRENT] += 1
        stats[s][STAT_MAXIMUM] += 1
    assert value(lib, "g_random_seed", u32).value == seed
    assert [list(c.statistics[s]) for s in range(STAT_COUNT)] == stats
    assert [c.current_health, c.maximum_health, c.current_stamina, c.maximum_stamina, c.current_mana, c.maximum_mana] == bars
    assert all(s.experience == 0 for s in c.skills)
