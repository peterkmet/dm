# SPDX-License-Identifier: GPL-3.0-or-later
"""Parse FTL modules (Dungeon Master v3.60 Amiga) and lay them out in memory like BJELoad_R."""

import argparse
import struct
from dataclasses import dataclass

MAGIC = 0x6160
TAG_INFO, TAG_DATA, TAG_CODE = 0x10, 0x11, 0x12


@dataclass
class Module:
    region_size: int  # bytes from data base: jump table and uninitialized data
    data_size: int  # bytes of initialized data below the data base
    jt_offset: int  # jump table offset from the data base
    stack_size: int
    code_size: int
    compressed: bool
    data_rle: bytes
    relocs: list[int]  # signed offsets from the data base of longs to relocate
    exports: list[int]  # code offsets of exported functions
    code: bytes  # code section, unpacked


@dataclass
class Image:
    data: bytes
    base: int
    data_base: int
    code_base: int
    entries: list[int]


def _sum16(b):
    return sum(struct.unpack(f">{len(b) // 2}H", b)) & 0xFFFF


def parse(raw):
    magic, chk, version, b6, b7 = struct.unpack_from(">HHHBB", raw)
    count = struct.unpack_from(">H", raw, 0x12)[0]
    if magic != MAGIC or version != 2 or b6 != 1 or b7 != 0 or count >= 0x65:
        raise ValueError("not an FTL module")
    table = raw[0x14 : 0x14 + count * 12]
    s = sum(raw[i] * i for i in range(4, 0x14)) + sum(table[i] * ((i & 0xFF) + 1) for i in range(len(table)))
    if s & 0xFFFF != chk:
        raise ValueError("header checksum mismatch")
    secs = {}
    for i in range(count):
        tag, kind, off, size = struct.unpack_from(">HHII", table, i * 12)
        if tag in (TAG_INFO, TAG_DATA, TAG_CODE):
            if kind != 1 or tag in secs:
                raise ValueError(f"bad section {tag:#x}")
            secs[tag] = raw[off : off + size]
    if len(secs) != 3:
        raise ValueError("missing section")
    info, data, code = secs[TAG_INFO], secs[TAG_DATA], secs[TAG_CODE]

    region, dsize, _jt_size, jt_off, stack, csize, _dsec, _one, packed = struct.unpack_from(">7I2H", info)
    info_chk, code_chk, data_chk = struct.unpack_from(">3H", info, 0x22)
    if (_sum16(info) - info_chk) & 0xFFFF != info_chk:
        raise ValueError("info checksum mismatch")
    if sum(data) & 0xFFFF != data_chk:
        raise ValueError("data checksum mismatch")
    if packed:
        code = unpack(code)
    if sum(code[:csize]) & 0xFFFF != code_chk:
        raise ValueError("code checksum mismatch")

    rle_len, reloc_len = struct.unpack_from(">II", data, 2)
    rle = data[10 : 10 + rle_len]
    relocs = list(struct.unpack_from(f">{reloc_len // 2}h", data, 10 + rle_len))
    exports = list(struct.unpack_from(f">{(len(info) - 0x28) // 4}I", info, 0x28))
    return Module(region, dsize, jt_off, stack, csize, bool(packed), rle, relocs, exports, code)


def _unrle(rle):
    out = bytearray()
    i = 0
    while i < len(rle):
        w = rle[i : i + 2]
        i += 2
        out += w
        if w == b"\0\0":
            out += bytes(struct.unpack_from(">H", rle, i)[0])
            i += 2
    return out


def unpack(packed):
    """Expand the 'R#' nibble coding that APPA.FTL applies to packed code sections."""
    if packed[:2] != b"\x52\x23":
        raise ValueError("not R# packed")
    count = struct.unpack_from(">I", packed, 4)[0]
    table = struct.unpack_from(">1920H", packed, 8)
    stream = packed[0xF08:]
    pos = 0

    def nibbles(n):
        nonlocal pos
        v = 0
        for _ in range(n):
            v = (v << 4) | (stream[pos // 2] >> (4 if pos % 2 == 0 else 0)) & 0xF
            pos += 1
        return v

    out = bytearray()
    for _ in range(count):
        v = nibbles(1)
        if v < 8:
            w = table[(v << 4) | nibbles(1)]
        elif v == 15:
            w = nibbles(4)
        else:
            w = table[((v << 8) | nibbles(2)) - 0x780]
        out += struct.pack(">H", w)
    return bytes(out)


def load_image(mod, base):
    align = lambda x: (x + 1) & ~1
    data_base = base + align(mod.data_size)
    code_base = align(data_base + mod.region_size)
    mem = bytearray(code_base - base + mod.code_size)
    init = _unrle(mod.data_rle)
    if len(init) != mod.data_size:
        raise ValueError("data length mismatch")
    mem[: len(init)] = init
    for off in mod.relocs:
        p = data_base - base + off
        mem[p : p + 4] = struct.pack(">I", (struct.unpack_from(">I", mem, p)[0] + data_base) & 0xFFFFFFFF)
    entries = [code_base + e for e in mod.exports]
    for i, addr in enumerate(entries):
        p = data_base - base + mod.jt_offset + i * 8 + 2
        mem[p : p + 6] = struct.pack(">HI", 0x4EF9, addr)
    mem[code_base - base :] = mod.code[: mod.code_size]
    return Image(bytes(mem), base, data_base, code_base, entries)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("ftl")
    parser.add_argument("out")
    parser.add_argument("--base", type=lambda s: int(s, 0), default=0x100000)
    args = parser.parse_args()
    with open(args.ftl, "rb") as f:
        img = load_image(parse(f.read()), args.base)
    with open(args.out, "wb") as f:
        f.write(img.data)
    print(f"base={img.base:#x} data_base={img.data_base:#x} code_base={img.code_base:#x}")
    print(" ".join([f"A5={img.data_base:#x}"] + [f"{e:#x}" for e in img.entries]))
