# Music for the advanced mode: decisions

Decisions of the user for the music feature, before the PRD (workflow step 2). The next session writes the PRD from this file.

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

## Open for the PRD

- The Super NES rule when a new trigger square comes while a track plays (stop it, or wait).
- Where the port looks for the MP3 files (for example `game/music/`), and the file names.
- The special tracks: entrance, Hall of Champions, game over, game won, CSB prison, story and ending animations (tracks in `MUSIC_MAP.md`).
- How the CSB square table and the trigger lists go into the port (a data table in the source: the table is facts, not a game asset).
