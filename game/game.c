#include "game.h"

static CRLF_API* api;
static Game_Resource_IDs* res_id;


/* TILES **********************************************************************/
//TODO: Atlas Cells
#define UI_IMAGE_TC_TILE_FOREST_01 ui_image_tex_coords_atlas_row_colum(3,0)
#define UI_IMAGE_TC_TILE_MOUNTAIN_01 ui_image_tex_coords_atlas_row_colum(3,1)
#define UI_IMAGE_TC_TILE_WATER_01 ui_image_tex_coords_atlas_row_colum(3,2)

/* UI *************************************************************************/
#if !defined(__GAMELIB_STATIC_LINK__)
UI_Context* ui_ctx;
#endif

#define UI_BUTTON(button_id) \
const u32 button_id = UI_ID(#button_id); \
const bool button_id##_hover = ui_ctx->input.hover_id == button_id;\
const bool button_id##_down = ui_ctx->input.down_id == button_id;

#define UI_BUTTON_STR(button_id, button_id_str) \
const u32 button_id = UI_ID_STR(button_id_str); \
const bool button_id##_hover = ui_ctx->input.hover_id == button_id;\
const bool button_id##_down = ui_ctx->input.down_id == button_id;

void draw_menu_entry(
    const int row, const String label, const String value, vec3 color
) {
    const float anchor_y =  1.f - (float)(row+1) * 0.1725f;
    UI_TEXT(label, {
        .layout = {
            .anchor = UI_SIZE(0.f, anchor_y),
            .offset = UI_POS(12,0)
        },
        .align = {.x = UI_ALIGNMENT_X_RIGHT },
        .color = color,
        .scale = .2f,
    });

    UI_TEXT(value, {
        .layout = {
            .anchor = UI_SIZE(1.f, anchor_y),
            .offset = UI_POS(-12,0)
        },
        .align = {.x = UI_ALIGNMENT_X_LEFT },
        .color = color,
        .scale = .2f,
    });
}

void draw_nav_button(
    const String id_str, const String str,
    const vec2 dir, const float btn_size
) {
    UI_BUTTON_STR(btn, id_str)
    UI({
       .id = btn,
       .layout = {
           .anchor = {0.5f - dir.x * 0.675f, 0.5f - dir.y * 0.675f},
           .size ={ btn_size, btn_size},
           .offset = {dir.x * btn_size, dir.y * btn_size},
       },
       .bg_color = btn_down ? COLOR_MAGENTA : COLOR_GRAY_DARK,
       .blocks_cursor = true,
       }) {
        UI_TEXT(str, {
            .layout = {
                .anchor = UI_SIZE(0.5f, 0.5f),
            },
            .color = COLOR_WHITE,
            .scale = 0.2f,
            .align = {
                .x = UI_ALIGNMENT_X_CENTER,
                .y = UI_ALIGNMENT_Y_CENTER,
            },
            .bg_slice = btn_hover,
        });
    }
}

void draw_game_menu() {
    UI({
        .id = UI_ID("Root"),
        .layout = {
            .anchor = UI_ANCHOR_CENTER,
            .size = UI_SIZE(1000,1000),
        },
        // .is_hidden = true,
        .bg_color = COLOR_WHITE,
        .nine_slice_id = res_id->nine_slice_square01_gray,
        .is_slice_center_hidden = true,
    }){
        UI({
            .id = UI_ID("TopBar"),
            .layout = {
                .anchor = UI_POS(0.f,1.0f),
                .offset = UI_POS(325.f,-30.f),
                .size = UI_SIZE(650.f,60.f),
            },
            .bg_color = COLOR_RED,
        }) {
            UI_TEXT(STRING("The Labrador Grasslands"), {
                .layout = {
                    .anchor = UI_SIZE(0.f, .5f),
                    .offset = UI_POS(5.f,0)
                },
                .align = {.x = UI_ALIGNMENT_X_RIGHT},
                .color = COLOR_GREEN,
                .scale = .125f,
                .outline = 3.f,
                .outline_color = COLOR_BLUE,
            });
            UI_TEXT(STRING("(Winter, Fog)"), {
                .layout = {
                    .anchor = UI_SIZE(1., .5f),
                    .offset = UI_POS(-5.f,0)
                },
                .align = {.x = UI_ALIGNMENT_X_LEFT},
                .color = COLOR_TEAL,
                .scale = .125f,
                .outline = 3.f,
                .outline_color = COLOR_BLUE,
            });
        }
        UI({
            .id = UI_ID("SideBar"),
            .layout = {
                .anchor = UI_POS(1,0.5),
                .offset = UI_POS(-175,0),
                .size = UI_SIZE(350,1000),
            },
            .bg_color = COLOR_RED,
        }) {
            UI_TEXT(STRING("CRLF Game"), {
                .layout = {
                    .anchor = UI_SIZE(.5f, 1.f),
                    .offset = UI_POS(0,-40)
                },
                .color = COLOR_YELLOW,
                .scale = .2f,
                .outline = 4.f,
                .outline_color = COLOR_BLUE,
            });

            UI({
                .layout = {
                    .anchor = {0.5, 1.0},
                    .offset = UI_POS(0,-225),
                    .size = UI_SIZE(300,300),
                },
                .bg_color = COLOR_GRAY,
            }) {
                const String strings [] ={
                    STRING("DAY"),
                    STRING("HEALTH"),
                    STRING("LOVE"),
                    STRING("PEACE"),
                    STRING("HARMONY"),
                };
                const String values [] ={
                    STRING("01"),
                    STRING("100"),
                    STRING("1337"),
                    STRING("69"),
                    STRING("131"),
                };
                const vec3 colors [] ={
                    COLOR_YELLOW,
                    COLOR_GREEN,
                    COLOR_RED,
                    COLOR_MAGENTA,
                    COLOR_CYAN,
                };
                for (int i = 0; i < 5; i ++) {
                    draw_menu_entry(i, strings[i], values[i], colors[i]);
                }
            }

        }

        const float nav_btn_size = 90.f;
        const float nav_size = nav_btn_size * 3.f;
        UI({
            .id = UI_ID("Navigation"),
            .layout = {
                .anchor = UI_POS(0.f,0.f),
                .offset = UI_POS(nav_btn_size*3.f*0.5f, nav_btn_size*3.f*0.5f),
                .size = UI_SIZE(nav_size, nav_size),
            },
            .bg_color = COLOR_RED,
        }) {
            draw_nav_button(
                STRING("nav_left"), STRING("Left"),
                UI_POS(1,0), nav_btn_size
            );
            draw_nav_button(
                STRING("nav_right"), STRING("Right"),
                UI_POS(-1,0), nav_btn_size
                );
            draw_nav_button(
                STRING("nav_down"), STRING("Down"),
                UI_POS(0,1), nav_btn_size
                );
            draw_nav_button(
                STRING("nav_up"), STRING("Up"),
                UI_POS(0,-1), nav_btn_size
            );
        }

        UI({
            .id = UI_ID("Actions"),
            .layout = {
                .anchor = UI_POS(0.f,0.f),
                .offset = UI_POS(nav_size + 190.f, nav_size * .5f),
                .size = UI_SIZE(380.f, nav_size),
            },
            .bg_color = COLOR_RED,
        }) {
            UI_BUTTON(action_a)
            UI({
                .id = action_a,
                .layout = {
                    .anchor = {0.f,     0.f},
                    .size =   {170.f, 250.f},
                    .offset = {100.f,  125.f},
                },
                .bg_color = action_a_down ? COLOR_MAGENTA : COLOR_GRAY_DARK,
                .blocks_cursor = true,
            }) {
                UI_TEXT(STRING("Action 1"), {
                    .layout = {
                        .anchor = UI_SIZE(0.5f, 1.f),
                    },
                    .color = COLOR_YELLOW,
                    .scale = 0.2f,
                    .align = {
                        .x = UI_ALIGNMENT_X_CENTER,
                        .y = UI_ALIGNMENT_Y_CENTER,
                    },
                    .bg_slice = action_a_hover,
                });
                UI_TEXT(STRING("Campfire\n[+1 Day]"), {
                    .layout = {
                        .anchor = UI_SIZE(0.5f, 0.7f),
                    },
                    .color = COLOR_YELLOW,
                    .scale = 0.175f,
                    .align = {
                        .x = UI_ALIGNMENT_X_CENTER,
                        .y = UI_ALIGNMENT_Y_CENTER,
                    },
                    .bg_slice = action_a_hover,
                });
                UI_IMAGE({
                    .texture = {.id = res_id->tex_placeholder },
                    .color = COLOR_YELLOW,
                    .pivot = UI_POS(0.5f, 0.5f),
                    .layout = {
                        .anchor = UI_SIZE(0.5f, 0.3f),

                        .size = {100.f,100.f},
                    },
                });
            }
            UI_BUTTON(action_b)
            UI({
                .id = action_b,
                .layout = {
                    .anchor = {1.f,     0.f},
                    .size =   {170.f, 250.f},
                    .offset = {-100.f,  125.f},
                },
                .bg_color = action_b_down ? COLOR_MAGENTA : COLOR_GRAY_DARK,
                .blocks_cursor = true,
            }) {
                UI_TEXT(STRING("Action B"), {
                    .layout = {
                        .anchor = UI_SIZE(0.5f, 1.f),
                    },
                    .color = COLOR_WHITE,
                    .scale = 0.2f,
                    .align = {
                        .x = UI_ALIGNMENT_X_CENTER,
                        .y = UI_ALIGNMENT_Y_CENTER,
                    },
                    .bg_slice = action_b_hover,
                });
            }
        }

        const float tiles_in_view = 10.f;
        const float tile_size = 650.f / tiles_in_view;
        const vec2 tile_size_vec =  UI_SIZE(tile_size, tile_size);
        UI({
            .id = UI_ID("GameWorld"),
            .layout = {
                .anchor = UI_POS(0.f,1.f),
                .offset = UI_POS(325,-325 - tile_size),
                .size = UI_SIZE(650,650),
            },
            .bg_color = COLOR_BLUE,
        }) {
            for (int x = 0; x<10; x++) {
                for (int y = 0; y<10; y++) {
                    UI_IMAGE({
                        .color = COLOR_WHITE,
                        .pivot = UI_POS(0.f, 0.f),
                        .texture = {
                            .id = res_id->tiles,
                            .coords = UI_IMAGE_TC_TILE_FOREST_01,
                        },
                        .layout = {
                            .anchor = UI_SIZE(0.0f, 0.0f),
                            .offset = UI_POS((float)x * tile_size,(float)y * tile_size),
                            .size = tile_size_vec,
                        },
                        .blocks_cursor = true,
                    });
                }
            }
        }
    }
}
void draw_settings_menu(){
     UI({
         .id = UI_ID("Root"),
         .layout = {
             .anchor = UI_ANCHOR_CENTER,
             .size = UI_SIZE(1000,1000),
         },
         .bg_color = COLOR_GRAY_DARK,
     }){
         UI({
             .id = UI_ID("SettingsMenu"),
             .layout = {
                 .anchor = UI_ANCHOR_CENTER,
                 .offset = UI_POS(0,0),
                 .size = UI_SIZE(250,450),
             },
             .bg_color = COLOR_RED,
         }) {
             UI_BUTTON(back_id)
             UI({
                 .id = back_id,
                 .layout = {
                     .anchor = UI_SIZE(0.5f, 0.5f),
                     .size ={back_id_down ? 200 : 175, 75},
                     .offset = UI_POS(0,0),
                 },
                 .bg_color = back_id_down ? COLOR_MAGENTA : COLOR_GRAY_DARK,
                 .blocks_cursor = true,
                 }) {
                 UI_TEXT(STRING("Back"), {
                     .layout = {
                         .anchor = UI_SIZE(0.5f, 0.5f),
                     },
                     .color = COLOR_WHITE,
                     .scale = 0.2f,
                     .align = {
                         .x = UI_ALIGNMENT_X_CENTER,
                         .y = UI_ALIGNMENT_Y_TOP,
                     },
                     .bg_slice = back_id_hover,
                 });
             }
         }
     }
}
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

            UI_BUTTON(github)
            const float github_size = github_hover ? 66.f : 64.f;
            UI_IMAGE({
                .id = github,
                .texture = {.id = res_id->logo_crlf },
                .color =  github_down ? COLOR_MAGENTA : COLOR_WHITE,
                .pivot = UI_POS(0.5f, 0.0f),
                .layout = {
                    .anchor = UI_SIZE(0.5f, 0.0f),
                    .offset = UI_POS(0.f, 32.f),
                    .size = UI_SIZE(github_size, github_size),
                },
                .blocks_cursor = true,
            });

            UI_BUTTON(image_test)
            const float image_size = image_test_hover ? 128.f : 120.f;
            UI_IMAGE( {
                .id = image_test,
                .texture = {.id = res_id->tex_placeholder },
                .color =  image_test_down ? COLOR_MAGENTA : COLOR_WHITE,
                .pivot = UI_POS(0.5f, 0.0f),
                .layout = {
                    .anchor = UI_SIZE(0.5f, 0.0f),
                    .offset = UI_POS(0.f, 128.f),
                    .size = UI_SIZE(image_size, image_size),
                },
                .blocks_cursor = true,
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
            const int num_buttons =
#if defined(SDL_PLATFORM_EMSCRIPTEN)
                3;
#else
                4;
#endif

            for (int i = 0; i<num_buttons; i++) {
                UI_BUTTON_STR(btn, strings[i])
                UI({
                    .id = btn,
                    .layout = {
                        .anchor = UI_SIZE(0.5f, 0.75f),
                        .size ={ btn_down ? 340 : 315,75},
                        .offset = UI_POS(0,-i*100),
                    },
                    .bg_color = btn_down ? COLOR_MAGENTA : COLOR_GRAY_DARK,
                    .blocks_cursor = true,
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
                        .bg_slice = btn_hover,
                    });
                }
            }
        }
    }
}

/* PUBLIC GAME API IMPLEMENTATION *********************************************/
GAME_API bool game_init(
    CRLF_API* new_api, Game* game, UI_Context* ui, Game_Resource_IDs* res_ids
) {
    api = new_api;
    *game = default_game();
    res_id = res_ids;
#if !defined(__GAMELIB_STATIC_LINK__)
    ui_ctx = ui;
#endif

    return true;
}

GAME_API void game_tick(Game* game, float dt) {}

GAME_API void game_draw(const Game* game) {
    if (game->is_main_menu)
        draw_main_menu();
    else
        // draw_game_menu();
        draw_settings_menu();
}

GAME_API void game_cleanup(Game* game) {}
