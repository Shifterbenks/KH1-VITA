#!/usr/bin/env bash
set -euo pipefail
if [ "$#" -ne 1 ]; then echo "usage: $0 /path/to/xa_ex_0010.mset" >&2; exit 2; fi
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/kh1vita_host_mset_test"
cc -std=c11 -Wall -Wextra -Werror -I"$ROOT/include" \
  "$ROOT/src/kh1_mset.c" "$ROOT/tools/host_mset_test.c" -lm -o "$OUT"
"$OUT" "$1"
