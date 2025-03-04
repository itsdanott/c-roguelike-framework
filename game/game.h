/*
Roguelike Game Lib Template
The game.h and game.c files serve as a template for gameplay implementation.

# SDL
Never call SDL functions besides the standard C runtime function replacements
(Stdinc) from inside the game lib!

The framework handles all the SDL related calls - the SDL dependency in the game
is only required to make sure the Stdinc functions are available.

*/

#ifndef GAME_H
#define GAME_H

#include <SDL3/SDL.h>
#include "../c_roguelike_framework.h"

/* PUBLIC GAME TYPES **********************************************************/
typedef struct {
    float pos_x;
    float pos_y;
} Player;

// Make sure to define the Game struct in the header as it is also a member
// of the framework's App struct.
typedef struct {
    Player player;
    vec2 test_vec2;
} Game;

static Game default_game() {
    return (Game){
        .test_vec2 = {0},
    };
}

/* PUBLIC GAME API ************************************************************/
bool game_init(CRLF_API* new_api, Game* game, UI_Context* ui);
void game_tick(Game* game, float dt);
void game_draw(Game* game);
void game_cleanup(Game* game);

#endif //GAME_H
