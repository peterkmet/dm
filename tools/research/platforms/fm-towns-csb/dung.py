"""Parse a raw CSB/DM dungeon (parts after decompression) in big or little endian."""
import struct
SIZE = (4, 6, 4, 8, 16, 4, 4, 4, 4, 8, 4, 0, 0, 0, 8, 4)

def parse(dg, e):
    W = lambda o: struct.unpack_from(e + 'H', dg, o)[0]
    d = dict(seed=W(0), raw=W(2), nmaps=dg[4], text=W(6), start=W(8), sft=W(10),
             counts=[W(12 + 2 * t) for t in range(16)])
    p = 44; maps = []
    for i in range(d['nmaps']):
        f = W(p + 8)
        maps.append(dict(off=W(p), xo=dg[p + 6], yo=dg[p + 7], level=f & 63, w=((f >> 6) & 31) + 1,
                         h=((f >> 11) & 31) + 1, desc=dg[p + 10:p + 16]))
        p += 16
    ncols = sum(m['w'] for m in maps)
    d['cols'] = [W(p + 2 * i) for i in range(ncols)]; p += 2 * ncols
    d['sft_list'] = [W(p + 2 * i) for i in range(d['sft'])]; p += 2 * d['sft']
    d['textdata'] = [W(p + 2 * i) for i in range(d['text'])]; p += 2 * d['text']
    d['things'] = []
    for t in range(16):
        n, s = d['counts'][t], SIZE[t]
        d['things'].append([[W(p + i * s + 2 * k) for k in range(s // 2)] for i in range(n)]); p += n * s
    rawmap = dg[p:p + d['raw']]
    for m in maps:
        m['sq'] = [[rawmap[m['off'] + x * m['h'] + y] for y in range(m['h'])] for x in range(m['w'])]
    d['maps'] = maps; d['end'] = p + d['raw']
    return d
