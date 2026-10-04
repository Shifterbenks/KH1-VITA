#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 2 ]; then
  echo "usage: $0 /path/to/di08.ard /path/to/xa_ex_0010.mdls" >&2
  exit 2
fi

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/kh1vita_host_all_test"

cc -std=c11 -Wall -Wextra -Werror \
  -I"$ROOT/include" \
  "$ROOT/src/kh1_ard.c" \
  "$ROOT/src/kh1_mdls.c" \
  "$ROOT/src/kh1_mdls_geometry.c" \
  "$ROOT/src/kh1_mdls_skeleton.c" \
  "$ROOT/src/kh1_mdls_texture.c" \
  "$ROOT/tools/host_all_test.c" \
  -lm -o "$OUT"

"$OUT" "$1" "$2"
