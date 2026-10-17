import os, sys, struct
from dmtools import csb_mini as be
import dung
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..', '')  # the repository root
d = open(ROOT + 'reference/extracted/fm-towns-csb/files/' + 'CHTWE.EXP', 'rb').read()[0x200:]
T = d[0x42128:0x42128 + 10 * 1024]
A = dung.parse(be.decode_save(open(ROOT + 'game-csb/disk-b/MINI.DAT', 'rb').read())['dungeon'], '>')
def track(m, x, y): return T[m * 1024 + y * 32 + x]
if __name__ == '__main__':
    for m in range(10):
        mp = A['maps'][m]
        print(f'map {m} level {mp["level"]} {mp["w"]}x{mp["h"]}')
        for y in range(32):
            row = ''
            for x in range(32):
                t = track(m, x, y)
                inside = x < mp['w'] and y < mp['h']
                wall = inside and (mp['sq'][x][y] >> 5) == 0
                c = '..' if t == 0 else ('XX' if t == 255 else f'{t:02d}')
                row += (c if not wall else ('##' if t == 0 else c.lower() + '#'[:0])) + ('' if inside else '') + ' '
            print(row)
