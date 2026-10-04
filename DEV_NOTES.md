# KH1 Vita - notes de développement, milestone 4

Validation effectuée contre les fichiers Final Mix fournis par l'utilisateur.

## Payload de référence

- `SLPS_251.98` SHA-1 : `e70bda789916142aafb53d85cef2e806b35ad8d8`
- `di08.ard` : valide
- modèle : `xa_ex_0010.mdls` — Sora in-game
- moveset : `xa_ex_0010.mset`

Aucun de ces fichiers n'est redistribué avec ce dépôt.

## MDLS / modèle Sora

- joints : 293
- meshes : 9
- textures : 7
- sous-paquets mesh : 95
- triangle strips : 1 177
- vertices : 5 182
- triangles : 2 828
- bind-pose bounds : approx. `<-69.559, -0.003, -26.876>` à `<69.559, 152.585, 31.053>`

Ordre local actuellement validé dans la base PS2 utilisée par le parseur :

`Translation * RotZ * RotY * RotX * Scale`

Puis :

`Global = ParentGlobal * Local`

## MSET / MMTN

Le premier bloc MMTN contient les offsets/tailles vers la section d'animations. La table de motions commence à `MMTN + 0x40` pour le fichier de Sora et se termine par `0xffffffff`.

### Motion 0 de `xa_ex_0010.mset`

Header observé :

- `+0x04` : `frame_count = 120`
- `+0x08` : `fps = 60.0`
- `+0x0c` : flags
- `+0x10` : `joint_count = 293`
- `+0x14` : taille/header = `0x50`
- `+0x18` : `channel_count = 154`
- `+0x1c` : offset table canaux
- `+0x20` : `aux_channel_count = 12`
- `+0x24` : offset canaux auxiliaires
- `+0x28` : offset keyframes/courbes
- `+0x2c` : `aux_joint_count = 11`
- `+0x30` : offset joints auxiliaires
- `+0x34` : `static_transform_count = 154`
- `+0x38` : offset transforms statiques
- `+0x3c` : offset de fin/section suivante

### Canal animé

6 octets :

```c
struct Channel {
    uint16_t joint_id;
    uint8_t  transform_type;
    uint8_t  key_count;
    uint16_t key_index;
};
```

La motion 0 utilise les transform types 4..6 sur la table principale et 7..9 sur une petite table auxiliaire.

### Keyframe

16 octets :

```c
struct Keyframe {
    uint16_t interpolation_raw;
    uint16_t frame;
    float value;
    float tangent_in;
    float tangent_out;
};
```

Les 154 canaux principaux de la motion 0 référencent **622 keyframes**. Le bloc de courbes observé a exactement la taille attendue pour ces 622 entrées.

Pour cette motion, le type d'interpolation utile est le type non flaggé `2`. Le milestone 4 l'évalue avec une Hermite cubique entre deux clés, avec la tangente de sortie de la clé courante et la tangente d'entrée de la clé suivante, mises à l'échelle par la durée du segment.

### Transform statique

8 octets :

```c
struct StaticTransform {
    uint16_t joint_id;
    uint16_t transform_type;
    float value;
};
```

Types :

- 1..3 : scale ;
- 4..6 : rotation ;
- 7..9 : translation.

Cette structure et cette famille de transform types recoupent le reverse-engineering public de KH1 dans Hypercrown.

### Clip RAM

`kh1_mset_load_clip()` charge les canaux, clés et transforms statiques une seule fois. `kh1_mset_evaluate_clip()` reconstruit une copie des joints pour une frame demandée. Le runtime Vita n'a donc pas à relire le `.mset` à chaque frame.

Test automatique de boucle, motion 0 :

- différence frame 0 vs frame 120 : `0`
- différence frame 0 vs frame 60 : non nulle (`~0.0864` dans le test courant)

La preview texturée montre une animation cohérente et une boucle correcte.

## Cas volontairement non interprétés

Certaines motions de Sora et d'autres MSET utilisent :

- des bits hauts dans `transform_type` ;
- des valeurs `interpolation_raw` avec flags hauts ;
- une petite rig/table auxiliaire.

Le milestone 4 les reconnaît/valide autant que possible, mais l'évaluateur retourne `KH1_MSET_UNSUPPORTED_CURVE` lorsqu'il ne connaît pas précisément la sémantique. Aucun flag n'est masqué silencieusement pour produire une animation possiblement fausse.

## Robustesse du corpus fourni

- MDLS : 459/500 passent le chemin single-weight ; 41 nécessitent le multi-weight.
- MSET : 580/580 passent maintenant la validation structurelle.
- les offsets optionnels `0xffffffff` sont acceptés lorsque leur compteur est nul ;
- les motions statiques à 0 frame sont acceptées.

## Validation locale

Chemins portables compilés en C11 avec `-Wall -Wextra -Werror`.

Tests actuels :

- ARD/MDLS/geometry/bind/texture : OK ;
- MSET Sora 118 motions : OK ;
- motion 0 : 154 canaux, 154 statiques, 622 clés : OK ;
- boucle 0 -> 120 : OK ;
- ASan/UBSan sur le test d'animation : OK lors de la validation milestone.

Le build VPK complet reste à vérifier avec VitaSDK réel.

## Sources publiques utilisées pour recouper le format

- Hypercrown — reverse-engineering KH1 MDLS/MSET : `https://github.com/Some1fromthedark/Hypercrown`
- OpenKH — outils/formats Kingdom Hearts : `https://github.com/OpenKH/OpenKh`
- VitaSDK headers GXM : `https://github.com/vitasdk/vita-headers/blob/master/include/psp2/gxm.h`
- VitaSDK samples : `https://github.com/vitasdk/samples`
- vita2dlib : `https://github.com/xerpi/libvita2d`

Ces sources servent de documentation/validation. Le port conserve sa propre couche C portable au lieu de recopier un renderer PS2.
