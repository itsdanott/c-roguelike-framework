/*

Make sure to define the Game struct in the header as it is also a member
of the framework's App struct.

*/

#ifndef GAME_H
#define GAME_H

#include <SDL3/SDL.h>
#include "../c_roguelike_framework.h"

#if !defined(bool)
#define bool	_Bool
#define true	1
#define false	0
#endif

/* PUBLIC GAME TYPES **********************************************************/
typedef struct {
    float pos_x;
    float pos_y;
} Player;

typedef struct {
    Player player;
    vec2 test_vec2;
} Game;

static Game default_game() {
    return (Game) {
    .test_vec2 = {0},
    };
}

/* PUBLIC GAME API ************************************************************/
bool game_init(CRLF_API* new_api, Game* game);
void game_tick(Game* game, float dt);
void game_draw(Game* game);
void game_cleanup(Game* game);

#endif //GAME_H
