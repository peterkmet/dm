"""Differential test: csb_mini.py against the original CEDT.FTL routines in dmtools.emu.

Run: uv run python diff_test.py   (from /Users/lego/github/dm)
"""

import struct
import sys
from pathlib import Path

from dmtools.emu import Emu
from dmtools.ftl import load_image, parse

sys.path.insert(0, str(Path(__file__).parent))
import csb_mini as cm  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
MINI = (ROOT / "game-csb/disk-b/MINI.DAT").read_bytes()

CURSOR, HEAP_PTR, READ_STUB, ALLOC_STUB = 0x2F0000, 0x2F0010, 0x2F0100, 0x2F0200
FILE0, HEAP = 0x300000, 0x400000
SLOTS = 0x2E0000
SEED = 0x2F0020  # value for the random seed at A5-0x1520


def l(v):
    return struct.pack(">I", v)


# Read(fh, len, buf) for service A5+0x20a: copy len bytes from *CURSOR to buf.
READ = bytes.fromhex("202f0008" "226f000c" "2079") + l(CURSOR) + bytes.fromhex(
    "2200" "6706" "12d8" "5381" "66fa" "23c8") + l(CURSOR) + bytes.fromhex("4e75")
# alloc(size) at 0x105020: bump allocator, even sizes.
ALLOC = bytes.fromhex("2039") + l(HEAP_PTR) + bytes.fromhex("222f0004" "5281" "0281fffffffe" "d3b9") + l(
    HEAP_PTR) + bytes.fromhex("4e75")


class Cedt:
    def __init__(self):
        img = load_image(parse((ROOT / "game-csb/disk-b/CEDT.FTL").read_bytes()), 0x100000)
        self.e = Emu(img, max_cycles=200_000_000)
        self.a5 = img.data_base
        e = self.e
        e.write(READ_STUB, READ)
        e.write(ALLOC_STUB, ALLOC)
        e.write(self.a5 + 0x20A, struct.pack(">HI", 0x4EF9, READ_STUB))
        e.write(0x105020, struct.pack(">HI", 0x4EF9, ALLOC_STUB))
        e.write(0x105036, bytes.fromhex("4e75"))  # free(): no-op
        e.write(HEAP_PTR, l(HEAP))
        e.write(self.a5 - 0x4030, l(SLOTS))  # slot array pointer
        e.write(SLOTS, bytes(2 * 0x3A6))
        self.next_file = FILE0

    def rl(self, a):
        return struct.unpack(">I", self.e.read(a, 4))[0]

    def rw(self, a):
        return struct.unpack(">H", self.e.read(a, 2))[0]

    def call(self, addr, *args):
        return self.e.call(addr, args) & 0xFFFF

    def alloc(self, n):
        p = self.rl(HEAP_PTR)
        self.e.write(HEAP_PTR, l(p + ((n + 1) & ~1)))
        return p

    def load(self, slot_index, raw):
        """The load sequence of 0x108030 with the original leaf routines. Returns plain parts."""
        e, slot = self.e, SLOTS + slot_index * 0x3A6
        e.write(self.next_file, raw)
        e.write(CURSOR, l(self.next_file))
        self.next_file += (len(raw) + 0xFFFF) & ~0xFFFF
        hdr = slot + 0x26
        assert self.call(0x108D78, (4, hdr)) == 1
        e.write(slot + 0x22, struct.pack(">HH", 2, 0x1D))
        assert self.call(0x108DA2, (4, hdr), (2, 0x1D)) == 1, "header chain"
        self.call(0x10A1A2, (4, slot))
        assert self.rw(slot + 0x304) == 2 and self.rl(slot + 0x308) == 0x580
        keys = [self.rw(hdr + 0x138 + 2 * i) for i in range(5)]
        sums = [self.rw(hdr + 0x158 + 2 * i) for i in range(5)]
        g = slot + 0x226

        def block(i, n, buf):
            assert self.call(0x108C62, (4, buf), (2, n), (2, keys[i]), (2, sums[i])) == 1, f"block {i}"
            return bytes(e.read(buf, n))

        blocks = [block(0, 128, g)]
        gmax, emax = self.rw(g + 46), self.rw(g + 28)
        for off, n in ((0x39A, gmax * 16), (0x39E, emax * 10), (0x3A2, emax * 2)):
            e.write(slot + off, l(self.alloc(n)))
        blocks.append(block(1, gmax * 16, self.rl(slot + 0x39A)))
        b2 = block(2, 0x580, self.alloc(0x580))
        champ = self.alloc(0x500)
        e.write(champ, b2[:0x500])
        e.write(slot + 0x30C, l(champ))
        e.write(slot + 0x31A, b2[0x500:])
        blocks.append(b2)
        blocks.append(block(3, emax * 10, self.rl(slot + 0x39E)))
        blocks.append(block(4, emax * 2, self.rl(slot + 0x3A2)))
        port = self.alloc(0x740)
        e.write(slot + 0x316, l(port))
        cur = self.rl(CURSOR)
        e.write(port, e.read(cur, 0x740))
        e.write(CURSOR, l(cur + 0x740))
        assert self.call(0x108EFE, (4, slot)) == 1, "dungeon"
        h = self.rl(slot + 0x2B2)
        hd = bytes(e.read(h, 44))
        parts = [hd, bytes(e.read(self.rl(slot + 0x2AE), 16 * hd[4])),
                 bytes(e.read(self.rl(slot + 0x2B6), 2 * self.rw(slot + 0x2BE))),
                 bytes(e.read(self.rl(slot + 0x2C0), 2 * cm.w(hd, 10))),
                 bytes(e.read(self.rl(slot + 0x2A6), 2 * cm.w(hd, 6)))]
        for t in range(16):
            parts.append(bytes(e.read(self.rl(slot + 0x2C4 + 4 * t), cm.w(hd, 12 + 2 * t) * cm.THING_SIZE[t])))
        parts.append(bytes(e.read(self.rl(slot + 0x2AA), cm.w(hd, 2))))
        self.call(0x109C20, (2, slot_index))
        e.write(slot + 0x20, struct.pack(">H", 1))
        return dict(header=bytes(e.read(hdr, 0x200)), blocks=blocks, dungeon=b"".join(parts))


def same(py, orig, what):
    if bytes(py) != bytes(orig):
        print(what, [(hex(i), py[i], orig[i]) for i in range(min(len(py), len(orig))) if py[i] != orig[i]][:40])
    assert bytes(py) == bytes(orig), what
    print("ok", what)


def check_decode(raw, label):
    py = cm.decode_save(raw)
    orig = Cedt().load(0, raw)
    same(py["header"], orig["header"], f"{label}: plain header")
    for i in range(5):
        same(py["blocks"][i], orig["blocks"][i], f"{label}: block {i}")
    same(py["dungeon"], orig["dungeon"], f"{label}: dungeon parts")


def check_bad_byte():
    bad = bytearray(MINI)
    bad[0xBC0 + 7] ^= 0x01  # inside block 3
    try:
        Cedt().load(0, bytes(bad))
    except AssertionError as err:
        print("ok original rejects a changed byte:", err)
    else:
        raise AssertionError("original accepted a changed byte")
    try:
        cm.decode_save(bytes(bad))
    except ValueError as err:
        print("ok csb_mini rejects a changed byte:", err)
    else:
        raise AssertionError("csb_mini accepted a changed byte")


def synthetic_game():
    """A save with dungeon ID 12 (the prison) and the MINI.DAT champion. No real prison save exists here."""
    s = cm.decode_save(MINI)
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


def check_merge(game_raw):
    """Run the original 0x105c7e on two loaded slots and compare with csb_mini.merge."""
    c = Cedt()
    c.load(0, game_raw)
    c.load(1, MINI)
    e = c.e
    e.write(c.a5 - 0x4032, struct.pack(">H", c.rw(SLOTS + 0x230)))
    e.write(c.a5 - 0x1520, l(SEED))
    assert c.call(0x10A31C, (4, SLOTS), (2, 3)) == 1, "0x10a31c accepts the game"
    c.call(0x105C7E)
    slot1 = SLOTS + 0x3A6
    orig_hdr = bytes(e.read(slot1 + 0x26, 0x200))
    orig_champ = bytes(e.read(c.rl(slot1 + 0x30C), 0x500))
    orig_globals = bytes(e.read(slot1 + 0x226, 0x80))

    state = [SEED]

    def rng():  # 0x1064be with the same seed
        state[0] = (state[0] * 0x4CB2F + 11) & 0xFFFFFFFF
        return (state[0] >> 8) & 0xFFFF

    game = cm.decode_save(game_raw)
    assert cm.can_make_adventure(game)
    py = cm.merge(game, cm.decode_save(MINI), rng)
    same(py["header"], orig_hdr, "merge: header")
    mask = lambda b: b"".join(b[c * 0x140:c * 0x140 + 0x114] + bytes(4) + b[c * 0x140 + 0x118:(c + 1) * 0x140] for c in range(4))
    same(mask(bytes(py["blocks"][2][:0x500])), mask(orig_champ), "merge: champions (portrait pointer at 0x114 masked)")
    same(py["blocks"][0], orig_globals, "merge: globals")
    return py


def main():
    check_decode(MINI, "MINI.DAT")
    check_bad_byte()
    game = synthetic_game()
    check_decode(game, "synthetic game (encode_save)")
    new = check_merge(game)
    out = cm.encode_save(new, cm.make_rng(3))
    check_decode(out, "new CSBGAME.DAT (encode_save)")
    print("all checks passed")


if __name__ == "__main__":
    main()
