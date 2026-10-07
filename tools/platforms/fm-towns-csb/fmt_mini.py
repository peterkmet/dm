"""Decode FM-Towns CSB MINI.DAT (little-endian CSB save, block 2 = 0x57c bytes)."""
import sys
import csb_mini_le as m

def decode(path, blk2=0x57C):
    raw = open(path, 'rb').read()
    h, l = m.decode_header(raw); f = m.field_offsets(l)
    keys = [m.w(h, f['keys'] + 2 * i) for i in range(16)]
    sums = [m.w(h, f['checksums'] + 2 * i) for i in range(5)]
    pos = 512; blocks = []
    def block(i, n):
        nonlocal pos
        b = bytearray(raw[pos:pos + n]); ok = m.xor_block(b, 0, keys[i], n // 2) == sums[i]
        pos += n; blocks.append(b); return ok
    print('b0', block(0, 128)); g = blocks[0]
    print('b1', block(1, m.w(g, 46) * 16)); print('b2', block(2, blk2))
    print('b3', block(3, m.w(g, 28) * 10)); print('b4', block(4, m.w(g, 28) * 2))
    return raw, pos, blocks

if __name__ == '__main__':
    raw, pos, blocks = decode(sys.argv[1])
    for p in range(pos, pos + 2000, 2):
        dg = raw[p:]
        try:
            n = m.dungeon_size(dg)
        except Exception:
            continue
        if len(dg) >= n + 2 and m.w(dg, n) == sum(dg[:n]) & 0xFFFF and n > 1000:
            print('dungeon at', hex(p), 'portraits', p - pos, 'size', n, 'tail', len(dg) - n - 2)
