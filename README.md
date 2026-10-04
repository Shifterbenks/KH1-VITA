
## Milestone 5 — test réel Vita

Ce dossier est préparé pour produire **`KH1Vita_Test.vpk`** avec VitaSDK 2026.08. Le test n'exige plus l'ELF PS2 : seuls `di08.ard`, `xa_ex_0010.mdls` et `xa_ex_0010.mset` sont nécessaires sur la Vita. Voir `docs/VITA_TEST.md` et `BUILD_VPK.md`.

# KH1 Vita Port — milestone 4: Sora animé

Prototype de port natif de **Kingdom Hearts Final Mix PS2** vers PS Vita.

Cette version ne lance pas encore le jeu complet. Le pipeline portable sait maintenant charger le modèle in-game de Sora (`xa_ex_0010.mdls`), reconstruire son squelette et ses textures, puis lire une vraie motion de `xa_ex_0010.mset` et mettre à jour la pose image par image :

`ARD -> MDLS -> mesh packets -> squelette -> UV/CLUT -> MSET -> courbes -> pose animée -> triangles texturés`

Le code n'embarque aucun fichier du jeu.

## Données attendues sur la Vita

Copie tes fichiers extraits dans :

- `ux0:data/KH1VITA/game/SLPS_251.98`
- `ux0:data/KH1VITA/kingdom/`

Au démarrage, le programme ouvre `kingdom/di08.ard`, sélectionne `xa_ex_0010.mdls`, charge le `.mset` correspondant et joue la **motion 0** de Sora en boucle avec le rasterizer logiciel actuel.

## Validation actuelle avec Sora

`xa_ex_0010.mdls` :

- 293 joints ;
- 9 meshes ;
- 7 textures ;
- 5 182 vertices ;
- 2 828 triangles.

`xa_ex_0010.mset` :

- 118 motions ;
- motion 0 : 120 frames à 60 Hz ;
- 154 canaux animés principaux ;
- 154 transforms statiques ;
- 622 keyframes référencées ;
- frame 0 et frame 120 donnent la même pose ;
- une frame intermédiaire produit bien une pose différente.

Une preview hôte issue des vrais fichiers de Sora a été vérifiée visuellement : l'animation reste cohérente et le modèle revient à sa pose de départ à la fin de la boucle.

## Ce qui est maintenant décodé

- ARD et liste des ressources ;
- header/sections MDLS ;
- paquets de mesh single-weight ;
- triangle strips -> triangles ;
- vertices, UV et joint principal ;
- squelette SRT + hiérarchie parent/enfant ;
- textures PS2 indexées 8-bit + CLUT -> RGBA8888 ;
- rasterizer CPU texturé + z-buffer ;
- conteneur MSET/MMTN et table de motions ;
- table de transforms statiques MSET ;
- canaux de motion de 6 octets ;
- keyframes de 16 octets ;
- interpolation Hermite du format non flaggé utilisé par la motion 0 ;
- clip animation chargé en RAM, sans relire le `.mset` à chaque frame ;
- reconstruction de la pose globale puis skinning single-weight en temps réel.

## Couverture des assets fournis

### MDLS

Scan : **500 fichiers**.

- 459 passent le chemin single-weight actuel ;
- 41 contiennent au moins un paquet multi-weight type 2, détecté mais pas encore décodé.

### MSET

Scan : **580 fichiers**.

- 580/580 passent la validation structurelle du conteneur avec le parseur milestone 4 ;
- certains petits MSET ont des sections absentes marquées `0xffffffff` ou des motions statiques à 0 frame : ces cas sont maintenant acceptés proprement.

Cela ne signifie pas encore que toutes les 580 animations sont évaluables : certaines motions utilisent des flags/variantes de courbe supplémentaires volontairement laissés en `unsupported` plutôt que devinés.

## Build VitaSDK

Linux/macOS :

```sh
export VITASDK=/usr/local/vitasdk
./build_vita.sh
```

PowerShell :

```powershell
$env:VITASDK = "C:\chemin\vers\vitasdk"
.\build_vita.ps1
```

Le VPK généré est `build/KH1VITA.vpk`.

Le build Vita complet reste à tester sur une vraie installation VitaSDK ; les parties portables et le bridge sont testées côté hôte.

## Tests hôte

Parseurs MDLS/ARD :

```sh
./tools/test_host.sh /chemin/vers/di08.ard /chemin/vers/xa_ex_0010.mdls
```

MSET :

```sh
./tools/test_mset.sh /chemin/vers/xa_ex_0010.mset
```

Animation motion 0 :

```sh
./tools/test_animation.sh /chemin/vers/xa_ex_0010.mdls /chemin/vers/xa_ex_0010.mset
```

Preview animée hôte : compiler `tools/host_animated_preview.c` avec les sources portables ou s'en servir comme référence de test.

## Prochaines étapes

1. tester le VPK milestone 4 sur Vita/Vita3K ;
2. décoder les variantes flaggées des courbes MSET ;
3. décoder les paquets MDLS multi-weight ;
4. remplacer le rasterizer CPU par un renderer GXM ;
5. passer les textures décodées et les vertex/index buffers au GPU ;
6. charger une room complète, puis les objets et la logique de jeu progressivement.

Voir `docs/GXM_PLAN.md` et `DEV_NOTES.md` pour les détails techniques.
