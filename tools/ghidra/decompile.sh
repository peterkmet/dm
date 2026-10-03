#!/bin/bash
# Decompile a flat 68000 binary with Ghidra headless.
# Usage: [SYMBOLS=<symbols.csv>] decompile.sh <image.bin> <base-address> <output.c> [REG=<value>] [<entry> ...]
set -euo pipefail
GHIDRA=$(brew --prefix ghidra)/libexec
export JAVA_HOME="${JAVA_HOME:-$(brew --prefix openjdk@21)/libexec/openjdk.jdk/Contents/Home}"
out="$(cd "$(dirname "$3")" && pwd)/$(basename "$3")"
proj=$(mktemp -d)
trap 'rm -rf "$proj"' EXIT
"$GHIDRA/support/analyzeHeadless" "$proj" p -import "$1" \
  -loader BinaryLoader -loader-baseAddr "$2" -processor 68000:BE:32:default \
  -scriptPath "$(realpath "$(dirname "$0")")" -preScript Entries.java "${@:4}" -postScript ApplySymbols.java "${SYMBOLS:-}" -postScript InferParams.java -postScript ExportC.java "$out" >"$out.log" 2>&1 \
  || { tail -20 "$out.log"; exit 1; }
