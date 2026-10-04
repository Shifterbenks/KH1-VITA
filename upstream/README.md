# Upstream KH1 decomp

Ce starter **ne redistribue pas** le code du dépôt de décompilation.

Quand on passera à l'intégration, `tools/fetch_upstream.py` peut cloner le dépôt public
`ethteck/kh1` dans `upstream/kh1/`.

Le dépôt upstream cible encore le binaire principal PS2/MIPS : il faudra donc isoler et
porter les fonctions de gameplay utilisables, puis remplacer les couches PS2 (rendu,
audio, I/O, timing, etc.) par des implémentations Vita.
