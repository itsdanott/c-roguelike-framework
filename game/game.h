#ifndef GAME_H
#define GAME_H

#include "../c_roguelike_framework.h"

#if !defined(bool)
#define bool	_Bool
#define true	1
#define false	0
#endif

typedef struct {
	float pos_x;
	float pos_y;
} Player;

typedef struct {
	Player player;
} Game;

bool game_init(CRLF_API* new_api);
void game_tick(Game* game);
void game_draw();
void game_cleanup();

#endif //GAME_H
