#include "game.h"

static CRLF_API* api;

/* UI *************************************************************************/
#if defined(__GAMELIB_STATIC_LINK__)
extern
#endif
UI_Context* ui_ctx;

void draw_main_menu() {
    UI_LAYOUT(
        UI_LAYOUT(
            UI_TEXT("NEW GAME");
            UI_IMAGE(42);
        );

        UI_LAYOUT(
            UI_TEXT("LOAD GAME");
            UI_IMAGE(69);
        );
    );
}

/* PUBLIC GAME API IMPLEMENTATION *********************************************/
bool game_init(CRLF_API* new_api, Game* game, UI_Context* ui) {
    api = new_api;
    *game = default_game();
#if !defined(__GAMELIB_STATIC_LINK__)
    ui_ctx = ui;
#endif

    const char* name = "ROGUELIKE";
    api->log_msg("Hello World from the %s game!", name);
    api->log_msg(
        "ui_context string arena capacity = %zu (accessed from game)",
        ui_ctx->string_arena.capacity
    );

    return true;
}

void game_tick(Game* game, float dt) {}

void game_draw(Game* game) {
    draw_main_menu();
}

void game_cleanup(Game* game) {}
