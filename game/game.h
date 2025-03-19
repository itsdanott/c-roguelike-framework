/*
Roguelike Game Lib Template
The game.h and game.c files serve as a template for gameplay implementation.

*/

#ifndef GAME_H
#define GAME_H

#include "../third-party/FastNoiseLite/C/FastNoiseLite.h"

#include <SDL3/SDL.h>
#include "../c_roguelike_framework.h"

#if defined(WIN32) && !defined(__GAMELIB_STATIC_LINK__)
#define GAME_API __declspec(dllexport)
#else
#define GAME_API
#endif
/* PUBLIC GAME TYPES **********************************************************/

// Make sure to define the Game struct in the header as it is also a member
// of the framework's App struct.
typedef struct {
    //Fonts
    i32 font1;

    //Textures
    i32 nine_slice;
    i32 logo_crlf;
    i32 tiles;
    i32 tex_placeholder;
    i32 characters;

    //Nine Slice Rounded
    i32 nine_slice_rounded_black;
    i32 nine_slice_rounded_dark;
    i32 nine_slice_rounded_gray;
    i32 nine_slice_rounded_bright;

    //Nine Slice Square 01
    i32 nine_slice_square01_black;
    i32 nine_slice_square01_dark;
    i32 nine_slice_square01_gray;
    i32 nine_slice_square01_bright;
} Game_Resource_IDs;

typedef enum {
    GAME_STATE_MENU,
    GAME_STATE_NEW_GAME,
    GAME_STATE_SETTINGS,
    GAME_STATE_ABOUT_GAME,
    GAME_STATE_GAMEPLAY,
} Game_State;

typedef enum {
    TILE_TYPE_FOREST,
    TILE_TYPE_MOUNTAIN,
    TILE_TYPE_WATER,
    TILE_TYPE_GRASS,
    TILE_TYPE_GRID,//TOOD:remove
} Tile_Type;

typedef enum {
    CHARACTER_TYPE_NONE,
    CHARACTER_TYPE_MONARCH,
    CHARACTER_TYPE_DEER,
} Character_Type;

#define WORLD_SIZE 256
#define MAX_NPCs 128
#define START_NPC_NUM_DEER 96

#define TILE_INDEX(x, y) x + y * WORLD_SIZE

static void world_pos_from_tile_index(const size_t index, i32* x, i32* y) {
    *x = index % WORLD_SIZE;
    *y = index / WORLD_SIZE;
}

static ivec2 world_pos_from_tile_index_ivec2(const size_t index) {
    return (ivec2){
        .x = index % WORLD_SIZE,
        .y = index / WORLD_SIZE,
    };
}

static void get_direction_from_to(
    const i32 from_x, const i32 from_y,
    const i32 to_x, const i32 to_y,
    i32* direction_x, i32* direction_y
) {
    *direction_x = to_x - from_x;
    *direction_y = to_y - from_y;
}

static bool is_in_world_bounds(const i32 x, const i32 y) {
    return x < WORLD_SIZE && x >= 0 &&
           y < WORLD_SIZE && y >= 0;
}

typedef struct {
    Character_Type character;
    i32 pos_x;
    i32 pos_y;
    i32 target_pos_x;
    i32 target_pos_y;
    i32 action_breaks;
} NPC;

typedef struct {
    Tile_Type tiles[WORLD_SIZE * WORLD_SIZE];
    NPC npcs[MAX_NPCs];
    i32 num_npcs;
} World;

typedef struct {
    i32 pos_x;
    i32 pos_y;
} Player;

static Player default_player() {
    return (Player){
        .pos_x = WORLD_SIZE/2,
        .pos_y = WORLD_SIZE/2,
    };
}

typedef struct {
    Player player;
    i32 days;
    i32 wood;
    Game_State state;
    World world;
    i32 seed;
    fnl_state fnl;
    Random random;
    bool quit_requested;
} Game;

static Game default_game() {
    return (Game){
        .state = GAME_STATE_MENU,
        .player = default_player(),
        .seed = 1337,
    };
}

/* PUBLIC GAME API ************************************************************/
GAME_API bool game_init(
    CRLF_API* new_api, Game* game, UI_Context* ui, Game_Resource_IDs* res_ids
);
GAME_API void game_tick(Game* game, float dt);
GAME_API void game_draw(Game* game);
GAME_API void game_cleanup(Game* game);
GAME_API void game_ui_input(Game* game, u32 id);
GAME_API void game_keyboard_input(Game* game, SDL_KeyboardEvent event);

#endif //GAME_H
