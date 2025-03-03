#include "game.h"

static CRLF_API* api;

bool game_init(CRLF_API* new_api, Game* game) {
    api = new_api;
    *game = default_game();

    const char* name = "ROGUELIKE";
    api->CRLF_Log("Hello World from the %s game!", name);

    return true;
}

void game_tick(Game* game, float dt) {
}

void game_draw() {}
void game_cleanup() {}
