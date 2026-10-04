#!/usr/bin/env bash
set -euo pipefail
if [ "$#" -ne 2 ]; then echo "usage: $0 /path/to/model.mdls out.ppm" >&2; exit 2; fi
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/kh1vita_textured_preview"
cc -std=c11 -Wall -Wextra -Werror -I"$ROOT/include" \
  "$ROOT/src/kh1_mdls.c" "$ROOT/src/kh1_mdls_geometry.c" \
  "$ROOT/src/kh1_mdls_skeleton.c" "$ROOT/src/kh1_mdls_texture.c" \
  "$ROOT/src/software_rasterizer.c" "$ROOT/tools/host_textured_preview.c" \
  -lm -o "$OUT"
"$OUT" "$1" "$2"
