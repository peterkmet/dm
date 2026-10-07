import struct,wave,os,gdat,dec
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '')  # the repository root
R=ROOT; E=R+'reference/extracted/'
PAL=3546895
NAMES={671:'metallic_thud',672:'switch',673:'door',674:'wooden_thud_wall_trolin_golem_attack',675:'explosion',
677:'scream_death',678:'swallow',679:'champion_wounded_1',680:'champion_wounded_2',681:'champion_wounded_3',682:'champion_wounded_4',
683:'spell_explosion',684:'swing_swoosh',685:'buzz_teleport_fuse',687:'party_damaged_bump',688:'attack_painrat_dragon',
689:'attack_ghost_mummy',690:'attack_screamer_oitu',691:'attack_scorpion',692:'attack_magenta_worm',693:'attack_giggler',
701:'move_animated_armour',702:'move_couatl_wasp',703:'move_golem_mummy_trolin_giggler_vexirk_demon',704:'blow_horn',
705:'move_scorpion_rat_ruster_screamer_rockpile_worm_oitu',706:'move_slime_water_elemental',707:'war_cry',708:'attack_rockpile',
709:'attack_water_elemental',710:'attack_couatl',711:'move_red_dragon',712:'move_skeleton'}
SOUNDS=['metallic_thud','switch','door','entrance_door','wooden_thud','strong_explosion','weak_explosion','scream','swallow',
'champion_0_damaged','champion_1_damaged','champion_2_damaged','champion_3_damaged','swing','buzz','party_damaged','spell',
'war_cry','blow_horn','attack_screamer_oitu','attack_scorpion','attack_magenta_worm','attack_giggler','attack_painrat_dragon',
'attack_rockpile','attack_ghost_mummy','attack_water_elemental','attack_couatl','move_animated_armour','move_couatl_wasp',
'move_golem_mummy_trolin_giggler_vexirk_demon','move_scorpion_rat_ruster_screamer_rockpile_worm_oitu','move_slime_water_elemental',
'move_red_dragon','move_skeleton']
def wav(path,rate,data,width=1):
    os.makedirs(os.path.dirname(path),exist_ok=True)
    w=wave.open(path,'wb'); w.setnchannels(1); w.setsampwidth(width); w.setframerate(int(round(rate))); w.writeframes(data); w.close()
s8=lambda b: bytes((x+128)&255 for x in b)          # signed 8-bit -> WAV unsigned
SND=list(range(671,694))+list(range(701,713)); SND=[i for i in SND if i not in(676,686)]
n=0
# Amiga 3.60: every sound table entry at its own rate
am=gdat.parse(R+'game/Graphics.DAT')['items']; kaos=open(R+'decomp/KAOS.bin','rb').read()
for k in range(35):
    g,_,rate=struct.unpack('>hBB',kaos[0xd52+12*k:0xd56+12*k])
    hz=PAL/(72800//rate)
    wav(E+'amiga-3.60/sounds/s%02d_%s_g%d_%dHz.wav'%(k,SOUNDS[k],g,hz),hz,s8(am[g][2:])); n+=1
sw=open(R+'decomp/SWSH.bin','rb').read(); per,ln=struct.unpack('>HI',sw[0xa54:0xa5a])
wav(E+'amiga-3.60/sounds/ftl_logo_swoosh_%dHz.wav'%(PAL/per),PAL/per,s8(sw[0xa5a:0xa5a+ln])); n+=1
# DOS 3.4: unsigned 8-bit, 6000 Hz
do=gdat.parse(E+'Game,Dungeon_Master,DOS,Software/DungeonMasterPC34/DATA/GRAPHICS.DAT')['items']
for g in SND: wav(E+'dos-3.4/sounds/g%d_%s.wav'%(g,NAMES[g]),6000,do[g][2:]); n+=1
# DOS SONG.DAT: SND8 parts at 11025 Hz, SEQ2 sequence
so=gdat.parse(E+'Game,Dungeon_Master,DOS,Software/DungeonMasterPC34/DATA/SONG.DAT')['items']
parts={}
for k in range(1,10):
    o,_=dec.snd8(so[k]); parts[k]=bytes(max(0,min(255,v+128)) for v in o)
    wav(E+'dos-3.4/music/song_part_%d.wav'%k,11025,parts[k]); n+=1
seq=[]
for (w,) in struct.iter_unpack('<H',so[0]):
    seq.append(w&0x7fff)
    if w&0x8000: break
wav(E+'dos-3.4/music/entrance_song_one_loop.wav',11025,b''.join(parts[p] for p in seq)); n+=1
print('song seq',seq)
# Atari ST v1.1: SND1 4-bit RLE
st=gdat.parse(E+'Game,Dungeon_Master,Atari_ST,Software/HardDisk/2009-02-22 PP/GRAPHICS.DAT')['items']
for g in SND[:21]:
    o,_,_=dec.snd1(st[g-138]); wav(E+'atari-st-1.1/sounds/g%d_%s.wav'%(g,NAMES[g]),5486,o); n+=1
o,_,_=dec.snd1(st[535]); wav(E+'atari-st-1.1/sounds/g673_entrance_door_4237Hz.wav',4237,o); n+=1
# X68000 3.0: OKI/Dialogic ADPCM, low nibble first, ~7812.5 Hz
x=gdat.parse(E+'x68000/files/GRAPHICS.DAT')['items']
for g in SND[:21]:
    o=dec.oki(x[g-138][2:],False); wav(E+'x68000/sounds/g%d_%s.wav'%(g,NAMES[g]),7812.5,struct.pack('<%dh'%len(o),*[v*16 for v in o]),2); n+=1
# CSB Amiga: Sound/*.amg, SD chunks of ENDA.DAT and ANIM.DAT, NAKED.AMG music
for f in sorted(os.listdir(R+'game-csb/disk-b/Sound')):
    b=open(R+'game-csb/disk-b/Sound/'+f,'rb').read(); wav(E+'csb-amiga/sounds/%s_5457Hz.wav'%f[:-4],PAL/650,s8(b[2:])); n+=1
for name,path in (('enda',R+'game-csb/ENDA.DAT'),('anim',R+'game-csb/disk-b/ANIM.DAT'),('dm_titl',R+'game/TITL.DAT')):
    b=open(path,'rb').read(); p=0; k=0
    while p+6<=len(b):
        tag=b[p:p+2]; ln=struct.unpack('>H',b[p+2:p+4])[0]
        if tag==b'SD':
            k+=1; pl=b[p+6:p+6+ln]; L=struct.unpack('>H',pl[:2])[0]
            wav(E+'csb-amiga/sounds/%s_sd%d_5457Hz.wav'%(name,k),PAL/650,s8(pl[2:2+L])); n+=1
        p+=6+ln
    print(name,'SD chunks',k)
na=gdat.parse(R+'game-csb/disk-b/MIDI/NAKED.AMG')['items']
seq=[c for c in na[0] if c!=0xff]
wav(E+'csb-amiga/music/naked_amg_one_pass_10814Hz.wav',PAL/328,b''.join(s8(na[c][2:]) for c in seq)); n+=1
print('naked seq',seq,'files',n)
