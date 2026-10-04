# Plan de migration du renderer CPU vers SceGxm

Le rasterizer CPU est utile comme oracle de validation, mais pas comme renderer final du jeu. La prochaine étape est de garder exactement les mêmes données décodées (vertices, indices, UV, textures RGBA, pose) et de remplacer seulement le backend de rendu.

## 1. Initialisation GXM

VitaSDK expose GXM via :

```c
#include <psp2/gxm.h>
```

et le binaire doit lier `SceGxm_stub`.

Le chemin de départ suivra le sample VitaSDK actuel :

1. `sceGxmInitialize()` ;
2. deux display buffers 960x544 en CDRAM ;
3. `sceGxmMapMemory()` ;
4. `sceGxmColorSurfaceInit()` ;
5. un `SceGxmSyncObject` par buffer ;
6. display queue + callback `sceDisplaySetFrameBuf()`.

Le framebuffer CPU milestone 4 reste disponible derrière une interface séparée pour comparer visuellement les deux backends.

## 2. Render target / context

Ajouter ensuite :

- `SceGxmRenderTarget` ;
- `SceGxmContext` ;
- depth/stencil surface ;
- début/fin de scène ;
- viewport/culling/depth state.

Au début : pas de MSAA, pas d'effets, une seule passe opaque + alpha simple.

## 3. Vertex/index buffers

Format cible de départ :

```c
struct GxmVertex {
    float x, y, z;
    float u, v;
};
```

Le décodeur MDLS produit déjà ces informations. Les triangle strips sont déjà convertis en triangles CPU ; pour le premier backend GXM, on peut donc uploader un index buffer U16/U32 sans reproduire la logique de strip PS2.

Objectif : un seul upload statique des coordonnées locales/index, puis mise à jour des positions animées. Quand le skinning multi-weight/GPU sera prêt, on pourra déplacer davantage de travail dans le vertex shader.

## 4. Textures

Le milestone 4 décode déjà les textures PS2 en RGBA8888. Le premier backend GXM utilisera directement cette sortie comme texture linéaire, afin d'éviter de mélanger deux problèmes à la fois (décodage GS + sampling GPU).

Une fois le rendu validé, on pourra étudier une voie plus compacte pour limiter RAM/bande passante.

## 5. Shaders

Créer un shader minimal :

- vertex : position transformée + UV ;
- fragment : texture * couleur blanche ;
- alpha simple ;
- depth test/write.

La création/destruction des programmes et le shader patcher doivent suivre un lifecycle complet, en s'appuyant sur les headers VitaSDK et sur un projet public mature comme vita2dlib pour les détails de gestion de ressources.

## 6. Ordre de validation

1. triangle coloré ;
2. triangle texturé ;
3. texture de Sora seule ;
4. Sora bind-pose ;
5. Sora motion 0 ;
6. comparaison screenshot CPU/GXM ;
7. seulement ensuite : multi-weight, davantage de motions, rooms.

## 7. Critères avant de supprimer le rasterizer CPU

Ne pas retirer le backend logiciel avant que :

- la silhouette GXM corresponde au CPU ;
- les UV/textures correspondent ;
- le z-order soit identique ;
- la motion 0 boucle correctement ;
- les erreurs de ressources GXM soient propres à l'init/term.

Le renderer CPU sert donc de référence de vérité pendant toute la migration.
