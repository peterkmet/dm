"""Check Super NES DM music trigger squares against the Amiga DM dungeon."""
import os, struct, dmcsb, dung
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..', '')  # the repository root
raw = open(ROOT + 'game/Dungeon.DAT', 'rb').read()
n = struct.unpack_from('>I', raw, 2)[0]
D = dung.parse(dmcsb.decompress_dungeon(raw[8:], n), '>')
F = dmcsb.open_dungeon(ROOT + 'game/Dungeon.DAT')
TX = {i: s for i, v, s in F.texts()}
TYPES = 'wall corridor pit stairs door teleporter fakewall ?'.split()
TRIG = [(1, 0, 8, 4), (2, 2, 14, 27), (3, 2, 25, 22), (3, 4, 8, 20), (4, 9, 19, 39), (5, 7, 45, 14),
        (6, 2, 12, 14), (6, 4, 26, 17), (7, 9, 13, 22), (8, 1, 15, 40), (8, 13, 49, 38), (9, 1, 7, 26),
        (9, 3, 12, 8), (10, 5, 13, 25), (10, 8, 20, 36), (11, 6, 19, 11), (12, 5, 25, 18), (13, 3, 28, 30),
        (13, 12, 37, 41), (14, 11, 36, 45), (17, 10, 13, 10), (17, 11, 21, 42)]

def sq(m, x, y):
    mp = D['maps'][m]
    if 0 <= x < mp['w'] and 0 <= y < mp['h']:
        return TYPES[mp['sq'][x][y] >> 5]
    return 'outside'

def texts(m, x0, y0, r):
    out = []
    for t in F.located_things(m):
        if t['type'] in ('TextString', 'Text') and abs(t['x'] - x0) <= r and abs(t['y'] - y0) <= r:
            out.append((t['x'], t['y'], t['thing']))
    return out

if __name__ == '__main__':
    for m, mp in enumerate(D['maps']):
        print('map', m, 'level', mp['level'], 'size', mp['w'], mp['h'], 'offset', mp['xo'], mp['yo'])
    for tr, m, x, y in TRIG:
        mp = D['maps'][m]
        print(f'track {tr:02d} map {m} ({x},{y}) rel: {sq(m, x, y)}  abs-offset: {sq(m, x - mp["xo"], y - mp["yo"])}')
