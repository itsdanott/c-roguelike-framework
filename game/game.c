#include "game.h"

static CRLF_API* api;

_Bool game_init(CRLF_API* new_api){
	api = new_api;
	const char* name = "roguelike";
	api->CRLF_Log("Hello World from %s game!", name);

	return true;
}

void game_tick(Game* game){
	api->CRLF_Log("Game is Ticking!");
}

void game_draw(){

}
void game_cleanup(){

}