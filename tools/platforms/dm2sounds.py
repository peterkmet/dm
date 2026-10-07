# Extracts the 292 DM2 (Skullkeep, DOS) sound items to ../dm2/sounds/ and writes ../dm2/sounds.json.
import os, re, json, wave, hashlib, struct
import dm2gdat

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '')  # the repository root
OUT = ROOT + 'reference/extracted/dm2/'
RATE = 11025
CREATURE = {0x00: ('creature move', 'move (AI actions 1, 2, 9)'), 0x01: ('creature move', 'turn or step (AI actions 3, 4, 6, 7)'),
            0x02: ('creature attack', 'throw (AI actions 0x0e, 0x0f) or melee'), 0x03: ('creature other', 'cloud explosion part'),
            0x04: ('creature other', 'action 5'), 0x05: ('creature attack', 'melee or hurt'), 0x06: ('creature other', 'death part'),
            0x07: ('creature attack', 'melee attack (AI actions 8, 0x10, 0x26) or shoot'), 0x08: ('creature other', 'steal or take items'),
            0x09: ('creature hurt', 'random pain sound when damaged'), 0x0a: ('creature hurt', 'random pain sound when damaged; also steal or use'),
            0x0b: ('creature other', 'merchant: action 0x20'), 0x0c: ('creature other', 'merchant: action 0x1c'),
            0x0d: ('creature other', 'merchant: action 0x1b'), 0x0e: ('creature other', 'action 0x1c or explode (0x3d)'),
            0x0f: ('creature other', 'action 0 (idle)'), 0x10: ('creature hurt', 'hit reaction (action 0x11)'),
            0x11: ('creature death', 'dies or vanishes (action 0x13)'), 0x12: ('creature attack', 'second attack sound'),
            0x84: ('interface', 'click on a creature that has no interaction'), 0x8d: ('combat', 'hit by a champion weapon')}
CAT = {3: 'global', 8: 'wall set', 9: 'wall ornament', 10: 'floor ornament', 13: 'explosion', 14: 'door', 15: 'creature',
       16: 'weapon', 17: 'clothing', 18: 'scroll', 19: 'potion', 20: 'container', 21: 'misc item', 22: 'champion',
       23: 'sky (weather)', 24: 'teleporter'}
EXPL = {0: 'fireball', 2: 'lightning bolt', 3: 'non-material (dispell)', 4: 'door effect', 6: 'poison bolt', 7: 'poison cloud',
        9: 'push effect', 10: 'push effect', 13: 'door or square effect', 48: 'weather lightning', 49: 'area damage', 68: 'special'}

def classify(c, i, e, keys):
    if c == 3 and i == 0:
        return {0x81: ('explosion', 'creature transforms or explodes (AI 0x3b-0x40)'), 0x88: ('switch', 'door button or wall click'),
                0x89: ('teleporter', 'creature moves through a teleporter; sensor teleport'), 0x8b: ('interface', 'sound volume test')}[e]
    if c == 3: return ('scripted', 'map %d: text "sound" trigger on a square (entry = text value), plays at the square' % (i - 1))
    if c == 8: return ('wall', 'click on a plain wall (knock)')
    if c in (9, 10):
        if (c, i, e) == (9, 91, 0xfb): return ('eat/drink', 'mouth in the inventory: eat or drink')
        cyc = keys.get((c, i, 0x0b, 0x0d)) or 0
        if c == 9 and i == 254: return ('switch', 'default wall ornament sound')
        if c == 10 and i == 254: return ('switch', 'default floor ornament sound')
        what = 'wall switch, button, or lever' if c == 9 else 'floor plate or square sensor (also steps on it)'
        if cyc & 0x7fff or (c, i) in ((10, 8), (9, 133)):
            n = cyc & 0x7fff or {(10, 8): 60, (9, 133): 26}[(c, i)]
            return ('ambient', 'animated ornament (cycle %d steps): ambient loop of sensor 0x2c with the audible flag; also %s' % (n, what))
        return ('switch' if c == 9 else 'floor', what + ' with the audible flag')
    if c == 13:
        return ('explosion', '%s explodes' % EXPL.get(i, 'explosion %d' % i)) if e == 0x81 else \
               ('spell', 'missile of %s is cast or launched' % EXPL.get(i, 'type %d' % i)) if e == 0x86 else ('explosion', 'explosion effect')
    if c == 14: return {0x8d: ('door', 'door hit by a weapon'), 0x8e: ('door', 'door moves'), 0x8f: ('door', 'door closes fully')}[e]
    if c == 15:
        if 0x84 == e or 0x8d == e or i != 254: return CREATURE[e]
    if 16 <= c <= 21:
        if e == 0x85: return ('item', 'item lands or hits (thrown, dropped, falls into a pit)')
        if e == 0x86: return ('item', 'item is thrown or shot')
        return ('combat', 'weapon action sound (action table entry)')
    if c == 22 and e < 0x80: return ('combat', 'bare-hand action (punch, kick) of the action table')
    if c == 22:
        return {0x82: ('champion', 'champion is hurt'), 0x83: ('eat/drink', 'champion eats or drinks (also at a fountain)'),
                0x87: ('champion', 'champion falls into a pit or the party dies (scream)'),
                0x8a: ('champion', 'party bumps into a wall or is pushed by a creature')}[e]
    if c == 23: return ('weather', 'thunder (outdoor storm)')
    if c == 24: return ('teleporter', 'party or creature moves through an audible teleporter')
    return ('?', '')

def main():
    keys, item, text = dm2gdat.load()
    label = dm2gdat.dev_labels()
    os.makedirs(OUT + 'sounds', exist_ok=True)
    rows = []; first = {}
    for k in sorted(k for k in keys if k[2] == 2):
        c, i, _, e = k
        name = label.get((c, i)) if c == 15 else (text(c, i, 0x18) if 16 <= c <= 22 else None)
        slug = re.sub(r'[^a-z0-9]+', '_', (name or '').lower()).strip('_')
        b = item(keys[k]); assert b[:6] == b'\x77\x2b\x08\x01\x00\x00'
        pcm = b[6:]
        fn = 'c%d_i%d_e%02x%s.wav' % (c, i, e, '_' + slug if slug else '')
        w = wave.open(OUT + 'sounds/' + fn, 'wb'); w.setnchannels(1); w.setsampwidth(1); w.setframerate(RATE)
        w.writeframes(bytes(x ^ 0x80 for x in pcm)); w.close()
        h = hashlib.sha1(pcm).hexdigest()[:8]; first.setdefault(h, fn)
        cls, trig = classify(c, i, e, keys)
        rows.append(dict(file=fn, cat=c, catname=CAT[c], idx=i, entry=e, name=name or '', cls=cls, sec=round(len(pcm) / RATE, 2),
                         trigger=trig, sha=h, same=first[h] if first[h] != fn else ''))
    json.dump(rows, open(OUT + 'sounds.json', 'w'), indent=1)
    print(len(rows), 'sounds,', len(first), 'different samples')


DM1_NAMES = {0: 'metallic thud', 1: 'switch', 2: 'door', 4: 'wooden thud', 5: 'strong explosion', 7: 'scream', 8: 'swallow',
             9: 'champion 0 damaged', 10: 'champion 1 damaged', 11: 'champion 2 damaged', 12: 'champion 3 damaged', 13: 'swing',
             14: 'buzz (teleporter)', 15: 'party damaged', 16: 'spell', 21: 'attack Magenta Worm', 22: 'attack Giggler', 24: 'attack Rockpile (DM2 Rocky)',
             25: 'attack Ghost, Mummy', 30: 'move Golem, Mummy, Trolin, Giggler, Vexirk, Demon',
             31: 'move Scorpion, Rat, Ruster, Screamer, Rockpile, Worm, Oitu', 32: 'move Slime, Water Elemental', 34: 'move Skeleton'}
# DM1 port event (Amiga 3.60 index) -> DM2 sound that plays in the same situation.
DM1_MATCH = {0: 'c16_i254_e85.wav', 1: 'c9_i19_e88.wav', 2: 'c14_i254_e8e.wav', 4: 'c21_i254_e85.wav', 5: 'c13_i0_e81.wav',
             7: 'c22_i254_e87.wav', 8: 'c22_i254_e83.wav', 9: 'c22_i0_e82_aliai_mon.wav', 10: 'c22_i1_e82_kol_del_tac.wav',
             11: 'c22_i2_e82_anders_light_wielder.wav', 12: 'c22_i3_e82_het_farvil.wav', 13: 'c16_i254_e00.wav',
             14: 'c24_i0_e89.wav', 15: 'c22_i254_e8a.wav', 16: 'c13_i0_e86.wav', 21: 'c15_i15_e07_tiger_striped_worm.wav',
             22: 'c15_i13_e0a_giggler.wav', 24: 'c15_i12_e07_rocky.wav', 25: 'c15_i38_e07_mummy.wav', 30: 'c15_i38_e00_mummy.wav',
             31: 'c15_i15_e00_tiger_striped_worm.wav', 32: 'c15_i9_e00_glop.wav', 34: 'c15_i65_e00_skeleton.wav'}

INTRO = '''# Dungeon Master II sound effects

This page lists the 292 sound effects of Dungeon Master II (Skullkeep, DOS) and how the game uses them.

## Source and format

- Source: `dm2/game/DATA/GRAPHICS.DAT`, GDAT items with type 2 (`dm2/docs/skull-formats.md`, section 1).
- Item: 6-byte header (`77 2b 08 01 00 00`), then signed 8-bit mono PCM. The game plays it at 11025 Hz.
- Output: `sounds/c<category>_i<index>_e<entry>_<name>.wav`, 8-bit unsigned PCM, 11025 Hz, mono. Category and index are decimal. The entry is two hex digits.
- Names: creatures from the developer labels (language `0xf`, entry 0). Items and champions from text entry `0x18`. Ornaments, doors, and explosions have no name in the GDAT.
- The 292 items hold 107 different samples. The column "Same as" names the first file with the same data.
- Tools: `tools/platforms/dm2gdat.py` (reader), `dm2sounds.py` (this extraction and page), `compare.py` (comparison page).

## How the game selects a sound

- `sound_request(category, index, entry, priority, volume, x, y, delay)` (`0x15ca9`) plays a GDAT sound at a square. The volume and the stereo position come from the distance to the party.
- `sound_play_with_fallback` (`0x15baa`) uses `index` when the sound exists. Else it uses index `0xfe` (254), the default of the category.
- A `dm2/decomp/SKULL.c` call names the trigger. The table column "Trigger" gives it.

## Category meanings

| Category | Meaning | Index | Entries |
|---|---|---|---|
| 3 | Global sounds and map scripts | 0: global. `map + 1`: sounds of that map | `0x81` explosion of a creature, `0x88` button click, `0x89` teleport, `0x8b` volume test. Map index: entry = value of a sound text on a square |
| 8 | Wall set | graphics set; 254 = default | `0x84` click on a plain wall |
| 9 | Wall ornaments | ornament | `0x88` activation and ambient loop; `0xfb` mouth in the inventory |
| 10 | Floor ornaments | ornament | `0x88` activation, step on the square, ambient loop |
| 13 | Explosions | explosion type (`0xff80 + n`) | `0x81` explosion, `0x86` cast or launch |
| 14 | Doors | door type; 254 = default | `0x8d` hit, `0x8e` moves, `0x8f` closed |
| 15 | Creatures (also static objects such as bush, pillar, table) | creature type | animation sounds (below), `0x84` click, `0x8d` hit by a weapon |
| 16-21 | Weapons, clothing, scrolls, potions, containers, misc items | item; 254 = default | `0x85` lands or hits, `0x86` thrown, low entries = action sounds |
| 22 | Champions | portrait; 254 = default | `0x82` hurt, `0x83` eat or drink, `0x87` pit fall or death, `0x8a` bump; 0 and 1 punch and kick |
| 23 | Sky (outdoor weather) | graphics set | 0 = thunder |
| 24 | Teleporters | 0 | `0x89` teleport |

### Creature entries

A creature animation step (GDAT `(15, type, 7, 0xfc)`, 4 bytes) holds a sound entry in byte 0 (`0x7f` = none). `creature_ai_animate_and_get_next_time` plays it with `sound_request(15, type, entry, ...)`. The table `(15, type, 8, 0xfb)` maps an AI action code to the first step of its sequence. From these tables:

- `0x00`: move (AI actions 1, 2, 9). These are the footsteps of the creatures.
- `0x01`: turn or second move step (minions).
- `0x07`, `0x12`, `0x02`, `0x05`: attack, shoot, throw.
- `0x10`: hit reaction (action `0x11`). `0x09`, `0x0a`: random pain sound (1 in 8) when the creature takes damage (`creature_damage_current_group`).
- `0x11`: death (action `0x13`).
- `0x08`, `0x0a`: steal, take items. `0x0b`-`0x0d`: merchant actions.
'''

FINDINGS = '''
## Ambient sounds

- Source: an animated ornament with a sensor of type `0x2c` (animated ornament) and the audible flag `0x40` (`sensor_start_ambient_sound` `0x56d6a`, `timeline_event_sensor_ambient_sound` `0x5964e`, event `0x5a`).
- Trigger: the map is entered (`sensor_process_map_entry`) or the sensor turns on (`sensor_toggle_animation`). The event plays `(9 or 10, ornament, 2, 0x88)` and adds itself again after one animation cycle. Thus the sound repeats on a timer while the sensor is on and the party is on that map.
- Cycle: GDAT value `(category, ornament, 0x0b, 0x0d)` (low 15 bits) or the length of text `(category, ornament, 5, 0x0d)`. The unit is one game step. Value `(category, ornament, 0x0b, 0x88)` shifts the phase.
- The sound comes from the ornament square. A wall ornament sounds from the square in front of the wall.
- Ornaments with an animation cycle and a sound: wall 9, 41, 54, 55, 133; floor 8, 10, 24, 62, 67. Ornaments 41 and 24 have short click or zap sounds. The long loops are wall 9 (1.75 s), wall 55 (1.55 s), floor 8 (3.0 s, 60 steps), floor 62 (2.41 s), floor 10 (1.14 s), floor 67 (0.91 s), wall 54 (0.82 s).
- Not checked: which of these ornaments the DM2 dungeon places as audible `0x2c` sensors. This needs a `DUNGEON.DAT` scan.
- Other sounds that work as ambience:
  - Map scripts: `c3_i8_e00` (map 7, 1.06 s), `c3_i12_e00` (map 11, 2.68 s), `c3_i12_e01` (map 11, 4.95 s). A sound text on a square plays them once when a sensor sets it.
  - Weather: thunder `c23_i1_e00` (2.5 s) and the lightning strike `c13_i48_e81` (2.98 s), outdoor storms only (`world_update_day_night`).
  - Random floor text (text kind 9): it plays the floor ornament sound named by the text value when the random teleport does not happen.

## Footsteps

- Party: DM2 has no footstep sound. A party move plays no sound. Only a bump plays `(22, portrait, 0x8a)`.
- Floor sensors with the audible flag play the floor ornament sound when the party steps on them. Examples: grass `c10_i5_e88` and `c10_i58_e88` (0.21 s rustle, same data).
- Creatures: entry `0x00` is the move sound. The same sample is shared by groups of creatures:
'''

CANDIDATES = '''
## Candidates for DM1

The DM1 Amiga set has 35 events (`reference/extracted/AUDIO_REPORT.md`). DM1 has no ambient sounds, no party footsteps, no door slam, no thrown item sound, and no creature death or pain sounds.

### New sounds for DM1 (no DM1 event)

| DM2 file | Use in DM1 |
|---|---|
| `c10_i8_e88.wav` (3.0 s), `c10_i62_e88.wav`, `c10_i10_e88.wav`, `c10_i67_e88.wav`, `c9_i9_e88.wav`, `c9_i55_e88.wav`, `c9_i54_e88.wav` | Ambient loops for animated or special squares (for example fountains, pits, magic squares). Listen first: the GDAT does not name these ornaments. |
| `c3_i8_e00.wav`, `c3_i12_e00.wav`, `c3_i12_e01.wav` | One-shot ambient events (map scripts) |
| `c23_i1_e00.wav`, `c13_i48_e81.wav` | Thunder and lightning; DM1 has no weather. Low value |
| `c10_i5_e88.wav` | Footstep on a floor ornament or a pressure plate |
| Creature `0x00` move sounds | Footsteps of creatures that DM1 plays as one shared move sound |
| `c14_i254_e8f.wav` | Door closes fully (DM1 plays the same door sound for all door moves) |
| `c14_i254_e8d.wav` | Weapon hits a door |
| `c16_i254_e86.wav` | Item is thrown |
| `c22_i254_e87.wav` | Party falls into a pit (DM1 plays "party damaged") |
| `c15_*_e11` | Creature dies |
| `c15_*_e10`, `c15_*_e09`, `c15_*_e0a` | Creature is hurt |
| `c8_i254_e84.wav` | Click on a plain wall |
| `c3_i0_e88.wav` | Very short click for buttons and inventory |

### DM1 events that can use a DM2 sound

The `dm2` column of `compare/index.html` holds these matches. Keep the Amiga sound by default. The DM2 sound is an option.

| DM1 event | DM2 file |
|---|---|
'''

def write_md():
    rows = json.load(open(OUT + 'sounds.json'))
    import collections
    move = collections.OrderedDict()
    for x in rows:
        if x['cat'] == 15 and x['entry'] == 0:
            move.setdefault(x['sha'], []).append(x)
    t = [INTRO, FINDINGS]
    for grp in move.values():
        t.append('  - `%s` (%.2f s): %s\n' % (grp[0]['file'], grp[0]['sec'], ', '.join(g['name'] for g in grp)))
    t.append(CANDIDATES)
    for e, f in DM1_MATCH.items():
        t.append('| %d %s | `%s` |\n' % (e, DM1_NAMES[e], f))
    t.append('\nNo clear DM2 match: entrance door, weak explosion, war cry, blow horn, and the attack and move sounds of creatures that DM2 does not have (Screamer, Oitu, Scorpion, Pain Rat, Red Dragon, Water Elemental, Couatl, Animated Armour, Giant Wasp).\n')
    t.append('\n## All sounds\n\n| File | Category | Index | Entry | Name | Class | Length (s) | Trigger | Same as |\n|---|---|---|---|---|---|---|---|---|\n')
    for x in rows:
        t.append('| `%s` | %d %s | %d | `0x%02x` | %s | %s | %.2f | %s | %s |\n' % (x['file'], x['cat'], x['catname'], x['idx'], x['entry'],
                 x['name'], x['cls'], x['sec'], x['trigger'], '`%s`' % x['same'] if x['same'] else ''))
    open(OUT + 'SOUNDS.md', 'w').write(''.join(t))

if __name__ == '__main__':
    main()
    write_md()
