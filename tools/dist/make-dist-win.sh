#!/bin/bash
# Build build/DungeonMaster-win.zip: the Windows game (x86-64) and SDL3. The player adds the game disk images.
set -euo pipefail
cd "$(dirname "$0")/../.."
make windows
sdl_version="$(awk -F' = ' '/^SDL_MINGW_VERSION = / {print $2}' Makefile)"
out=build/dist-win/DungeonMaster
rm -rf build/dist-win build/DungeonMaster-win.zip
mkdir -p "$out"
x86_64-w64-mingw32-strip -o "$out/dm.exe" build/dm.exe
cp "build/sdl-mingw/SDL3-$sdl_version/x86_64-w64-mingw32/bin/SDL3.dll" "$out/SDL3.dll"
cat > "$out/README.txt" <<'EOF'
Dungeon Master and Chaos Strikes Back for Windows (64-bit, Windows 10 or later).

The game files are not included. Copy your Amiga disk images (.adf) into this folder:
- Dungeon Master v3.60 (one disk).
- Optional: Chaos Strikes Back for the Amiga, disks A and B.
At the first start the game extracts them into game\ and game-csb\. Disks of other
versions (DOS, Atari ST, other Amiga versions) do not work: the game names the wrong file.

Double-click dm.exe. Windows can warn about an unknown publisher: click "More info",
then "Run anyway". The console window shows the messages of the game.

The LLM mode is not available on Windows.

The saved games go to game\ (Dungeon Master) and game-csb\ (Chaos Strikes Back) in this folder.
EOF
sed -i '' 's/$/\r/' "$out/README.txt"
(cd build/dist-win && zip -qr ../DungeonMaster-win.zip DungeonMaster)
rm -rf build/dist-win
ls -lh build/DungeonMaster-win.zip
