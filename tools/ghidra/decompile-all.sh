#!/bin/bash
# Decompile every FTL module in game/ to reference/decomp/dm/<MODULE>.c with Ghidra headless.
# docs/reference/<module>-symbols.csv, if it exists, names functions and globals.
set -euo pipefail
cd "$(dirname "$0")/../.."
mkdir -p reference/decomp/dm
pids=()
for ftl in game/*.FTL; do
  m=$(basename "$ftl" .FTL)
  args=$(uv run dm-ftl "$ftl" "reference/decomp/dm/$m.bin" | tail -1)
  SYMBOLS="docs/reference/$(echo "$m" | tr A-Z a-z)-symbols.csv" tools/ghidra/decompile.sh "reference/decomp/dm/$m.bin" 0x100000 "reference/decomp/dm/$m.c" $args &
  pids+=($!)
done
rc=0
for pid in "${pids[@]}"; do wait "$pid" || rc=1; done
exit $rc
