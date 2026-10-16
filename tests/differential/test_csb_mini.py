"""Differential test: dmtools.csb_mini against the original CEDT.FTL routines in dmtools.emu."""

import functools
import struct

import pytest

from dmtest.cedt import CEDT, SLOTS, Cedt, l, same
from dmtest.markers import MINI, needs_csb_mini, needs_file
from dmtools import csb_mini as cm

pytestmark = [needs_file(CEDT), needs_csb_mini]

SEED = 0x2F0020  # value for the random seed at A5-0x1520


@functools.cache
def mini():
    return MINI.read_bytes()


def synthetic_game():
    """A save with dungeon ID 12 (the prison) and the MINI.DAT champion. No real prison save exists here."""
    s = cm.decode_save(mini())
    cm.put_w(s["header"], s["fields"]["dungeon_id"], 12)
    struct.pack_into(">I", s["header"], s["fields"]["game_id"], 0x12345678)
    cm.put_w(s["blocks"][0], 16, 0)  # party direction north: the merge turns the champion
    b2 = s["blocks"][2]
    b2[0x1C], b2[0x1D] = 0, 1
    b2[0x140:0x280] = b2[0:0x140]  # second champion, same cell as the first
    b2[0x140:0x148] = b"ZED\0XYZ\0"  # bytes after the NUL: 0x109c20 clears them
    cm.put_w(s["blocks"][0], 10, 2)
    cm.put_w(b2, 0x36, 500)  # health below max
    return cm.encode_save(s, cm.make_rng(7))


@functools.cache
def merged():
    """Run the original 0x105c7e on two loaded slots and csb_mini.merge on the same saves."""
    game_raw = synthetic_game()
    c = Cedt()
    c.load(0, game_raw)
    c.load(1, mini())
    e = c.e
    e.write(c.a5 - 0x4032, struct.pack(">H", c.rw(SLOTS + 0x230)))
    e.write(c.a5 - 0x1520, l(SEED))
    assert c.call(0x10A31C, (4, SLOTS), (2, 3)) == 1, "0x10a31c accepts the game"
    c.call(0x105C7E)
    slot1 = SLOTS + 0x3A6
    orig = dict(header=bytes(e.read(slot1 + 0x26, 0x200)), champions=bytes(e.read(c.rl(slot1 + 0x30C), 0x500)),
                globals=bytes(e.read(slot1 + 0x226, 0x80)))

    state = [SEED]

    def rng():  # 0x1064be with the same seed
        state[0] = (state[0] * 0x4CB2F + 11) & 0xFFFFFFFF
        return (state[0] >> 8) & 0xFFFF

    game = cm.decode_save(game_raw)
    assert cm.can_make_adventure(game)
    return cm.merge(game, cm.decode_save(mini()), rng), orig


SAVES = {
    "MINI.DAT": mini,
    "synthetic game (encode_save)": synthetic_game,
    "new CSBGAME.DAT (encode_save)": lambda: cm.encode_save(merged()[0], cm.make_rng(3)),
}
PARTS = ["header", *(f"block {i}" for i in range(5)), "dungeon"]


@functools.cache
def decoded(label):
    raw = SAVES[label]()
    return cm.decode_save(raw), Cedt().load(0, raw)


def part(save, name):
    return save["blocks"][int(name.split()[1])] if name.startswith("block") else save[name]


@pytest.mark.parametrize("name", PARTS)
@pytest.mark.parametrize("label", SAVES)
def test_decode_matches_original(label, name):
    py, orig = decoded(label)
    same(part(py, name), part(orig, name), f"{label}: {name}")


def changed_byte():
    bad = bytearray(mini())
    bad[0xBC0 + 7] ^= 0x01  # inside block 3
    return bytes(bad)


def test_original_rejects_changed_byte():
    with pytest.raises(AssertionError, match="block 3"):
        Cedt().load(0, changed_byte())


def test_csb_mini_rejects_changed_byte():
    with pytest.raises(ValueError):
        cm.decode_save(changed_byte())


def test_merge_header():
    py, orig = merged()
    same(py["header"], orig["header"], "merge: header")


def test_merge_champions():
    """The portrait pointer at 0x114 of each champion is masked."""
    py, orig = merged()

    def mask(b):
        return b"".join(b[c * 0x140:c * 0x140 + 0x114] + bytes(4) + b[c * 0x140 + 0x118:(c + 1) * 0x140] for c in range(4))

    same(mask(bytes(py["blocks"][2][:0x500])), mask(orig["champions"]), "merge: champions")


def test_merge_globals():
    py, orig = merged()
    same(py["blocks"][0], orig["globals"], "merge: globals")
