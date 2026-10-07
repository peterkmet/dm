#!/bin/bash
# Decompile every FTL module in game/ to decomp/dm/<MODULE>.c with Ghidra headless.
# docs/<module>-symbols.csv, if it exists, names functions and globals.
set -euo pipefail
cd "$(dirname "$0")/../.."
mkdir -p decomp/dm
pids=()
for ftl in game/*.FTL; do
  m=$(basename "$ftl" .FTL)
  args=$(uv run dm-ftl "$ftl" "decomp/dm/$m.bin" | tail -1)
  SYMBOLS="docs/$(echo "$m" | tr A-Z a-z)-symbols.csv" decomp/tools/ghidra/decompile.sh "decomp/dm/$m.bin" 0x100000 "decomp/dm/$m.c" $args &
  pids+=($!)
done
rc=0
for pid in "${pids[@]}"; do wait "$pid" || rc=1; done
exit $rc
