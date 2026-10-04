#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / "upstream" / "kh1"
URL = "https://github.com/ethteck/kh1.git"

if DEST.exists():
    print(f"Deja present: {DEST}")
    raise SystemExit(0)

print("Clone du depot de decompilation depuis son depot officiel...")
print("Le code upstream n'est pas redistribue dans ce starter.")
try:
    subprocess.run(["git", "clone", "--depth", "1", URL, str(DEST)], check=True)
except (OSError, subprocess.CalledProcessError) as e:
    print(f"Echec: {e}", file=sys.stderr)
    raise SystemExit(1)
