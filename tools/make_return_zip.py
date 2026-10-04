#!/usr/bin/env python3
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT.parent / "KH1Vita_with_user_files.zip"
SKIP_DIRS = {"build", ".git", "__pycache__"}
SKIP_SUFFIX = {".vpk", ".elf", ".velf", ".o"}

with zipfile.ZipFile(OUT, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as z:
    for p in ROOT.rglob("*"):
        rel = p.relative_to(ROOT)
        if any(part in SKIP_DIRS for part in rel.parts):
            continue
        if p.is_file() and p.suffix.lower() not in SKIP_SUFFIX:
            z.write(p, Path(ROOT.name) / rel)
print(OUT)
