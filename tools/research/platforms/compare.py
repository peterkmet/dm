# Builds ../compare/: one folder per port sound event, one WAV per version, and index.html.
import struct, wave, os, hashlib, html, json, sys
import gdat, dec
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'snes'))
import res as snes_res, brr as snes_brr

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '')  # the repository root
R = ROOT; E = R + 'reference/extracted/'; OUT = E + 'compare/'
PAL = 3546895
SOUNDS = ['metallic_thud', 'switch', 'door', 'entrance_door', 'wooden_thud', 'strong_explosion', 'weak_explosion', 'scream',
          'swallow', 'champion_0_damaged', 'champion_1_damaged', 'champion_2_damaged', 'champion_3_damaged', 'swing', 'buzz',
          'party_damaged', 'spell', 'war_cry', 'blow_horn', 'attack_screamer_oitu', 'attack_scorpion', 'attack_magenta_worm',
          'attack_giggler', 'attack_painrat_dragon', 'attack_rockpile', 'attack_ghost_mummy', 'attack_water_elemental',
          'attack_couatl', 'move_animated_armour', 'move_couatl_wasp', 'move_golem_mummy_trolin_giggler_vexirk_demon',
          'move_scorpion_rat_ruster_screamer_rockpile_worm_oitu', 'move_slime_water_elemental', 'move_red_dragon', 'move_skeleton']
# Port event (Amiga 3.60 index) -> index in the 34-entry sound table of DM 2.x (Apple IIGS, FM-Towns, X68000, Super NES).
V2 = {0: 0, 1: 1, 2: 2, 23: 3, 4: 4, 5: 5, 7: 6, 25: 7, 8: 8, 9: 9, 10: 10, 11: 11, 12: 12, 16: 13, 19: 14, 20: 15, 13: 16,
      14: 17, 15: 18, 21: 19, 6: 20, 22: 21, 28: 22, 29: 23, 30: 24, 18: 25, 31: 26, 32: 27, 17: 28, 24: 29, 26: 30, 27: 31,
      33: 32, 34: 33}
ENTRANCE = 3
VERSIONS = ['amiga-3.60', 'amiga-csb', 'dos-3.4', 'pc9801', 'fm-towns', 'apple-iigs', 'atari-st-1.1', 'x68000', 'snes', 'dm2']

def be16(b): return struct.unpack('>H', b[:2])[0]
def s8(b): return bytes((x + 128) & 255 for x in b)  # signed 8-bit -> WAV unsigned

def write(path, rate, frames, width):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    w = wave.open(path, 'wb'); w.setnchannels(1); w.setsampwidth(width); w.setframerate(int(round(rate))); w.writeframes(frames); w.close()

def stats(pcm, full):
    """pcm: list of ints, full: max magnitude of the format."""
    peak = max((abs(v) for v in pcm), default=0)
    clip = sum(1 for v in pcm if v >= full - 1 or v <= -full) / max(1, len(pcm))
    return peak / full, clip

# Amiga 3.60 table (item, rate) and v2 table (item, rate byte) from the Apple IIGS program
kaos = open(R + 'decomp/KAOS.bin', 'rb').read()
AM = [struct.unpack('>hBB', kaos[0xd52 + 12 * k:0xd56 + 12 * k]) for k in range(35)]
AM = [(g, PAL / (72800 // rate)) for g, _, rate in AM]
gs = open(E + 'apple-iigs/files/DUNGEON.MASTER', 'rb').read()
T2 = [(struct.unpack_from('<H', gs, 0xe6a8 + 8 * k)[0] + 138, gs[0xe6a8 + 8 * k + 3]) for k in range(34)]
for e, k in V2.items():
    assert T2[k][0] == AM[e][0], (e, k, T2[k], AM[e])
RB_ENTRANCE = 0x91  # entrance door rate byte (Apple IIGS start segment, X68000 FTLCODE)

data = {
    'amiga-3.60': gdat.parse(R + 'game/Graphics.DAT')['items'],
    'amiga-csb': gdat.parse(R + 'game-csb/Graphics.DAT')['items'],
    'dos-3.4': gdat.parse(E + 'Game,Dungeon_Master,DOS,Software/DungeonMasterPC34/DATA/GRAPHICS.DAT')['items'],
    'pc9801': gdat.parse(E + 'pc9801/files/JDATA/GRAPHICS.DAT')['items'],
    'fm-towns': gdat.parse(E + 'fm-towns/files/DATA/GRAPHICS.DAT')['items'],
    'apple-iigs': gdat.parse(E + 'apple-iigs/files/GRAPHICS.GAME')['items'],
    'atari-st-1.1': gdat.parse(E + 'Game,Dungeon_Master,Atari_ST,Software/HardDisk/2009-02-22 PP/GRAPHICS.DAT')['items'],
    'x68000': gdat.parse(E + 'x68000/files/GRAPHICS.DAT')['items'],
}
OFS = {'amiga-3.60': 0, 'amiga-csb': 0, 'dos-3.4': 0, 'pc9801': 138, 'fm-towns': 138, 'apple-iigs': 138, 'atari-st-1.1': 138, 'x68000': 138}
UNSIGNED = {'dos-3.4', 'pc9801'}

def pcm8(ver, g):
    """Signed 8-bit sample bytes as the version plays them."""
    it = data[ver][g - OFS[ver]]
    body = it[2:] if ver == 'amiga-3.60' else it[2:2 + be16(it)]  # Amiga 3.60: port plays size-2; others: first word count
    return bytes((x ^ 0x80) for x in body) if ver in UNSIGNED else body

def rate8(ver, e):
    if ver in ('amiga-3.60', 'amiga-csb'): return AM[e][1], 'Paula, rate from sound table'
    if ver == 'dos-3.4': return 6000, '6000 Hz for every sound (dmweb, not checked in code)'
    if ver == 'pc9801': return 6000, '6000 Hz (dmweb, not checked in code)'
    if ver == 'fm-towns': return 5500, '5500 Hz constant in the EDM.EXP play routine'
    if ver == 'apple-iigs':
        rb = RB_ENTRANCE if e == ENTRANCE else T2[V2[e]][1]
        return 894886 / 34 * rb / 512, 'DOC freq = rate byte %d, 32 oscillators' % rb

cells = {}  # (event, version) -> dict

def add(e, ver, path_rate, frames, width, pcm, full, bits, note, key=None):
    name = '%s.wav' % ver
    folder = 's%02d_%s' % (e, SOUNDS[e]) if isinstance(e, int) else e
    write(OUT + folder + '/' + name, path_rate, frames, width)
    peak, clip = stats(pcm, full)
    cells[(folder, ver)] = dict(file=folder + '/' + name, rate=round(path_rate), bits=bits, n=len(pcm),
                                sec=len(pcm) / path_rate, peak=peak, clip=clip, note=note, key=key)

# 8-bit versions with the Amiga sample data
for e in range(35):
    g = AM[e][0]
    for ver in ('amiga-3.60', 'amiga-csb', 'dos-3.4', 'pc9801', 'fm-towns', 'apple-iigs'):
        if ver in ('pc9801', 'fm-towns', 'apple-iigs') and e != ENTRANCE and e not in V2: continue
        d = pcm8(ver, g); hz, how = rate8(ver, e)
        key = hashlib.sha1(d[:min(len(d), be16(data['amiga-csb'][g]))]).hexdigest()[:8]
        pcm = [x - 256 if x > 127 else x for x in d]
        note = how + ('; entrance door = door sample' if e == ENTRANCE else '')
        if e == ENTRANCE and ver in ('dos-3.4', 'pc9801'): note += '; entrance door rate not checked'
        add(e, ver, hz, s8(d), 1, pcm, 128, 8, note, key)
# Atari ST 1.1: SND1 4-bit RLE, MFP timer A: 614400 / rate byte
for e in list(V2) + [ENTRANCE]:
    k = 2 if e == ENTRANCE else V2[e]
    if k > 21: continue
    rb = RB_ENTRANCE if e == ENTRANCE else T2[k][1]
    o, _, _ = dec.snd1(data['atari-st-1.1'][T2[k][0] - 138])
    note = 'SND1 4-bit RLE, MFP timer A = %d' % rb
    if rb == 138: note += ' (rate byte from the 2.x table; the ST table was not checked)'
    add(e, 'atari-st-1.1', 614400 / rb, o, 1, [(x // 17) * 16 - 128 for x in o], 128, 4, note)
# X68000: OKI ADPCM, IOCS _ADPCMOUT 5.2 kHz (rate byte < 130) or 3.9 kHz
for e in list(V2) + [ENTRANCE]:
    k = 2 if e == ENTRANCE else V2[e]
    if k > 21: continue
    rb = RB_ENTRANCE if e == ENTRANCE else T2[k][1]
    hz = 15625 / 4 if rb >= 0x82 else 15625 / 3
    o = dec.oki(data['x68000'][T2[k][0] - 138][2:], False)
    add(e, 'x68000', hz, struct.pack('<%dh' % len(o), *[v * 16 for v in o]), 2, o, 2048, 4,
        'OKI ADPCM, rate byte %d -> %s' % (rb, '3.9 kHz' if rb >= 0x82 else '5.2 kHz'))

# Super NES: SFX scripts (ROM item 548 + table index) for the SPC700 driver
def snes_script(item):
    d = snes_res.get(item); n = d[3]
    ptrs = [struct.unpack_from('<H', d, 4 + n + 2 * c)[0] for c in range(8)]
    chans = []
    for p in ptrs:
        if not p: continue
        y = p + 6; ev = []; tempo = 0x14
        while y < len(d) and d[y] != 0:
            c = d[y]; y += 1
            if c >= 0xe0:
                ln = {0xe4: 2, 0xe5: 2, 0xe6: 2, 0xf2: 2}.get(c, 1)
                arg = d[y:y + ln]; y += ln; ev.append((c, arg))
            elif c >= 0x80:
                dur = None
                if y < len(d) and d[y] < 0x80 and d[y] != 0: dur = d[y]; y += 1
                ev.append(('note', c, dur))
        chans.append(ev)
    return chans

def snes_render(item):
    """Mono render at 32 kHz. Plays notes, ties, rests, waits, sample and volume changes; ignores envelopes and detune."""
    out = []; info = []
    for ev in snes_script(item):
        t = 0.0; tempo = 0x14; vol = 255; smp = None; dur = 1; buf = []
        for x in ev:
            if x[0] == 0xe0: tempo = x[1][0]
            elif x[0] == 0xe1: smp = snes_brr.sample(x[1][0])
            elif x[0] == 0xe3: vol = x[1][0]
            elif x[0] == 0xf2: buf += [0.0] * int(struct.unpack('<H', x[1])[0] * 256 / (500 * tempo) * 32000)
            elif x[0] == 'note':
                if x[2]: dur = x[2]
                n = int(dur * 256 / (500 * tempo) * 32000)
                if x[1] >= 0xc8 or smp is None: buf += [0.0] * n; continue
                pitch = (snes_brr.T[x[1] & 0x7f] * smp['tuning'] >> 8) & 0x3fff
                info.append((x[1], pitch, smp['item']))
                pcm = smp['pcm']; loop = smp['loop'] // 9 * 16 if smp['looped'] else None
                pos = 0.0; step = pitch / 4096
                for i in range(n + 256):  # 256 samples of release
                    j = int(pos)
                    if j >= len(pcm):
                        if loop is None: break
                        pos -= len(pcm) - loop; j = int(pos)
                    g = 1.0 if i < n else 1 - (i - n) / 256
                    buf.append(pcm[j] * vol / 255 * g); pos += step
        out = [a + b for a, b in zip(out + [0] * max(0, len(buf) - len(out)), buf + [0] * max(0, len(out) - len(buf)))]
    return [max(-16384, min(16383, int(v))) for v in out], info

def snes_cell(folder, item, label):
    chans = snes_script(item)
    notes = [x for ev in chans for x in ev if x[0] == 'note' and x[1] < 0xc8]
    ins = [x[1][0] for ev in chans for x in ev if x[0] == 0xe1]
    smp = snes_brr.sample(ins[0])
    tempo = next((x[1][0] for x in chans[0] if x[0] == 0xe0), 0x14)
    if len(notes) == 1 and len(chans) == 1 and not smp['looped']:
        nt, dur = notes[0][1], notes[0][2]
        hz = snes_brr.rate(nt, smp['tuning'])
        keep = int(dur * 256 / (500 * tempo) * hz)
        pcm = smp['pcm'][:keep]
        note = 'BRR sample %02X (ROM item %d), note %02X -> %d Hz; key-off after %d ticks (%d of %d samples)' % (
            ins[0], smp['item'], nt, round(hz), dur, len(pcm), len(smp['pcm']))
    else:
        pcm, info = snes_render(item); hz = 32000
        note = 'rendered at 32 kHz from SFX script (ROM item %d): samples %s, notes %s; envelopes ignored' % (
            item, ' '.join('%02X' % i for i in sorted(set(ins))), ' '.join('%02X' % n for n, _, _ in info[:6]) + (' ...' if len(info) > 6 else ''))
    add(folder, 'snes', hz, struct.pack('<%dh' % len(pcm), *[2 * v for v in pcm]), 2, pcm, 16384, 4, label + note)

for e, k in V2.items():
    snes_cell('s%02d_%s' % (e, SOUNDS[e]), 548 + k, '')
# FTL / title swoosh row
sw = open(R + 'decomp/SWSH.bin', 'rb').read(); per, ln = struct.unpack('>HI', sw[0xa54:0xa5a]); d = sw[0xa5a:0xa5a + ln]
add('x_ftl_logo_swoosh', 'amiga-3.60', PAL / per, s8(d), 1, [x - 256 if x > 127 else x for x in d], 128, 8, 'SWSH.FTL, period %d' % per)
snes_cell('x_ftl_logo_swoosh', 544, 'title swoosh (sound -4, called by the intro code); ')
# Super NES sounds with no port event (negative sound numbers, ROM items 540-546)
for k, item in ((-8, 540), (-7, 541), (-6, 542), (-5, 543), (-3, 545)):
    snes_cell('x_snes_extra_%d' % -k, item, 'sound %d, no port event; ' % k)

# DM2 (Skullkeep, DOS): a sound that plays in the same situation; ../dm2/ comes from dm2sounds.py
import dm2sounds
DM2 = {x['file']: x for x in json.load(open(E + 'dm2/sounds.json'))}
def dm2_pcm(fn):
    w = wave.open(E + 'dm2/sounds/' + fn); d = w.readframes(w.getnframes()); w.close()
    return d, [x - 128 for x in d]
for e, fn in dm2sounds.DM1_MATCH.items():
    d, pcm = dm2_pcm(fn); x = DM2[fn]
    add('s%02d_%s' % (e, SOUNDS[e]), 'dm2', dm2sounds.RATE, d, 1, pcm, 128, 8, '%s: %s' % (fn, x['trigger']))

# HTML
rows = ['s%02d_%s' % (e, SOUNDS[e]) for e in range(35)] + sorted({f for f, _ in cells if f.startswith('x_')})
keys = {}
for c in cells.values():
    if c['key']: keys.setdefault(c['key'], []).append(c)
ref = {}  # sha -> first version name
for (f, v), c in sorted(cells.items(), key=lambda kv: VERSIONS.index(kv[0][1])):
    if c['key'] and c['key'] not in ref: ref[c['key']] = v
h = ['<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">',
     '<title>DM Sound Comparison</title><style>',
     ':root{--bg:#fff;--fg:#111;--mut:#666;--line:#ddd;--same:#e8f1ff;--hd:#f4f4f4}',
     '@media (prefers-color-scheme: dark){:root{--bg:#161616;--fg:#eee;--mut:#999;--line:#333;--same:#1d2a40;--hd:#222}}',
     'body{background:var(--bg);color:var(--fg);font:13px system-ui,sans-serif;margin:16px}',
     'table{border-collapse:collapse}th,td{border:1px solid var(--line);padding:4px;vertical-align:top}',
     'th{background:var(--hd);position:sticky;top:0;z-index:1}td.ev{font-weight:600;white-space:nowrap}',
     'td.same{background:var(--same)}audio{width:170px;height:28px;display:block}.m{color:var(--mut);font-size:11px;max-width:200px}',
     '.wrap{overflow-x:auto}</style></head><body><h1>Dungeon Master sound effects by version</h1>',
     '<p>Rows: port sound events (index = Amiga 3.60 sound table). Columns: versions. A blue cell has the same 8-bit sample data as '
     'the version named in it ("data = ..."); only the playback rate differs. Amiga 3.60 items have 2 more trailing samples than the '
     'other 8-bit versions; the comparison ignores them. The dm2 column holds a Dungeon Master II (DOS) sound that plays in the same situation; it is a different recording. Peak = maximum level of full scale; clip = share of samples at full scale.</p>',
     '<div class="wrap"><table><tr><th>event</th>' + ''.join('<th>%s</th>' % v for v in VERSIONS) + '</tr>']
for f in rows:
    h.append('<tr><td class="ev">%s</td>' % html.escape(f))
    for v in VERSIONS:
        c = cells.get((f, v))
        if not c: h.append('<td></td>'); continue
        same = c['key'] and ref[c['key']] != v
        tag = ('data = %s; ' % ref[c['key']]) if same else ''
        h.append('<td class="%s"><audio controls preload="none" src="%s"></audio><div class="m">%d Hz, %d-bit, %.2f s, peak %.2f, clip %.1f %%<br>%s%s</div></td>' % (
            'same' if same else '', html.escape(c['file']), c['rate'], c['bits'], c['sec'], c['peak'], 100 * c['clip'], tag, html.escape(c['note'])))
    h.append('</tr>')
h.append('</table></div>')
# DM2 extra: one player per different DM2 sample, grouped by use
first = {}
for x in DM2.values(): first.setdefault(x['sha'], x)
used = {DM2[f]['sha'] for f in dm2sounds.DM1_MATCH.values()}
groups = [('Ambient', lambda x: x['cls'] in ('ambient', 'scripted', 'weather') or x['file'] == 'c13_i48_e81.wav'),
          ('Footsteps and movement', lambda x: x['cls'] in ('creature move', 'floor')),
          ('Other', lambda x: True)]
h.append('<h2>DM2 extra</h2><p>Dungeon Master II (Skullkeep, DOS) sounds that DM1 has no event for, or that differ from the DM1 match. '
         'One row per different sample (11025 Hz, 8-bit). "Also" lists other GDAT keys with the same data. Details: dm2/SOUNDS.md.</p>')
done = set()
for title, test in groups:
    h.append('<h3>%s</h3><div class="wrap"><table><tr><th>file</th><th>player</th><th>class</th><th>trigger</th><th>also</th></tr>' % title)
    for x in sorted(first.values(), key=lambda x: (x['cat'], x['idx'], x['entry'])):
        if x['sha'] in done or x['sha'] in used or not test(x): continue
        done.add(x['sha'])
        also = [y['file'] for y in DM2.values() if y['sha'] == x['sha'] and y is not x]
        d, _ = dm2_pcm(x['file']); write(OUT + 'dm2_extra/' + x['file'], dm2sounds.RATE, d, 1)
        h.append('<tr><td class="ev">%s<div class="m">%s, %.2f s</div></td><td><audio controls preload="none" src="dm2_extra/%s"></audio></td>'
                 '<td>%s</td><td class="m">%s</td><td class="m">%s</td></tr>' % (html.escape(x['file']), html.escape(x['name'] or x['catname']),
                 x['sec'], html.escape(x['file']), html.escape(x['cls']), html.escape(x['trigger']),
                 html.escape(', '.join(also[:6]) + (' ...' if len(also) > 6 else ''))))
    h.append('</table></div>')
h.append('</body></html>')
open(OUT + 'index.html', 'w').write('\n'.join(h))
json.dump({'%s|%s' % k: v for k, v in cells.items()}, open(OUT + 'cells.json', 'w'), indent=1)
print(len(cells), 'cells', {v: sum(1 for (f, w) in cells if w == v) for v in VERSIONS})
