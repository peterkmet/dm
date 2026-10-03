import struct

import pytest

from dmtools.ftl import load_image, parse


def pack_literals(code):
    """R# pack code using only literal codes (nibble f + 4 nibbles per word)."""
    nibbles = "".join("f" + code[i : i + 2].hex() for i in range(0, len(code), 2))
    nibbles += "0" * (-len(nibbles) % 4)
    return struct.pack(">HHI", 0x5223, 0x5223, len(code) // 2) + bytes(0xF00) + bytes.fromhex(nibbles)


def make_ftl(code, data_words, relocs, exports, region=0x30, data_size=8, packed=False, tags=(0x10, 0x11, 0x12)):
    """Build an FTL module the same way BJELoad_R reads it."""
    jt_offset = 0x10
    rle = b"".join(struct.pack(">H", w) for w in data_words)
    reloc_bytes = b"".join(struct.pack(">h", r) for r in relocs)
    sec11 = struct.pack(">HII", 1, len(rle), len(reloc_bytes)) + rle + reloc_bytes
    exp = b"".join(struct.pack(">I", e) for e in exports)
    info_wo_sum = [region, data_size, len(exports) * 8, jt_offset, 0x7D00, len(code), len(sec11), 1, int(packed), 0]
    sum16 = lambda b: sum(struct.unpack(f">{len(b) // 2}h", b)) & 0xFFFF
    head = struct.pack(">IIIIIIIHHH", *info_wo_sum)
    code_sum = sum(code) & 0xFFFF
    data_sum = sum(sec11) & 0xFFFF
    sums = struct.pack(">HH", code_sum, data_sum)
    chk = (sum16(head) + sum16(sums) + sum16(exp)) & 0xFFFF
    sec10 = head + struct.pack(">H", chk) + sums + exp
    secs = [(0x10, sec10), (0x11, sec11), (0x12, pack_literals(code) if packed else code)]
    secs = [(tag, body) for (_, body), tag in zip(secs, tags)]
    off = 0x14 + 12 * len(secs)
    table = b""
    for tag, body in secs:
        table += struct.pack(">HHII", tag, 1, off, len(body))
        off += len(body)
    header = struct.pack(">HHHBB10sH", 0x6160, 0, 2, 1, 0, b"\0" * 10, len(secs))
    s = sum(header[i] * i for i in range(4, 0x14))
    s += sum(table[i] * ((i & 0xFF) + 1) for i in range(len(table)))
    header = header[:2] + struct.pack(">H", s & 0xFFFF) + header[4:]
    return header + table + b"".join(body for _, body in secs)


def test_parse_and_load_image():
    code = bytes.fromhex("4e75") + bytes.fromhex("4e71 4e75")
    # RLE 0000 0002 expands to 4 zero bytes; data = 0002 0000 0000 0010
    words = [0x0002, 0x0000, 0x0002, 0x0010]
    ftl = make_ftl(code, words, relocs=[-4], exports=[0, 2])

    mod = parse(ftl)
    assert mod.code == code
    assert mod.exports == [0, 2]
    assert not mod.compressed

    img = load_image(mod, 0x100000)
    base = img.data_base
    assert base == 0x100000 + 8
    assert img.code_base == base + 0x30
    raw = img.data
    at = lambda addr, n: raw[addr - 0x100000 : addr - 0x100000 + n]
    assert at(0x100000, 4) == bytes.fromhex("0002 0000")
    assert struct.unpack(">I", at(base - 4, 4))[0] == base + 0x10
    # jump table: JMP abs.l to each export
    assert at(base + 0x10 + 2, 6) == b"\x4e\xf9" + struct.pack(">I", img.code_base)
    assert at(base + 0x18 + 2, 6) == b"\x4e\xf9" + struct.pack(">I", img.code_base + 2)
    assert at(img.code_base, 6) == code
    assert img.entries == [img.code_base, img.code_base + 2]


def test_parse_rejects_bad_checksum():
    ftl = bytearray(make_ftl(b"\x4e\x75", [0], [], [0]))
    ftl[-1] ^= 0xFF
    with pytest.raises(ValueError, match="code checksum"):
        parse(bytes(ftl))


def test_unpack_r23():
    from dmtools.ftl import unpack

    words = [0] * 0x780
    words[5], words[0x80] = 0xAAAA, 0xBBBB
    # nibbles: 0 5 -> dict[5]; 8 0 0 -> dict[0x800 - 0x780]; f 1 2 3 4 -> literal 0x1234
    stream = bytes.fromhex("0580 0f12 3400")
    packed = struct.pack(">HHI", 0x5223, 0x5223, 3) + struct.pack(">1920H", *words) + stream
    assert unpack(packed) == bytes.fromhex("aaaa bbbb 1234")


def test_parse_unpacks_packed_code():
    code = bytes.fromhex("4e71 4e71 4e75")
    mod = parse(make_ftl(code, [0x0002, 0x0000, 0x0002, 0x0010], [-4], [0], packed=True))
    assert mod.compressed
    assert mod.code == code


def test_parse_rejects_missing_section():
    with pytest.raises(ValueError, match="missing section"):
        parse(make_ftl(b"\x4e\x75", [0], [], [0], tags=(0x10, 0x11, 0x13)))


def test_load_image_rejects_wrong_data_length():
    mod = parse(make_ftl(b"\x4e\x75", [0x0000, 0x0010], [], [0], data_size=2))
    with pytest.raises(ValueError, match="data length"):
        load_image(mod, 0x100000)
