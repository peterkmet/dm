# SPDX-License-Identifier: GPL-3.0-or-later
"""The original CEDT.FTL save reader of Chaos Strikes Back in dmtools.emu, with stubs for file read and allocation."""

import struct

from dmtest.markers import GAME_CSB
from dmtools import csb_mini as cm
from dmtools.emu import Emu
from dmtools.ftl import load_image, parse

CEDT = GAME_CSB / "disk-b" / "CEDT.FTL"

CURSOR, HEAP_PTR, READ_STUB, ALLOC_STUB = 0x2F0000, 0x2F0010, 0x2F0100, 0x2F0200
FILE0, HEAP = 0x300000, 0x400000
SLOTS = 0x2E0000


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
        img = load_image(parse(CEDT.read_bytes()), 0x100000)
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
    diff = [(hex(i), py[i], orig[i]) for i in range(min(len(py), len(orig))) if py[i] != orig[i]][:40]
    assert bytes(py) == bytes(orig), f"{what}: {diff}"


def check_decode(raw, label):
    """csb_mini.decode_save and the original reader give the same header, blocks and dungeon parts."""
    py = cm.decode_save(raw)
    orig = Cedt().load(0, raw)
    same(py["header"], orig["header"], f"{label}: plain header")
    for i in range(5):
        same(py["blocks"][i], orig["blocks"][i], f"{label}: block {i}")
    same(py["dungeon"], orig["dungeon"], f"{label}: dungeon parts")
