import os, csb_mini_le as le, fmt_mini, dung, sys
from dmtools import csb_mini as be
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..', '')  # the repository root
raw, pos, _ = fmt_mini.decode(ROOT + 'reference/extracted/fm-towns-csb/files/CDATA/MINI.DAT')
F = dung.parse(raw[0x276c:], '<')
A = dung.parse(be.decode_save(open(ROOT + 'game-csb/disk-b/MINI.DAT', 'rb').read())['dungeon'], '>')
for k in ('seed', 'raw', 'nmaps', 'text', 'start', 'sft', 'counts'):
    print(k, A[k], F[k])
for i, (a, f) in enumerate(zip(A['maps'], F['maps'])):
    same = a['sq'] == f['sq']
    print(i, 'lvl', a['level'], f['level'], 'xy', a['xo'], a['yo'], f['xo'], f['yo'], 'wh', a['w'], a['h'], f['w'], f['h'],
          'desc', a['desc'].hex(), f['desc'].hex(), 'squares same' if same else 'DIFF')
    if not same:
        for x in range(a['w']):
            for y in range(a['h']):
                if a['sq'][x][y] != f['sq'][x][y]: print('   ', x, y, hex(a['sq'][x][y]), hex(f['sq'][x][y]))
for t in range(16):
    da = sum(1 for x, y in zip(A['things'][t], F['things'][t]) if x != y)
    print('type', t, 'records differ', da)
print('sft same', A['sft_list'] == F['sft_list'], 'cols same', A['cols'] == F['cols'])
for t in (1, 2, 3):
    for i, (x, y) in enumerate(zip(A['things'][t], F['things'][t])):
        if x != y: print('T', t, i, [hex(v) for v in x], [hex(v) for v in y])
