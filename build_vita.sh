#!/usr/bin/env bash
set -euo pipefail
: "${VITASDK:?Definis VITASDK, ex: export VITASDK=/usr/local/vitasdk}"
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
printf '\nVPK: build/KH1VITA.vpk\n'
