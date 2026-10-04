#!/usr/bin/env bash
set -euo pipefail
if [ "$#" -ne 2 ]; then echo "usage: $0 xa_ex_0010.mdls xa_ex_0010.mset" >&2; exit 2; fi
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/kh1vita_host_animation_test"
cc -std=c11 -Wall -Wextra -Werror -I"$ROOT/include" \
  "$ROOT/src/kh1_mdls.c" "$ROOT/src/kh1_mdls_skeleton.c" "$ROOT/src/kh1_mset.c" \
  "$ROOT/tools/host_animation_test.c" -lm -o "$OUT"
"$OUT" "$1" "$2"
