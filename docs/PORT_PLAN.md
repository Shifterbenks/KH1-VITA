# Plan de portage

## Etape 0 — bootstrap Vita

Projet VitaSDK/CMake, framebuffer, contrôles, I/O `ux0:data/KH1VITA/`, validation payload et logs.

## Etape 1 — formats de base

ARD, MDLS, sections textures, sélection des ressources de room.

## Etape 2 — géométrie et squelette

Mesh packets single-weight, strips, vertices/UV, matrix-slot -> joint, hiérarchie SRT et bind pose.

## Etape 3 — milestone actuel

- CLUT/texture -> RGBA8888 ;
- rasterizer CPU texturé + z-buffer ;
- preview de Sora validée sur PC ;
- chemin framebuffer Vita branché sur ce rasterizer ;
- parser MSET/MMTN générique ;
- 118 motions de `xa_ex_0010.mset` reconnues ;
- correspondance 293 joints MDLS/MSET validée.

## Etape 4

1. valider le VPK sur VitaSDK/Vita ;
2. décoder les paquets MDLS multi-weight ;
3. documenter les tables de canaux/courbes MSET ;
4. évaluer une première motion de Sora ;
5. faire le skinning animé sur CPU ;
6. migrer rendu/vertex buffers/textures vers GXM.

## Etape 5

Room complète, caméra/input, objets supplémentaires, audio et intégration progressive de logique décompilée portable.

La décompilation upstream vise la reconstruction du binaire MIPS PS2. Le port Vita garde donc une séparation stricte entre logique portable et backend plateforme.
