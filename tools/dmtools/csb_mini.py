# SPDX-License-Identifier: GPL-3.0-or-later
"""Decode CSB save-format files (MINI.DAT, CSBGAME.DAT) and build a new adventure.

Format and algorithm from CEDT.FTL (disk B), base 0x100000:
  0x108da2 header check, 0x108bfc block cipher, 0x108030 load order,
  0x108efe dungeon parts, 0x10a1a2 layout selection, 0x105c7e merge, 0x1085da save.
"""

import struct
import sys

HEADER = 512
KEY_WORD_CSB, KEY_WORD_DM = 29, 10  # index of the payload key word in the 256 noise bytes
THING_SIZE = (4, 6, 4, 8, 16, 4, 4, 4, 4, 8, 4, 0, 0, 0, 8, 4)  # CEDT table at 0x100568
CHAMPION_SIZE, PARTY_SIZE, PORTRAIT_SIZE = 0x140, 0x80, 0x1D0


def w(b, o):
    return struct.unpack_from(">H", b, o)[0]


def put_w(b, o, v):
    struct.pack_into(">H", b, o, v & 0xFFFF)


def xor_block(buf, off, key, words):
    """0x108bfc: XOR in place, return the checksum (same before and after)."""
    s = key
    for i in range(words, 0, -1):
        old = w(buf, off)
        new = old ^ key
        put_w(buf, off, new)
        s = (s + old + new) & 0xFFFF
        key = (key + i) & 0xFFFF
        off += 2
    return s


def checksum_only(buf, off, key, words):
    """0x108c2e: the checksum of xor_block without a change to the data."""
    s = key
    for i in range(words, 0, -1):
        old = w(buf, off)
        s = (s + old + (old ^ key)) & 0xFFFF
        key = (key + i) & 0xFFFF
        off += 2
    return s


def chain(h):
    acc = 0
    for g in range(32):
        w0, w1, w2, w3 = struct.unpack_from(">4H", h, g * 8)
        acc = w3 ^ (((w1 ^ ((w0 + acc) & 0xFFFF)) - w2) & 0xFFFF)
    return acc


def decode_header(raw):
    """0x108030/0x108da2: try the CSB key word, then the DM key word.
    Return (plain header, layout) with layout 2 = CSB (fields +2), 1 = DM."""
    for layout, kw in ((2, KEY_WORD_CSB), (1, KEY_WORD_DM)):
        h = bytearray(raw[:HEADER])
        xor_block(h, 0x100, w(h, kw * 2), 128)
        if chain(h) == sum(w(h, 0x100 + 2 * i) for i in range(128)) & 0xFFFF:
            return h, layout
    raise ValueError("header chain does not match")


def field_offsets(layout):
    """Payload fields. CSB (layout 2) is DM (layout 1) moved by 2 bytes from 0x12a on."""
    d = 2 if layout == 2 else 0
    return dict(header_byte=0x12A + d, platform=0x12B + d, game_id=0x132 + d,
                keys=0x136 + d, checksums=0x156 + d, format=0x176 + d,
                dungeon_id=0x178 + d, extra=0x17A + d)


def dungeon_size(dg):
    """0x108efe: byte length of the dungeon parts (without the checksum word)."""
    maps = dg[4]
    cols = sum(((w(dg, 44 + 16 * m + 8) & 0x7C0) >> 6) + 1 for m in range(maps))
    things = sum(w(dg, 12 + 2 * t) * THING_SIZE[t] for t in range(16))
    return 44 + 16 * maps + 2 * cols + 2 * w(dg, 10) + 2 * w(dg, 6) + things + w(dg, 2)


def decode_save(raw):
    h, layout = decode_header(raw)
    f = field_offsets(layout)
    keys = [w(h, f["keys"] + 2 * i) for i in range(16)]
    sums = [w(h, f["checksums"] + 2 * i) for i in range(5)]
    pos = HEADER
    blocks = []

    def block(i, n):
        nonlocal pos
        b = bytearray(raw[pos:pos + n])
        if len(b) != n or xor_block(b, 0, keys[i], n // 2) != sums[i]:
            raise ValueError(f"block {i} checksum")
        pos += n
        blocks.append(b)

    block(0, 128)
    g = blocks[0]
    block(1, w(g, 46) * 16)  # max active group count
    block(2, 4 * CHAMPION_SIZE + PARTY_SIZE)  # 0x580 for format 3 platform >= 4 (0x10a1a2)
    block(3, w(g, 28) * 10)  # event max count
    block(4, w(g, 28) * 2)
    portraits = bytes(raw[pos:pos + 4 * PORTRAIT_SIZE])
    pos += 4 * PORTRAIT_SIZE
    dg = bytes(raw[pos:])
    n = dungeon_size(dg)
    if len(dg) < n:
        raise ValueError("dungeon part is short")
    if len(dg) >= n + 2 and w(dg, n) != sum(dg[:n]) & 0xFFFF:
        raise ValueError("dungeon checksum")
    return dict(header=h, layout=layout, fields=f, blocks=blocks, portraits=portraits,
                dungeon=dg[:n], format=w(h, f["format"]), dungeon_id=w(h, f["dungeon_id"]))


def dungeon_dat(save):
    """Plain (uncompressed) Dungeon.DAT image: parts and the byte-sum word."""
    dg = save["dungeon"]
    return dg + struct.pack(">H", sum(dg) & 0xFFFF)


def can_make_adventure(game):
    """0x105ba2, 0x10a31c(kind 3), 0x1053ec."""
    count = w(game["blocks"][0], 10)
    if count == 0 or game["format"] != 3 or game["dungeon_id"] not in (10, 12):
        return False
    names = [bytes(game["blocks"][2][i * CHAMPION_SIZE:i * CHAMPION_SIZE + 8]).split(b"\0")[0]
             for i in range(count)]
    return all(names) and len(set(names)) == count


def merge(game, mini, rng):
    """0x105c7e: put the champions of a saved game into the MINI.DAT state."""
    gh, mh, gf, mf = game["header"], mini["header"], game["fields"], mini["fields"]
    mh[0x100:0x120] = gh[0x100:0x120]
    mh[0x120:0x128] = gh[0x120:0x128]
    gid = struct.unpack_from(">I", gh, gf["game_id"])[0]
    struct.pack_into(">I", mh, mf["game_id"], (gid + 1) & 0xFFFFFFFF)
    put_w(mh, mf["format"], w(gh, gf["format"]))
    mh[mf["header_byte"]] = 2

    count = w(game["blocks"][0], 10)
    b2 = mini["blocks"][2]
    b2[:4 * CHAMPION_SIZE] = game["blocks"][2][:4 * CHAMPION_SIZE]  # 0x109e4c
    mini["portraits"] = game["portraits"]
    put_w(mini["blocks"][0], 10, count)
    turn = (w(mini["blocks"][0], 16) - w(game["blocks"][0], 16)) & 0xFFFF
    for i in range(count):
        c = i * CHAMPION_SIZE
        for o, n in ((0, 8), (8, 20)):  # 0x109c20: zero name and title after the first NUL
            text = bytes(b2[c + o:c + o + n]).split(b"\0")[0]
            b2[c + o:c + o + n] = text.ljust(n, b"\0")
        if turn:
            b2[c + 0x1D] = (b2[c + 0x1D] + turn + 4) & 3  # cell
            b2[c + 0x1C] = (b2[c + 0x1C] + turn + 4) & 3  # direction
        b2[c + 0x20] = 0xFF  # action index
        b2[c + 0x21] = 0  # symbol step
        b2[c + 0x22] = 0  # symbols[0]
        b2[c + 0x28:c + 0x2B] = b"\0\0\0"  # max damage direction, max damage, poison events
        put_w(b2, c + 0x2C, 0xFFFF)
        put_w(b2, c + 0x2E, 0xFFFF)
        put_w(b2, c + 0x30, w(b2, c + 0x30) & 0x10)
        put_w(b2, c + 0x32, 0)  # wounds
        for cur in (0x34, 0x38, 0x3C):  # health, stamina, mana = maximum
            put_w(b2, c + cur, w(b2, c + cur + 2))
        put_w(b2, c + 0x40, 0)  # action defense
        put_w(b2, c + 0x42, 1500 + (rng() & 0xFF))  # food
        put_w(b2, c + 0x44, 1500 + (rng() & 0xFF))  # water
        for s in range(7):
            b2[c + 0x46 + 3 * s + 2] = 30  # minimum
            b2[c + 0x46 + 3 * s + 1] = b2[c + 0x46 + 3 * s]  # current = maximum
        b2[c + 0x46 + 2] = 10  # luck minimum
        for k in range(20):
            put_w(b2, c + 0x5C + 6 * k, 0)  # temporary experience
        for s in range(30):
            put_w(b2, c + 0xD4 + 2 * s, 0xFFFF)  # no objects
        put_w(b2, c + 0x110, 0)  # load
        put_w(b2, c + 0x112, 0)  # shield defense
    for i in range(count):  # 0x1062b8: give each champion a free cell
        for j in range(count):
            if j != i and b2[j * CHAMPION_SIZE + 0x1D] == b2[i * CHAMPION_SIZE + 0x1D]:
                used = {b2[k * CHAMPION_SIZE + 0x1D] for k in range(count)}
                b2[j * CHAMPION_SIZE + 0x1D] = min(set(range(4)) - used, default=0)
    return mini


def encode_save(save, rng):
    """0x1085da: new keys, block checksums, header chain (0x108e16), cipher, portraits, dungeon."""
    h, f = bytearray(save["header"]), save["fields"]
    kw = KEY_WORD_CSB if save["layout"] == 2 else KEY_WORD_DM
    keys = [rng() & 0xFFFF for _ in range(16)]
    for i, k in enumerate(keys):
        put_w(h, f["keys"] + 2 * i, k)
    blocks = [bytearray(b) for b in save["blocks"]]
    for c in range(4):  # 0x10a0f2: the file holds no portrait pointers
        blocks[2][c * CHAMPION_SIZE + 0x114:c * CHAMPION_SIZE + 0x118] = bytes(4)
    for i, b in enumerate(blocks):
        put_w(h, f["checksums"] + 2 * i, checksum_only(b, 0, keys[i], len(b) // 2))
    payload_sum = sum(w(h, 0x100 + 2 * i) for i in range(128)) & 0xFFFF
    acc = 0
    for g in range(32):
        o = g * 8
        for k in range(3):
            put_w(h, o + 2 * k, rng())
        acc = ((w(h, o + 2) ^ ((w(h, o) + acc) & 0xFFFF)) - w(h, o + 4)) & 0xFFFF
        put_w(h, o + 6, rng() if g < 31 else acc ^ payload_sum)
        acc ^= w(h, o + 6)
    xor_block(h, 0x100, w(h, kw * 2), 128)
    for i, b in enumerate(blocks):
        xor_block(b, 0, keys[i], len(b) // 2)
    return bytes(h) + b"".join(bytes(b) for b in blocks) + save["portraits"] + dungeon_dat(save)


def make_rng(seed=1):
    """0x1064be: seed = seed * 0x4cb2f + 11, result = seed >> 8 (16 bits used)."""
    state = [seed]

    def rng():
        state[0] = (state[0] * 0x4CB2F + 11) & 0xFFFFFFFF
        return (state[0] >> 8) & 0xFFFF
    return rng


def main():
    if len(sys.argv) not in (3, 5):
        sys.exit("usage: csb_mini.py MINI.DAT OUT_DUNGEON.DAT [SAVED_GAME OUT_CSBGAME.DAT]")
    mini = decode_save(open(sys.argv[1], "rb").read())
    open(sys.argv[2], "wb").write(dungeon_dat(mini))
    if len(sys.argv) == 5:
        game = decode_save(open(sys.argv[3], "rb").read())
        if not can_make_adventure(game):
            sys.exit("CAN'T MAKE NEW ADVENTURE FROM THIS SAVED GAME.")
        rng = make_rng()
        open(sys.argv[4], "wb").write(encode_save(merge(game, mini, rng), rng))


if __name__ == "__main__":
    main()
