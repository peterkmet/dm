# Music for the advanced mode: decisions

Decisions of the user for the music feature. The feature is implemented from this file, without a PRD.

## Sources

- The research is in `origs/extracted/AUDIO_REPORT.md` and `origs/extracted/MUSIC_MAP.md`, with the machine-readable mapping in `origs/extracted/music_map.json` (all git-ignored, local only).
- The MP3 files are in `music/` (git-ignored): `Game_Dungeon_Master_JP/` (FM-Towns DM), `Game_Chaos_Strikes_Back_JP/` (FM-Towns CSB), `Game_Dungeon_Master_US/` (Super NES DM), `Game_Dungeon_Master_Super_NES/` (Super NES swoosh).

## Decisions

1. Soundtracks: FM-Towns and Super NES as a choice, and "Original" (no music).
2. Format: all MP3. Decoder: `dr_mp3` (one header file, public domain).
3. FM-Towns MP3s: the existing 128 kbit/s files from `music/`. No new encoding.
4. Super NES: tracks start on the trigger squares of the Super NES version, converted to the map-relative squares of the Amiga dungeon (`music_map.json`).
5. Only in the advanced mode. On the automap, keys 1, 2, and 3 select Original, Super NES, and FM-Towns. The default is Original. The choice is not saved (as the cheats).
6. Chaos Strikes Back: FM-Towns CSB by square, from the table of the FM-Towns program (`MUSIC_MAP.md`). Key 2 does nothing in CSB (there is no Super NES CSB).
7. Playback as the original: a track plays one time and does not loop. FM-Towns DM: a new level starts its track and stops the current one. FM-Towns CSB: a new zone requests its track, which starts when the current track ends; a level change stops the music. Value 0 in the CSB table means no change.

8. Super NES: a new trigger square stops the current track and starts its track. A trigger of the track that plays does nothing.
9. Files: one flat folder `music/` in the disk A folder (`game/music/`, `game-csb/music/`), with the file names of `music/`: `FM-Towns,Track_NN,Music.mp3`, `Super_NES,Track_NN,Music.mp3`. A missing file gives silence.
10. Special tracks: only the tracks in the game. CSB prison (FM-Towns 06), game over (DM FM-Towns 13, CSB FM-Towns 17), game won (DM FM-Towns 18, Super NES 16). No entrance, title, story, or CSB ending track: they come before the choice, or the port has no FM-Towns animation.
11. Data: `port/src/musicmap.c` holds the CSB square table, generated one time from `music_map.json`. The DM level tracks and the 22 Super NES triggers are in `port/src/music.c`.
12. The return to the entrance or the CSB screen stops the music. A game over or game won track plays over the credits.

## Not done

- The countdown of 100 calls before a track starts (FM-Towns DM and CSB): a track starts at once.
- The freeze does not pause the music.
