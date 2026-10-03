#!/bin/bash
# Decompile every FTL module in game/ to decomp/<MODULE>.c with Ghidra headless.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p decomp
pids=()
for ftl in game/*.FTL; do
  m=$(basename "$ftl" .FTL)
  args=$(uv run dm-ftl "$ftl" "decomp/$m.bin" | tail -1)
  tools/ghidra/decompile.sh "decomp/$m.bin" 0x100000 "decomp/$m.c" $args &
  pids+=($!)
done
rc=0
for pid in "${pids[@]}"; do wait "$pid" || rc=1; done
exit $rc
