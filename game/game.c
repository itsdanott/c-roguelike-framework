#include "game.h"

static CRLF_API* api;

/* UI *************************************************************************/
#if !defined(__GAMELIB_STATIC_LINK__)
UI_Context* ui_ctx;
#endif

void draw_main_menu() {
    UI({
        .id = UI_ID("Root"),
        .layout = {
            .size = {
                .width = UI_SIZE_PERCENT(1.f),
                .height = UI_SIZE_PERCENT(1.f)
            },
            .child_align = {
                    .x = UI_ELEMENT_ALIGNMENT_X_CENTER,
            },
        },
        .bg_color = COLOR_GRAY_DARK,
    }) {
        UI({
            .id = UI_ID("MainMenu"),
            .layout = {
                .size = {
                    .width = UI_SIZE_PERCENT(.5f),
                    .height = UI_SIZE_PERCENT(.5f),
                },
            },
            .bg_color = COLOR_RED,
        }) {
            UI_TEXT(STRING("Hello world!"), {
                .id = UI_ID("TitleString"),
                .alignment = {
                    .x = UI_ELEMENT_ALIGNMENT_X_CENTER,
                }
            });
            UI_TEXT(STRING("7drl 2025!"), {
                .id = UI_ID("TitleString"),
                .alignment = {
                    .x = UI_ELEMENT_ALIGNMENT_X_CENTER,
                },
                .color = COLOR_GREEN,
            });
        }
    }
}

/* PUBLIC GAME API IMPLEMENTATION *********************************************/
GAME_API bool game_init(CRLF_API* new_api, Game* game, UI_Context* ui) {
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

GAME_API void game_tick(Game* game, float dt) {}

GAME_API void game_draw(Game* game) {
    draw_main_menu();
}

GAME_API void game_cleanup(Game* game) {}
