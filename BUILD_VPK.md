# Produire KH1Vita_Test.vpk

La méthode recommandée est VitaSDK 2026.08.

## Option 1 — GitHub Actions

1. Mettre ce dossier dans un dépôt GitHub.
2. Ouvrir **Actions > Build Vita VPK > Run workflow**.
3. Quand le job est terminé, télécharger l'artifact **KH1Vita-Test-VPK**.
4. Il contient `KH1Vita_Test.vpk`.

Aucun fichier du jeu n'est inclus dans le VPK ni envoyé au build CI.

## Option 2 — VitaSDK local

```bash
export VITASDK=/usr/local/vitasdk
export PATH="$VITASDK/bin:$PATH"
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Voir `docs/VITA_TEST.md` pour les trois fichiers de données à copier sur la Vita.
