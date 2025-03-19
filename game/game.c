
#define FNL_IMPL

#include "game.h"

static CRLF_API* api;
static Game_Resource_IDs* res_id;

static const char* GAME_TITLE = "Micro Monarch";

/* TILES **********************************************************************/
//TODO: Atlas Cells
#define TXC_TILE_FOREST_01 ui_image_tex_coords_atlas_row_colum(3,0)
#define TXC_TILE_MOUNTAIN_01 ui_image_tex_coords_atlas_row_colum(3,1)
#define TXC_TILE_WATER_01 ui_image_tex_coords_atlas_row_colum(3,2)

#define TXC_TILE_GRASS_01 ui_image_tex_coords_atlas_row_colum(2,0)

#define TXC_TILE_GRID ui_image_tex_coords_atlas_row_colum(3,3)

/* CHARACTERS******************************************************************/

#define TXC_CHARACTER_MONARCH ui_image_tex_coords_atlas_row_colum(0,0)
#define TXC_CHARACTER_DEER ui_image_tex_coords_atlas_row_colum(3,0)

/* UI *************************************************************************/
#define ROOT_LAYOUT root_container_layout()

UI_Element_Layout root_container_layout() {
    return (UI_Element_Layout) {
        .anchor = UI_ANCHOR_CENTER,
        .size = {1000.f, 1000.f},
    };
}

#if !defined(__GAMELIB_STATIC_LINK__)
UI_Context* ui_ctx;
#endif

#define UI_BUTTON(button_id) \
const u32 button_id = UI_ID(#button_id); \
const bool button_id##_hover = ui_ctx->input.hover_id == button_id;\
const bool button_id##_down = ui_ctx->input.down_id == button_id;

//button_id_str = String made with STRING
#define UI_BUTTON_STR(button_id, button_id_str) \
const u32 button_id = UI_ID_STR(button_id_str); \
const bool button_id##_hover = ui_ctx->input.hover_id == button_id;\
const bool button_id##_down = ui_ctx->input.down_id == button_id;

//id = u32 made with UI_ID
#define UI_BUTTON_ID(button, button_id) \
const u32 button = button_id; \
const bool button##_hover = ui_ctx->input.hover_id == button_id;\
const bool button##_down = ui_ctx->input.down_id == button_id;

void draw_menu_entry(
    const int row, const String label, const String value, vec3 color
) {
    const float anchor_y =  1.f - (float)(row+1) * 0.1725f;
    UI_TEXT(label, {
        .layout = {
            .anchor = {0.f, anchor_y},
            .offset = {12.f,0.f}
        },
        .align = {.x = UI_ALIGNMENT_X_RIGHT },
        .color = color,
        .scale = .2f,
    });

    UI_TEXT(value, {
        .layout = {
            .anchor = {1.f, anchor_y},
            .offset = {-12.f,0.f}
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
           .anchor = UI_ANCHOR_CENTER,
           .size ={ btn_size, btn_size},
           .offset = {dir.x * btn_size, dir.y * btn_size},
       },
       .bg_color = btn_down ? COLOR_MAGENTA : COLOR_GRAY_DARK,
       .blocks_cursor = true,
       }) {
        UI_TEXT(str, {
            .layout = {
                .anchor = {0.5f, 0.5f},
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

UI_Image_Tex_Coords tex_coords_from_tile(const Tile_Type tile_type) {
    switch (tile_type) {
    case TILE_TYPE_FOREST: return TXC_TILE_FOREST_01;
    case TILE_TYPE_MOUNTAIN: return TXC_TILE_MOUNTAIN_01;
    case TILE_TYPE_WATER: return TXC_TILE_WATER_01;
    case TILE_TYPE_GRASS: return TXC_TILE_GRASS_01;
    case TILE_TYPE_GRID: return TXC_TILE_GRID;
    }
    SDL_assert(0);
    return (UI_Image_Tex_Coords){0};
}

UI_Image_Tex_Coords tex_coords_from_character(const Character_Type character) {
    switch (character) {
    case CHARACTER_TYPE_MONARCH: return TXC_CHARACTER_MONARCH;
    case CHARACTER_TYPE_DEER: return TXC_CHARACTER_DEER;
    default:
        break;
    }
    SDL_assert(0);
    return (UI_Image_Tex_Coords){0};
}

void draw_character(
    const Character_Type character,
    const float x,
    const float y,
    const float tile_size
) {
    const vec2 tile_size_vec =  {tile_size, tile_size};
    const UI_Image_Tex_Coords tex_coords = tex_coords_from_character(character);
    switch (character) {
    default:
        UI_IMAGE({
            .color = COLOR_WHITE,
            .pivot = {0.f, 0.f},
            .texture = {
                .id = res_id->characters,
                .coords = tex_coords,
            },
            .layout = {
                .anchor = {0.0f, 0.0f},
                .offset = {
                    x * tile_size, //- tile_size*0.25f,
                    y * tile_size - tile_size*0.25f,
                    },
                .size = tile_size_vec,
            },
            .blocks_cursor = true,
        });
        break;
    }
}
void draw_tile(
    const Tile_Type tile_type,
    const float x,
    const float y,
    const float tile_size,
    const i32 world_x,
    const i32 world_y
) {
    const vec2 tile_size_vec =  {tile_size, tile_size};
    const UI_Image_Tex_Coords tex_coords = tex_coords_from_tile(tile_type);
    switch (tile_type) {
    default:
        UI_IMAGE({
            .color = COLOR_WHITE,
            .pivot = {0.f, 0.f},
            .texture = {
                .id = res_id->tiles,
                .coords = tex_coords,
            },
            .layout = {
                .anchor = {0.0f, 0.0f},
                .offset = {
                    x * tile_size,
                    y * tile_size,
                    },
                .size = tile_size_vec,
            },
        });
        break;
    case TILE_TYPE_WATER: {
        const float wave_strength = 1.5f;
        const float wave_displace = 1.25f;
        UI_IMAGE({
            .color = vec3_lerp(COLOR_BLUE, COLOR_AQUA,
                (SDL_sin(ui_ctx->time + world_x * world_y) + 1.f ) / 2.0f),
            .pivot = {0.f, 0.f},
            .texture = {
                .id = res_id->tiles,
                .coords = TXC_TILE_WATER_01,
            },
            .layout = {
                .anchor = {0.0f, 0.0f},
                .offset = {
                    x * tile_size + SDL_sin((ui_ctx->time + world_x*world_y) *wave_strength) * tile_size * wave_displace / 40.f,
                    y * tile_size + SDL_sin((ui_ctx->time  + world_x) * wave_strength * .75f) * tile_size * wave_displace / 20.f,
                    },
                .size = tile_size_vec,
            },
        });
        break;
    }
    }
}

void draw_game_world(const Game* game) {
    const int tiles_in_view = 11;
    const int center = tiles_in_view/2;
    const float tile_size = 650.f / (float)tiles_in_view;

    const i32 bottom_left_x = game->player.pos_x - center;
    const i32 bottom_left_y = game->player.pos_y - center;
    UI({
        .id = UI_ID("GameWorld"),
        .sort_order_override = CRLF_SORT_ORDER_MIN,
        .layout = {
            .anchor = {0.0f, 1.0f},
            .offset = {325.f, -325.f - tile_size},
            .size = {650.f, 650.f},
        },
        .bg_color = COLOR_BLUE,
        .is_hidden = true,
    }) {
        for (int x = 0; x<tiles_in_view; x++) {
            for (int y = 0; y<tiles_in_view; y++) {
                const i32 world_x = bottom_left_x + x;
                const i32 world_y = bottom_left_y + y;
                const bool is_in_bounds = is_in_world_bounds(world_x, world_y);

                if (is_in_bounds) {
                    const size_t tile_index = TILE_INDEX(world_x, world_y);
                    const Tile_Type tile = game->world.tiles[tile_index];
                    draw_tile(
                        tile, (float)x, (float)y, tile_size, world_x, world_y
                    );
                }
            }
        }

        UI({
            .layout = {
                .anchor = UI_ANCHOR_CENTER,
                .size = {650.f, 650.f},
            },
            .is_hidden = true,
        }) {
            draw_character(
                CHARACTER_TYPE_MONARCH, (float)center, (float)center, tile_size
            );
            for (int i = 0; i < game->world.num_npcs; i++) {
                const NPC* npc = &game->world.npcs[i];
                SDL_assert(npc->character != CHARACTER_TYPE_NONE);
                if (npc->pos_x > bottom_left_x &&
                    npc->pos_x < bottom_left_x + tiles_in_view &&
                    npc->pos_y > bottom_left_y &&
                    npc->pos_y < bottom_left_y + tiles_in_view) {
                    draw_character(
                        npc->character, (float)(npc->pos_x-bottom_left_x),
                        (float)(npc->pos_y-bottom_left_y),
                        tile_size
                    );
                }
            }
           //  draw_tile(
           //     TILE_TYPE_GRID, (float)center, (float)center, tile_size
           // );

        }
    }
}

void draw_game_menu_top_bar() {
    UI({
        .id = UI_ID("TopBar"),
        .layout = {
            .anchor = {0.f,1.0f},
            .offset = {325.f,-32.f},
            .size = {650.f,64.f},
        },
        .bg_color = COLOR_RED,
        }) {
        UI_TEXT(STRING("The Labrador Grasslands"), {
            .layout = {
                .anchor = {0.f, .5f},
                .offset = {5.f,0.f},
            },
            .align = {.x = UI_ALIGNMENT_X_RIGHT},
            .color = COLOR_GREEN,
            .scale = .125f,
            .outline = 3.f,
            .outline_color = COLOR_BLUE,
        });
        UI_TEXT(STRING("(Winter, Fog)"), {
            .layout = {
                .anchor = {1.0f, 0.5f},
                .offset = {-5.0f,0.f},
            },
            .align = {.x = UI_ALIGNMENT_X_LEFT},
            .color = COLOR_TEAL,
            .scale = .125f,
            .outline = 3.f,
            .outline_color = COLOR_BLUE,
        });
    }
}
#define GAME_SIDEBAR_WIDTH 350.f
void draw_game_menu_side_bar(const Game* game) {
    UI({
        .id = UI_ID("SideBar"),
        .layout = {
            .anchor = {1.f, 0.5f},
            .offset = {-175.f,0.f},
            .size = {GAME_SIDEBAR_WIDTH, 1000.f},
        },
        .bg_color = COLOR_RED,
    }) {
        UI_TEXT(STRING(GAME_TITLE), {
            .layout = {
                .anchor = {.5f, 1.f},
                .offset = {0.f, -40.f},
            },
            .color = COLOR_YELLOW,
            .scale = .2f,
            .outline = 4.f,
            .outline_color = COLOR_BLUE,
        });

        UI({
            .layout = {
                .anchor = {0.5f, 1.0f},
                .offset = {0.f, -225.f},
                .size = {300.f, 300.f},
            },
            .bg_color = COLOR_GRAY,
        }) {
            const String strings [] ={
                STRING("DAY"),
                STRING("HEALTH"),
                STRING("WOOD"),
                STRING("PEACE"),
                STRING("HARMONY"),
            };
#define GAME_SIDEBAR_STAT_MAX_DIGITS 16
#define GAME_SIDEBAR_STAT_BYTES (GAME_SIDEBAR_STAT_MAX_DIGITS* sizeof(char))
            char* days = arena_alloc(&ui_ctx->string_arena, GAME_SIDEBAR_STAT_BYTES);
            SDL_snprintf(days, GAME_SIDEBAR_STAT_BYTES, "%d", game->days);

            char* wood = arena_alloc(&ui_ctx->string_arena, GAME_SIDEBAR_STAT_BYTES);
            SDL_snprintf(wood, GAME_SIDEBAR_STAT_BYTES, "%d", game->wood);

            const String values [] ={
                STRING(days),
                STRING("100"),
                STRING(wood),
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
}

void draw_game_menu_navigation(const float nav_size){
    const float nav_btn_size = nav_size / 3.f;
    UI({
        .id = UI_ID("Navigation"),
        .layout = {
            .anchor = {0.f,0.f},
            .offset = {nav_btn_size*3.f*0.5f, nav_btn_size*3.f*0.5f},
            .size = {nav_size, nav_size},
        },
        .bg_color = COLOR_RED,
    }) {
        draw_nav_button(
            STRING("nav_left"), STRING("Left"),
            VEC2(-1.f,0.f), nav_btn_size
        );
        draw_nav_button(
            STRING("nav_right"), STRING("Right"),
            VEC2(1.f,0.), nav_btn_size
            );
        draw_nav_button(
            STRING("nav_down"), STRING("Down"),
            VEC2(0.f,-1.f), nav_btn_size
            );
        draw_nav_button(
            STRING("nav_up"), STRING("Up"),
            VEC2(0.f,1.), nav_btn_size
        );
    }
}

typedef struct {
    String title;
    String desc;
    u32 id;
} Action_Info;

void draw_action_btn(
    const Action_Info action,
    const float width,
    const float height,
    const bool is_a
) {

    UI_BUTTON_ID(btn, action.id)
    UI({
        .id = btn,
        .layout = {
            .anchor = {is_a ? 0.25f : 0.75f,  0.5f},
            .size =   {width, height},
            // .offset = {(is_a ? 1.f : -1.f) * width * 0.5f, 0.f},
        },
        .bg_color = btn_down ? COLOR_MAGENTA : COLOR_GRAY_DARK,
        .blocks_cursor = true,
    }) {
        UI_TEXT(action.title, {
            .layout = {
                .anchor = {0.5f, 1.f},
                .offset = {0.f, -30.f},
            },
            .color = COLOR_YELLOW,
            .scale = 0.175f,
            .align = {
                .x = UI_ALIGNMENT_X_CENTER,
                .y = UI_ALIGNMENT_Y_CENTER,
            },
            .bg_slice = btn_hover,
        });
        UI_TEXT(action.desc, {
            .layout = {
                .anchor = {0.5f, 1.f},
                .offset = {0.f, -110.f},
            },
            .color = COLOR_YELLOW,
            .scale = 0.175f,
            .align = {
                .x = UI_ALIGNMENT_X_CENTER,
                .y = UI_ALIGNMENT_Y_CENTER,
            },
            .bg_slice = btn_hover,
        });
        //TODO: image
        UI_IMAGE({
            .texture = {.id = res_id->tex_placeholder },
            .color = COLOR_YELLOW,
            .pivot = {0.5f, 0.0f},
            .layout = {
                .anchor = {0.5f, 0.0f},
                .size = {100.f,100.f},
                .offset = {0.f, 16.f},
            },
        });
    }
}

void draw_game_menu_actions(const float nav_size) {
    const float menu_width = 1000.f - GAME_SIDEBAR_WIDTH - nav_size;
    UI({
        .id = UI_ID("Actions"),
        .layout = {
            .anchor = {0.f, 0.f},
            .offset = {nav_size + menu_width * 0.5f, nav_size * .5f},
            .size = {menu_width, nav_size},
        },
        .bg_color = COLOR_RED,
    }) {
        const Action_Info action_a = {
            .title = STRING("CAMPFIRE"),
            .desc = STRING("Rest\n[+1 Day]"),
            .id = UI_ID("action_campfire")
        };
        const Action_Info action_b = {
            .title = STRING("CHOP"),
            .desc = STRING("Cut Tree\n[+1 Wood]"),
            .id = UI_ID("action_chop")
        };

        const float action_width = menu_width * 0.45f;
        draw_action_btn(action_a, action_width, nav_size, true);
        draw_action_btn(action_b, action_width, nav_size, false);
    }
}
void draw_game_menu_bottom() {
    const float nav_size = 285.f;
    draw_game_menu_navigation(nav_size);
    draw_game_menu_actions(nav_size);
}
void draw_gameplay(const Game* game) {
    UI({
        .layout = ROOT_LAYOUT,
        .is_hidden = true,
    }){
        draw_game_world(game);

        draw_game_menu_top_bar();
        draw_game_menu_side_bar(game);
        draw_game_menu_bottom();
    }
}

void draw_sub_menu_skeleton(const String title_str) {
    UI_TEXT(title_str, {
        .layout = {
            .anchor = {0.5f, 1.f},
            .offset = {0.f, -32.f}
        },
       .color = COLOR_WHITE,
       .scale = 0.2f,
    });

    UI_BUTTON(btn_back_to_menu)
    UI({
        .id = btn_back_to_menu,
        .layout = {
            .anchor = {0.5f, 0.f},
            .size ={btn_back_to_menu_down ? 200.f : 175.f, 75.f},
            .offset = {0.f, 75.f},
        },
        .bg_color = btn_back_to_menu_down ? COLOR_MAGENTA : COLOR_GRAY_DARK,
        .blocks_cursor = true,
        }) {
        UI_TEXT(STRING("Back"), {
            .layout = {
                .anchor = {0.5f, 0.5f},
            },
            .color = COLOR_WHITE,
            .scale = 0.2f,
            .align = {
                .x = UI_ALIGNMENT_X_CENTER,
                .y = UI_ALIGNMENT_Y_TOP,
            },
            .bg_slice = btn_back_to_menu_hover,
        });
    }
}

void draw_new_game_menu(){
    UI({
        .layout = ROOT_LAYOUT,
        .bg_color = COLOR_GRAY_DARK,
    }){
        UI({
            .layout = {
                .anchor = UI_ANCHOR_CENTER,
                .offset = {0.f,0.f},
                .size = {600.f,800.f},
            },
            .bg_color = COLOR_RED,
        }) {
            draw_sub_menu_skeleton(STRING("New Game"));
        }
    }
}

void draw_settings_menu(){
     UI({
         .layout = ROOT_LAYOUT,
         .bg_color = COLOR_GRAY_DARK,
     }){
         UI({
             .layout = {
                 .anchor = UI_ANCHOR_CENTER,
                 .offset = {0.f,0.f},
                 .size = {250.f,450.f},
             },
             .bg_color = COLOR_RED,
         }) {

            draw_sub_menu_skeleton(STRING("Settings"));
         }
     }
}

typedef struct {
    u32 btn;
    String str;
} Game_About_Entry;

void draw_about_game_menu(){
    UI({
        .layout = ROOT_LAYOUT,
        .bg_color = COLOR_GRAY_DARK,
    }){
        UI({
            .layout = {
                .anchor = UI_ANCHOR_CENTER,
                .offset = {0.f,0.f},
                .size = {500.f,800.f},
            },
            .bg_color = COLOR_RED,
        }) {
            draw_sub_menu_skeleton(STRING("About"));

            const Game_About_Entry entries[] = {
                {0, STRING("Third Party Libs")},
                {UI_ID("stb"), STRING("stb by Sean Barret")},
                {UI_ID("sdl"), STRING("SDL3")},
                {UI_ID("fastnoise"), STRING("FastNoise Lite by Auburn")},
                {UI_ID("emscripten"), STRING("emscripten")},
                {0, STRING("Font")},
                {UI_ID("born2bsporty"), STRING("Born2bSportyV2 by JapanYoshi")},
            };
            const i32 num_entries = sizeof(entries) / sizeof(Game_About_Entry);


            for (int i =0; i < num_entries; i++) {
                //Button
                if (entries[i].btn != 0 ) {
                    UI_BUTTON_ID(btn, entries[i].btn)
                    UI({
                        .id = btn,
                        .layout = {
                            .anchor = {0.5f, 0.75f},
                            .size = {315,60},
                            .offset = {0.f,-(float)i*60.f},
                        },
                        .bg_color = COLOR_MAGENTA,
                        .is_hidden = !btn_down,
                        .blocks_cursor = true,
                    }) {
                        UI_TEXT(entries[i].str, {
                            .layout = {
                                .anchor = {0.5f, 0.5f},
                            },
                            .align = {.x = UI_ALIGNMENT_X_CENTER },
                            .color = COLOR_WHITE,
                            .scale = 0.15f,
                            .bg_slice = btn_hover,
                        });
                    }
                } else {
                    //Normal text without URL (Headlines)
                    UI_TEXT(entries[i].str, {
                        .layout = {
                            .anchor = {0.5f, 0.75f},
                            .size = {315,60},
                            .offset = {0.f,-(float)i*60.f},
                        },
                        .align = {.x = UI_ALIGNMENT_X_CENTER },
                        .color = COLOR_GREEN,
                        .scale = 0.2f,
                    });
                }
            }
        }
    }
}

void draw_main_menu() {
    UI({
        .layout = ROOT_LAYOUT,
        .bg_color = COLOR_GRAY_DARK,
        .nine_slice_id = res_id->nine_slice_square01_black,
    }) {
        UI({
            .id = UI_ID("main_menu"),
            .layout = {
                .anchor = UI_ANCHOR_CENTER,
                .offset = {0.f, 0.f},
                .size = {400.f, 900.f},
            },
            .bg_color = COLOR_RED,
        }) {
            UI_TEXT(STRING(GAME_TITLE), {
                .id = UI_ID("Game_Title"),
                .layout = {
                    .anchor = {0.5f, 0.9f},
                },
                .color = COLOR_YELLOW,
                .scale = .25f,
                .outline = 4.f,
                .outline_color = COLOR_BLUE,
            });

            UI_BUTTON(btn_author)
            UI({
                .id = btn_author,
                .layout = {
                    .anchor = {0.5f, 0.0f},
                    .offset = {0.f, 105},
                    .size = {200,32},
                },
                .is_hidden = true,
                .blocks_cursor = true,
            }) {
                UI_TEXT(STRING("Made by o:tone for 7drl 2025"), {
                    .layout = {
                        .anchor = UI_ANCHOR_CENTER,
                    },
                    .color = COLOR_GREEN,
                    .scale = .125f,
                    .outline = 3.5f,
                    .bg_slice = btn_author_hover,
                });
            }


            UI_BUTTON(github)
            const float github_size = github_hover ? 85.f : 80.f;
            UI_IMAGE({
                .id = github,
                .texture = {.id = res_id->logo_crlf },
                .color =  github_down ? COLOR_MAGENTA : COLOR_WHITE,
                .pivot = {0.5f, 0.0f},
                .layout = {
                    .anchor = {0.5f, 0.0f},
                    .offset = {0.f, 15.f},
                    .size = {github_size, github_size},
                },
                .blocks_cursor = true,
            });

            UI_BUTTON(image_test)
            const float image_size = image_test_hover ? 128.f : 120.f;
            UI_IMAGE( {
                .id = image_test,
                .texture = {.id = res_id->tex_placeholder },
                .color =  image_test_down ? COLOR_MAGENTA : COLOR_WHITE,
                .pivot = {0.5f, 0.0f},
                .layout = {
                    .anchor = {0.5f, 0.0f},
                    .offset = {0.f, 128.f},
                    .size = {image_size, image_size},
                },
                .blocks_cursor = true,
            });

            /* MENU BUTTONS ***************************************************/
            const String strings [] ={
                STRING("New Game"),
                STRING("About"),
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
                        .anchor = {0.5f, 0.75f},
                        .size ={ btn_down ? 340 : 315,75},
                        .offset = {0,-(float)i * 100.f},
                    },
                    .bg_color = btn_down ? COLOR_MAGENTA : COLOR_GRAY_DARK,
                    .blocks_cursor = true,
                }) {
                    UI_TEXT(strings[i], {
                        .layout = {
                            .anchor = UI_ANCHOR_CENTER,
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

void npc_set_random_target_pos(Random* random, NPC* npc) {
    const ivec2 current_target_pos = (ivec2) {
        npc->target_pos_x, npc->target_pos_y
    };

    do {
        //TODO: test how this compares to using two random ranges / seperate axes
        const size_t target_tile_index = random_int_range(
            random, 0, WORLD_SIZE * WORLD_SIZE
        );
        world_pos_from_tile_index(
            target_tile_index, &npc->target_pos_x, &npc->target_pos_y
        );
        SDL_assert(is_in_world_bounds(npc->target_pos_x,npc->target_pos_y));
    } while (
        current_target_pos.x == npc->target_pos_x &&
        current_target_pos.y == npc->target_pos_y &&
        npc->pos_x == npc->target_pos_x &&
        npc->pos_y == npc->target_pos_y
    );
}
void generate_world(Game* game) {
    game->fnl = fnlCreateState();
    game->fnl.seed = game->seed;
    random_init(&game->random, game->seed);

    for (int x = 0; x < WORLD_SIZE; x++) {
        for (int y = 0; y < WORLD_SIZE; y++) {
            const float noise = fnlGetNoise2D(&game->fnl, (float)x, (float)y);
            Tile_Type tile;
            if (noise <= 0.25f) {
                tile =  TILE_TYPE_WATER;
            } else if (noise <= 0.5f) {
                tile =  TILE_TYPE_GRASS;
            } else if (noise < 0.85f) {
                tile = TILE_TYPE_FOREST;
            } else {
                tile = TILE_TYPE_MOUNTAIN;
            }

            game->world.tiles[TILE_INDEX(x,y)] = tile;
        }
    }

    for (int i = 0; i < START_NPC_NUM_DEER; i++) {

        const size_t pos_tile_index = random_int_range(
            &game->random, 0, WORLD_SIZE * WORLD_SIZE
        );

        NPC* npc = &game->world.npcs[i];
        *npc = (NPC){
            .character = CHARACTER_TYPE_DEER,
        };
        world_pos_from_tile_index(pos_tile_index, &npc->pos_x, &npc->pos_y);
        npc_set_random_target_pos(&game->random, npc);
        game->world.num_npcs +=1;
    }

}
void set_player_pos(Game* game, const i32 x, const i32 y) {
    game->player.pos_x = x;
    game->player.pos_y = y;
#if defined(__DEBUG__)
    api->log_msg("Player Pos: X=%d, Y=%d", x, y);
#endif
}

void game_simulate(Game* game) {
    for (int i =0; i < game->world.num_npcs; i++) {
        NPC* npc = &game->world.npcs[i];
        if (npc->action_breaks > 0) {
            npc->action_breaks--;
            continue;
        }
        i32 direction_x, direction_y;
        get_direction_from_to(
            npc->pos_x, npc->pos_y,
            npc->target_pos_x, npc->target_pos_y,
            &direction_x, &direction_y
        );
        if (direction_x == 0 && direction_y == 0) {
            npc_set_random_target_pos(&game->random, npc);
            continue;
        }

        if (direction_x != 0) {
            const i32 x_sign = CRLF_SIGN(direction_x);
            npc->pos_x += x_sign;
        } else if (direction_y != 0) {
            const i32 y_sign = CRLF_SIGN(direction_y);
            npc->pos_y += y_sign;
        }

        if (random_float(&game->random) > 0.5f) {
            npc->action_breaks +=  random_int_range(&game->random, 1,3);
        }

        SDL_assert(is_in_world_bounds(npc->pos_x,npc->pos_y));
    }
}
void end_day(Game* game) {
    game->days++;
    game_simulate(game);
}
void input_move_north(Game* game) {
    set_player_pos(game, game->player.pos_x, game->player.pos_y+1);
    end_day(game);
}
void input_move_south(Game* game) {
    set_player_pos(game, game->player.pos_x, game->player.pos_y-1);
    end_day(game);
}
void input_move_east(Game* game) {
    set_player_pos(game, game->player.pos_x+1, game->player.pos_y);
    end_day(game);
}
void input_move_west(Game* game) {
    set_player_pos(game, game->player.pos_x-1, game->player.pos_y);
    end_day(game);
}
void action_chop_tree(Game* game) {
    SDL_assert(is_in_world_bounds(game->player.pos_x, game->player.pos_y));
    const size_t tile_index = game->player.pos_x + game->player.pos_y *
        WORLD_SIZE;
    const Tile_Type tile = game->world.tiles[tile_index];
    SDL_assert(tile == TILE_TYPE_FOREST);
    game->world.tiles[tile_index] = TILE_TYPE_GRASS;
    game->wood++;
    end_day(game);
}
void action_campfire(Game* game) {
    end_day(game);
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

GAME_API void game_draw(Game* game) {
    switch (game->state) {
    case GAME_STATE_MENU:
        draw_main_menu();
        break;
    case GAME_STATE_NEW_GAME:
        draw_new_game_menu();
        break;
    case GAME_STATE_SETTINGS:
        draw_settings_menu();
        break;
    case GAME_STATE_ABOUT_GAME:
        draw_about_game_menu();
        break;
    case GAME_STATE_GAMEPLAY:
        draw_gameplay(game);
        break;
    }
}

GAME_API void game_cleanup(Game* game) {}

GAME_API void game_ui_input(Game* game, const u32 id) {

    switch (game->state) {
    case GAME_STATE_MENU:
        break;
    case GAME_STATE_NEW_GAME:
        break;
    case GAME_STATE_SETTINGS:
        break;
    case GAME_STATE_ABOUT_GAME:
        break;
    case GAME_STATE_GAMEPLAY:
        if (id == UI_ID("nav_left"))
            input_move_west(game);
        else if (id == UI_ID("nav_right"))
            input_move_east(game);
        else if (id == UI_ID("nav_up"))
            input_move_north(game);
        else if (id == UI_ID("nav_down"))
            input_move_south(game);
        else if (id == UI_ID("action_chop"))
            action_chop_tree(game);
        else if (id == UI_ID("action_campfire"))
            action_campfire(game);
        break;
    }
    if (id == UI_ID("New Game")) {
        generate_world(game);
        game->state = GAME_STATE_GAMEPLAY;//GAME_STATE_NEW_GAME;
    } else if (id == UI_ID("About")){
        game->state = GAME_STATE_ABOUT_GAME;
    } else if (id == UI_ID("Settings")){
        game->state = GAME_STATE_SETTINGS;
    } else if (id == UI_ID("Quit")) {
        game->quit_requested = true;
    } else if (id == UI_ID("btn_back_to_menu")) {
        game->state = GAME_STATE_MENU;
    } else if (id == UI_ID("github")) {
        SDL_OpenURL(
            "https://github.com/itsdanott/c-roguelike-framework/"
        );
    } else if ( id == UI_ID("btn_author")) {
        SDL_OpenURL(
            "https://bsky.app/profile/itsdanott.bsky.social"
            );
    } else if ( id == UI_ID("stb")) {
        SDL_OpenURL(
            "https://github.com/nothings/stb"
        );
    } else if ( id == UI_ID("sdl")) {
        SDL_OpenURL(
            "https://github.com/libsdl-org/SDL/"
            );
    } else if ( id == UI_ID("emscripten")) {
        SDL_OpenURL(
            "https://emscripten.org/"
            );
    } else if ( id == UI_ID("fastnoise")) {
        SDL_OpenURL(
            "https://github.com/Auburn/FastNoiseLite"
            );
    } else if ( id == UI_ID("born2bsporty")) {
        SDL_OpenURL(
            "https://www.pentacom.jp/pentacom/bitfontmaker2/gallery/?id=383"
        );
    }
}


GAME_API void game_keyboard_input(Game* game, SDL_KeyboardEvent event) {
    switch (event.key) {
        default: return;
    case SDLK_UP:
    case SDLK_W:
        input_move_north(game); return;
    case SDLK_S:
    case SDLK_DOWN: input_move_south(game); return;
    case SDLK_A:
    case SDLK_LEFT: input_move_west(game); return;
    case SDLK_D:
    case SDLK_RIGHT: input_move_east(game); return;
    case SDLK_Q: action_campfire(game); return;
    case SDLK_E: action_chop_tree(game); return;
    }
}