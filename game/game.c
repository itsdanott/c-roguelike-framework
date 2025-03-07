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
            .anchor = UI_ANCHOR_CENTER,
            .size = UI_SIZE(1000,1000),
        },
        .bg_color = COLOR_GRAY_DARK,
    }) {
        UI({
            .id = UI_ID("Main_Menu"),
            .layout = {
                .anchor = UI_ANCHOR_CENTER,
                .offset = UI_POS(0,0),
                .size = UI_SIZE(400,900),
            },
            .bg_color = COLOR_RED,
        }) {
            UI_TEXT(STRING("GAME TITLE"), {
                .id = UI_ID("Game_Title"),
                .layout = {
                    .anchor = UI_SIZE(0.5f, 0.9f),
                },
                .color = COLOR_YELLOW,
                .scale = .25f,
                .outline = 4.f,
                .outline_color = COLOR_BLUE,
            });

            UI_IMAGE(4, {
                .color = COLOR_WHITE,
                .pivot = UI_POS(0.5f, 0.0f),
                .layout = {
                    .anchor = UI_SIZE(0.5f, 0.0f),
                    .offset = UI_POS(0.f, 32.f),
                    .size = UI_SIZE(64,64),
                },
            });

            const String strings [] ={
                STRING("New Game"),
                STRING("Load Game"),
                STRING("Settings"),
                STRING("Quit"),
            };
            const vec3 colors [] = {
                COLOR_GREEN,
                COLOR_WHITE,
                COLOR_YELLOW,
                COLOR_MAGENTA
            };
            //Testing future interactivity via hot reload:
            const int selected_index = 0;

            for (int i = 0; i<4; i++) {
                bool is_selected = selected_index == i;
                UI({
                    .layout = {
                        .anchor = UI_SIZE(0.5f, 0.75f),
                        .size ={ is_selected ? 360 : 315,75},
                        .offset = UI_POS(0,-i*100),
                    },
                    .bg_color = COLOR_GRAY_DARK,
                }) {
                    UI_TEXT(strings[i], {
                        .layout = {
                            .anchor = UI_SIZE(0.5f, 0.5f),
                        },
                        .color = colors[i],
                        .scale = 0.2f,
                        .align = {
                            .x = UI_ALIGNMENT_X_CENTER,
                            .y = UI_ALIGNMENT_Y_TOP,
                        },
                        .bg_slice = is_selected,
                    });
                }
            }
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
