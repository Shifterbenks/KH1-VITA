# Test sur une vraie PS Vita

Ce jalon vise un test visuel minimal : charger Sora depuis les fichiers KH1 de l'utilisateur, décoder son MDLS/MSET et afficher l'animation motion 0 via le renderer logiciel.

## Fichiers de jeu requis

Copier uniquement ces fichiers, extraits depuis votre propre copie du jeu, vers :

`ux0:data/KH1VITA/kingdom/`

- `di08.ard`
- `xa_ex_0010.mdls`
- `xa_ex_0010.mset`

L'ELF `SLPS_251.98` n'est plus requis pour ce test précis.

## Contrôles

- START : quitter le test.
- CROIX : rescanner les fichiers si vous les avez copiés après avoir lancé l'application.

## Écran de statut

Au démarrage, deux grandes barres sont affichées :

- première barre verte : les 3 assets de test Sora sont présents ; rouge : au moins un manque ;
- deuxième barre verte : le dossier `kingdom` contient des données ; orange : il est vide.

Si tout est valide, le chargement de `di08.ard`, `xa_ex_0010.mdls` et `xa_ex_0010.mset` démarre et Sora doit apparaître texturé et animé.

Le log détaillé est écrit dans :

`ux0:data/KH1VITA/port.log`

## Build local

Avec VitaSDK 2026.08 installé :

```bash
export VITASDK=/usr/local/vitasdk
export PATH="$VITASDK/bin:$PATH"
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Le résultat attendu est `build/KH1Vita_Test.vpk`.

## Build GitHub Actions

Le workflow `.github/workflows/build-vita.yml` utilise l'image officielle `vitasdk/vitasdk:2026.08` et publie `KH1Vita_Test.vpk` comme artifact `KH1Vita-Test-VPK`.
