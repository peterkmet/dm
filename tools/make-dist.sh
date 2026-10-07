#!/bin/bash
# Build build/DungeonMaster.zip: the game, SDL3, game/ and game-csb/ without music and saves.
set -euo pipefail
cd "$(dirname "$0")/.."
make -C port
out=build/dist/DungeonMaster
rm -rf build/dist build/DungeonMaster.zip
mkdir -p "$out"
for d in game game-csb; do
  rsync -a --exclude music --exclude .DS_Store --exclude '*GAME.DAT' --exclude '*GAME.BAK' --exclude QUICK.DAT "$d/" "$out/$d/"
done
cp build/dm "$out/dm"
sdl="$(brew --prefix sdl3)/lib/libSDL3.0.dylib"
cp -L "$sdl" "$out/libSDL3.0.dylib"
chmod u+w "$out/libSDL3.0.dylib"
install_name_tool -id @executable_path/libSDL3.0.dylib "$out/libSDL3.0.dylib"
install_name_tool -change "$(otool -L build/dm | awk '/libSDL3/ {print $1}')" @executable_path/libSDL3.0.dylib "$out/dm"
codesign -f -s - "$out/libSDL3.0.dylib" "$out/dm"
cat > "$out/Dungeon Master.command" <<'EOF'
#!/bin/bash
cd "$(dirname "$0")" && exec ./dm
EOF
chmod +x "$out/Dungeon Master.command"
cat > "$out/README.txt" <<'EOF'
Dungeon Master and Chaos Strikes Back for macOS (Apple Silicon).

1. Open Terminal in this folder and remove the quarantine:
   xattr -d com.apple.quarantine dm "Dungeon Master.command" libSDL3.0.dylib
2. Double-click "Dungeon Master.command".

The saved games go to game/ (Dungeon Master) and game-csb/ (Chaos Strikes Back) in this folder.
EOF
(cd build/dist && ditto -c -k --norsrc --keepParent DungeonMaster ../DungeonMaster.zip)
rm -rf build/dist
ls -lh build/DungeonMaster.zip
