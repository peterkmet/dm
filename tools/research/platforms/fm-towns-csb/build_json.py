"""Write reference/extracted/music_map.json from the FM-Towns executables and the SNES table."""
import json, os, struct
from collections import Counter
from grid import T, A, track
from snes_check import TRIG, D, sq
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..', '')  # the repository root

CSB = 'music/Game_Chaos_Strikes_Back_JP/FM-Towns,Track_{:02d},Music.mp3'
DMJ = 'music/Game_Dungeon_Master_JP/FM-Towns,Track_{:02d},Music.mp3'
SNS = 'music/Game_Dungeon_Master_US/Super_NES,Track_{:02d},Music.mp3'

maps = []
for m in range(10):
    mp = A['maps'][m]
    grid = [[track(m, x, y) for x in range(mp['w'])] for y in range(mp['h'])]
    c = Counter(v for row in grid for v in row if v)
    boxes = {}
    for y, row in enumerate(grid):
        for x, v in enumerate(row):
            if v:
                b = boxes.setdefault(v, [x, y, x, y])
                b[0], b[1], b[2], b[3] = min(b[0], x), min(b[1], y), max(b[2], x), max(b[3], y)
    maps.append(dict(map=m, level=mp['level'], width=mp['w'], height=mp['h'],
                     tracks={f'{k:02d}': dict(squares=c[k], bbox_x0_y0_x1_y1=boxes[k], mp3=CSB.format(k)) for k in sorted(c)},
                     grid_rows_y_then_x=grid))

edm = open(ROOT + 'reference/extracted/fm-towns/files/EDM.EXP', 'rb').read()[0x200:]
lv = struct.unpack_from('<14h', edm, 0x3fbcc)

out = {
  'fm_towns_csb': {
    'source': 'CHTWE.EXP / CHTWJ.EXP (same table), FM-Towns CSB data track',
    'table': 'image offset 0x42128 (file offset +0x200), 10 maps x 32 x 32 bytes, index map*1024 + y*32 + x',
    'value_meaning': '0 = no change; N = CD track N; map 10 (1x1) has no table entry',
    'coordinates': 'map index and map-relative x, y, same as the Amiga CSB dungeon (MINI.DAT)',
    'events': [
      dict(event='entrance screen', track=2, mp3=CSB.format(2), evidence='CHTWE 0xc8a5: special(0) -> byte table 0x41fd8[0]=2'),
      dict(event='prison (dungeon ID 12, champion selection)', track=6, mp3=CSB.format(6), evidence='CHTWE 0x237c8: dungeon ID != 13 -> play 6 once'),
      dict(event='game over', track=17, mp3=CSB.format(17), evidence='CHTWE 0x4c82: special(3) -> 0x41fd8[3]=17'),
      dict(event='story animation part 1 (STORY.ANM, ANIMTW.EXP)', track=3, mp3=CSB.format(3), evidence='STORY.ANM TD 3, TD 8; TR 1 at 0x35b2'),
      dict(event='story animation part 2', track=8, mp3=CSB.format(8), evidence='STORY.ANM TR 2 at 0x6fc8'),
      dict(event='ending animation part 1 (ENDING.ANM)', track=18, mp3=CSB.format(18), evidence='ENDING.ANM TD 18, TD 20; TR 1 at 0x1c2a'),
      dict(event='ending animation part 2', track=20, mp3=CSB.format(20), evidence='ENDING.ANM TR 2 at 0x12e16'),
      dict(event='title animation (TITLE.ANM)', track=None, mp3=None, evidence='TITLE.ANM has no TR chunk'),
      dict(event='DM-style game won code', track=None, mp3=None, evidence='CHTWE 0x523d: special(2) -> 0x41fd8[2]=0 (stop, not used by the CSB ending)'),
    ],
    'unused_tracks': [29, 30, 31],
    'maps': maps,
  },
  'fm_towns_dm': {
    'source': 'EDM.EXP / JDM.EXP (same table), FM-Towns DM data track',
    'table': 'EDM image 0x3fbcc: 14 words, audio index per level; CD track = index + 2',
    'levels': [dict(map=i, level=i, track=v + 2, mp3=DMJ.format(v + 2)) for i, v in enumerate(lv)],
    'events': [
      dict(event='entrance screen', track=5, mp3=DMJ.format(5), evidence='EDM 0xc242: special(0) -> 0x3fa80[0]=3'),
      dict(event='game won (Lord Chaos fused)', track=18, mp3=DMJ.format(18), evidence='EDM 0x4fb4: special(2) -> 0x3fa80[2]=16'),
      dict(event='game over', track=13, mp3=DMJ.format(13), evidence='EDM 0x4a96: special(3) -> 0x3fa80[3]=11'),
      dict(event='special(1), no caller', track=6, mp3=DMJ.format(6), evidence='0x3fa80[1]=4'),
    ],
    'unused_tracks': [4, 7, 20],
  },
  'snes_dm': {
    'source': 'dmweb trigger table (AUDIO_REPORT.md); checked against game/Dungeon.DAT',
    'coordinates': 'dmweb x, y are dungeon-absolute: Amiga map-relative x = x - map x offset, y = y - map y offset',
    'triggers': [dict(track=t, map=m, snes_x=x, snes_y=y,
                      amiga_x=x - D['maps'][m]['xo'], amiga_y=y - D['maps'][m]['yo'],
                      amiga_square=sq(m, x - D['maps'][m]['xo'], y - D['maps'][m]['yo']), mp3=SNS.format(t)) for t, m, x, y in TRIG],
    'events': [
      dict(event='title screen', mp3='music/Game_Dungeon_Master_Super_NES/Swoosh,Sound.mp3'),
      dict(event='entrance', track=8, mp3=SNS.format(8)),
      dict(event='introduction', track=15, mp3=SNS.format(15)),
      dict(event='end credits', track=16, mp3=SNS.format(16)),
      dict(event='end screen (map 13 trigger)', track=8, mp3=SNS.format(8)),
    ],
    'unused_tracks': [0, 18],
  },
}
json.dump(out, open(ROOT + 'reference/extracted/music_map.json', 'w'), indent=1)
print('levels', [l['track'] for l in out['fm_towns_dm']['levels']])
for t in out['snes_dm']['triggers']: print(t['track'], t['map'], (t['amiga_x'], t['amiga_y']), t['amiga_square'])
for mm in maps: print(mm['map'], {k: (v['squares'], v['bbox_x0_y0_x1_y1']) for k, v in mm['tracks'].items()})
