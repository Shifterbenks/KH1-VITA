#pragma once

/*
 * Point d'integration du futur port du code decompile.
 * Pour l'instant, le starter ne tente PAS d'executer le code MIPS PS2.
 */
int kh1vita_game_bridge_boot(void);
void kh1vita_game_bridge_tick(void);
void kh1vita_game_bridge_shutdown(void);
