#!/usr/bin/env python3
from __future__ import annotations
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GAME = ROOT / "user_files" / "game"
KINGDOM = ROOT / "user_files" / "kingdom"

VERSIONS = {
    "SLPS_251.05": {
        "name": "Kingdom Hearts original japonais (PS2)",
        "sha1": "9dabbf867a7ec2a030df99ba1ed969f2deef0488",
    },
    "SLPS_251.98": {
        "name": "Kingdom Hearts Final Mix japonais (PS2)",
        "sha1": "e70bda789916142aafb53d85cef2e806b35ad8d8",
    },
}

def sha1(path: Path) -> str:
    h = hashlib.sha1()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()

report = {
    "recognized_elf": None,
    "elf_hash_ok": False,
    "kingdom_files": 0,
    "ready_for_analysis": False,
    "notes": [],
}

for filename, info in VERSIONS.items():
    path = GAME / filename
    if path.is_file():
        digest = sha1(path)
        report["recognized_elf"] = filename
        report["elf_sha1"] = digest
        report["elf_hash_ok"] = digest.lower() == info["sha1"].lower()
        report["game_version"] = info["name"]
        if not report["elf_hash_ok"]:
            report["notes"].append(
                f"Le SHA-1 de {filename} ne correspond pas a la version attendue."
            )
        break

if KINGDOM.is_dir():
    files = [p for p in KINGDOM.rglob("*") if p.is_file() and p.name != ".gitkeep"]
    report["kingdom_files"] = len(files)
else:
    report["notes"].append("Dossier user_files/kingdom absent.")

if report["recognized_elf"] is None:
    report["notes"].append("Ajoute SLPS_251.98 (recommande) ou SLPS_251.05 dans user_files/game/.")
if report["kingdom_files"] == 0:
    report["notes"].append("Ajoute le contenu extrait du dossier kingdom dans user_files/kingdom/.")

report["ready_for_analysis"] = bool(
    report["recognized_elf"] and report["elf_hash_ok"] and report["kingdom_files"] > 0
)

out = ROOT / "payload_report.json"
out.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

print(json.dumps(report, indent=2, ensure_ascii=False))
print(f"\nRapport ecrit: {out}")
raise SystemExit(0 if report["ready_for_analysis"] else 2)
