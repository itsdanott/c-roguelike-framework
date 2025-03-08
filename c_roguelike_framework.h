/*
* C ROGUELIKE FRAMEWORK ********************************************************

@@@   @@@    @     @@@
@     @ @    @     @
@     @@@    @     @@@
@@@   @  @   @@@   @

Lightweight C99 framework for 7drl 2025.

NOTE: Make sure to include SDL3 BEFORE including this header

*******************************************************************************/

#ifndef C_ROGUELIKE_FRAMEWORK_H
#define C_ROGUELIKE_FRAMEWORK_H

/* INTEGERS *******************************************************************/
typedef Sint8 i8;
typedef Sint16 i16;
typedef Sint32 i32;
typedef Sint64 i64;
typedef Uint8 u8;
typedef Uint16 u16;
typedef Uint32 u32;
typedef Uint64 u64;

/* STRING *********************************************************************/
typedef struct {
    i32 length;
    const char* chars;
} String;

static String make_string(const char* str) {
    return (String){
        .length = SDL_strlen(str),
        .chars = str,
    };
}

#define STRING(str) make_string(str)

/* MATH ***********************************************************************/
#define VEC2_ZERO (vec2){0}
#define VEC3_ZERO (vec3){0,0,0}
#define VEC4_ZERO (vec4){0,0,0,0}
#define VEC2_ONE  (vec2){1,1}
#define VEC3_ONE  (vec3){1,1,1}
#define VEC4_ONE  (vec4){1,1,1,1}

typedef struct {
    i32 x, y;
} ivec2;

typedef struct {
    i32 x, y, z;
} ivec3;

typedef struct {
    float x, y;
} vec2;

typedef struct {
    float x, y, z;
} vec3;

//NOTE: this violates ISO C99's unnamed structs/unions rule!
typedef struct {
    union {
        struct {
            float x, y, z, w;
        }; // For position
        struct {
            float r, g, b, a;
        }; // For color
    };
} vec4;

static vec2 vec2_mul_float(const vec2 vec, const float fac) {
    return (vec2){
        vec.x * fac,
        vec.y * fac,
    };
}

static vec3 vec3_mul_float(const vec3 vec, const float fac) {
    return (vec3){
        vec.x * fac,
        vec.y * fac,
        vec.z * fac,
    };
}

static vec2 vec2_mul_vec2(const vec2 a, const vec2 b) {
    return (vec2){
        a.x * b.x,
        a.y * b.y,
    };
}

static vec2 vec2_add_vec2(const vec2 a, const vec2 b) {
    return (vec2){
        a.x + b.x,
        a.y + b.y,
    };
}

static vec2 vec2_sub_vec2(const vec2 a, const vec2 b) {
    return (vec2){
        a.x - b.x,
        a.y - b.y,
    };
}

static vec2 vec2_sub_float(const vec2 a, const float b) {
    return (vec2){
        a.x - b,
        a.y - b,
    };
}

static vec2 vec2_div_float(const vec2 a, const float b) {
    return (vec2){
        a.x / b,
        a.y / b,
    };
}

static vec2 ivec2_to_vec2(const ivec2 a) {
    return (vec2){
        (float)a.x, (float)a.y
    };
}

//column-major mat
//that means we access elements by matrix[column][row] and what appears to be
//the first "row" when initializing a matrix is actually the first column.
//keep in mind that we use colum-major matrices as these are required by OpenGL
//this is a bit confusing as they look like row-major matrices when initializing
typedef struct {
    float matrix[4][4];
} mat4;

static mat4 mat4_ortho(
    const float left, const float right,
    const float bottom, const float top,
    const float z_near, const float z_far
) {
    mat4 result = {0};
    result.matrix[0][0] = 2.0f / (right - left);
    result.matrix[1][1] = 2.0f / (top - bottom);
    result.matrix[2][2] = -2.0f / (z_far - z_near);
    result.matrix[3][0] = -(right + left) / (right - left);
    result.matrix[3][1] = -(top + bottom) / (top - bottom);
    result.matrix[3][2] = -(z_far + z_near) / (z_far - z_near);
    result.matrix[3][3] = 1.0f;
    return result;
}

static float float_lerp(const float a, const float b, const float t) {
    return (1 - t) * a + t * b;
}

static vec2 vec2_lerp(const vec2 a, const vec2 b, const float t) {
    return (vec2){
        float_lerp(a.x, b.x, t),
        float_lerp(a.y, b.y, t),
    };
}

static vec3 vec3_lerp(const vec3 a, const vec3 b, const float t) {
    return (vec3){
        float_lerp(a.x, b.x, t),
        float_lerp(a.y, b.y, t),
        float_lerp(a.z, b.z, t),
    };
}

//djb2 hash function for strings with <= 32 characters
//by Daniel J. Bernstein (Public Domain)
//http://www.cse.yorku.ca/~oz/hash.html
static u32 short_str_hash(String str) {
    SDL_assert(str.length <= 32);

    u32 hash = 5381;
    int c;

    while ((c = *str.chars++)) {
        hash = ((hash << 5) + hash) + (u8)c; // hash * 33 + c
    }

    return hash;
}

/* COLORS *********************************************************************/
#define COLOR_RED			(vec3){1.00f,0.00f,0.00f}
#define COLOR_GREEN			(vec3){0.00f,1.00f,0.00f}
#define COLOR_BLUE			(vec3){0.00f,0.00f,1.00f}
#define COLOR_CYAN   		(vec3){0.00f,1.00f,1.00f}
#define COLOR_TEAL          (vec3){0.00f,0.50f,0.50f}
#define COLOR_YELLOW		(vec3){1.00f,1.00f,0.00f}
#define COLOR_MAGENTA		(vec3){1.00f,0.00f,1.00f}
#define COLOR_PURPLE		(vec3){0.50f,0.00f,0.50f}
#define COLOR_BLACK			(vec3){0.00f,0.00f,0.00f}
#define COLOR_GRAY			(vec3){0.50f,0.50f,0.50f}
#define COLOR_GRAY_BRIGHT	(vec3){0.75f,0.75f,0.75f}
#define COLOR_GRAY_DARK		(vec3){0.25f,0.25f,0.25f}
#define COLOR_WHITE			(vec3){1.00f,1.00f,1.00f}

/* ARENA ALLOC ****************************************************************/
typedef struct {
    u8* memory;
    size_t capacity;
    uintptr_t offset;
} Arena;

static Arena arena_init(const size_t capacity) {
    uint8_t* memory = SDL_malloc(capacity);
    SDL_assert(memory != NULL);
    const uintptr_t memory_address = (uintptr_t)memory;
    const uintptr_t next_alloc_offset = (memory_address % 64); //cache align
    return (Arena){
        .memory = memory,
        .capacity = capacity,
        .offset = next_alloc_offset,
    };
}

static void arena_cleanup(const Arena* arena) {
    SDL_assert(arena->memory != NULL);
    SDL_free(arena->memory);
}

static void* arena_alloc(Arena* arena, const size_t size) {
    SDL_assert(arena != NULL);
    SDL_assert(arena->offset + size < arena->capacity);
    void* ptr = arena->memory + arena->offset;
    arena->offset += size;
    return ptr;
}

static void arena_clear(Arena* arena) {
    SDL_assert(arena != NULL);
    arena->offset = 0;
}

/* TEX COORDS *****************************************************************/
//To avoid C's array weirdness we'll use a struct for tex_coords instead of an
//array(as we would in e.g. Odin)
typedef struct {
    vec2 bottom_left, bottom_right, top_left, top_right;
} Tex_Coords;

static Tex_Coords default_tex_coords() {
    return (Tex_Coords){
        .bottom_left = {0.0f, 0.0f},
        .bottom_right = {1.0f, 0.0f},
        .top_left = {0.0f, 1.0f},
        .top_right = {1.0f, 1.0f},
    };
}

/* UI *************************************************************************/
typedef struct UI_Context UI_Context;
extern UI_Context* ui_ctx;

#define UI_MAX_ELEMENTS 2048
#define UI_STRING_ARENA_SIZE 2048

#define UI_SIZE_FIXED(px)(UI_Element_Size){                                    \
    .mode = UI_ELEMENT_SIZE_MODE_FIXED,                                        \
    .value = px                                                                \
}
#define UI_SIZE_PERCENT(percent)(UI_Element_Size){                             \
    .mode = UI_ELEMENT_SIZE_MODE_PERCENT,                                      \
    .value = percent                                                           \
}
#define UI(...) \
    for ( \
        int UI_ELEM_MACRO_ITER = ((ui_element_start(), ui_container_element((UI_Container_Config)__VA_ARGS__)), 0);\
        UI_ELEM_MACRO_ITER < 1;\
        UI_ELEM_MACRO_ITER++, ui_element_end()\
    )
#define UI_ID(str) short_str_hash(STRING(str))
#define UI_ID_STR(str) short_str_hash(str)

#define UI_TEXT(text, ...) ui_text_element(text, (UI_Text_Config)__VA_ARGS__)

#define UI_IMAGE(...) ui_image_element((UI_Image_Config)__VA_ARGS__ )

#define UI_ANCHOR_CENTER (vec2){ .x = 0.5f, .y = 0.5f }
//TODO: decide about these macros - they offer simpler writing but are error-prone (implic type casting etc)
#define UI_POS(pos_x,pos_y) (vec2) { .x = pos_x, .y = pos_y }
#define UI_SIZE(width,height) (vec2) { .x = width, .y = height }

typedef enum {
    UI_ELEMENT_TYPE_NONE,
    UI_ELEMENT_TYPE_CONTAINER,
    UI_ELEMENT_TYPE_TEXT,
    UI_ELEMENT_TYPE_IMAGE,
} UI_Element_Type;

typedef enum {
    UI_ALIGNMENT_X_CENTER,
    UI_ALIGNMENT_X_LEFT,
    UI_ALIGNMENT_X_RIGHT,
} UI_Alignment_X;

typedef enum {
    UI_ALIGNMENT_Y_CENTER,
    UI_ALIGNMENT_Y_BOTTOM,
    UI_ALIGNMENT_Y_TOP,
} UI_Alignment_Y;

typedef struct {
    UI_Alignment_X x;
    UI_Alignment_Y y;
} UI_Alignment;

typedef struct {
    vec2 anchor;
    vec2 offset;
    vec2 size;
    //TODO: consider generalizing pivot(useful everywhere - not only for UI_Image)
} UI_Element_Layout;

typedef struct {
    u32 id;
    UI_Element_Layout layout;
    vec3 bg_color;
    i32 nine_slice_id;
    bool blocks_cursor;
    bool is_hidden;
    bool is_slice_center_hidden;
} UI_Container_Config;

typedef struct {
    float width;
    float height;
    float font_height;
    int num_lines;
} UI_Text_Dimension;

typedef struct {
    //TODO: text wrapping rules (e.g. FIT and NO_WRAP)
    u32 id;
    UI_Element_Layout layout;
    u32 font;
    String text;
    float scale;
    UI_Alignment align;
    vec3 color;
    vec3 outline_color;
    float outline;
    bool bg_slice;
    i32 bg_slice_id;
    //Calculated during the size_pos pass
    UI_Text_Dimension _dimension;
    //This gets calculated during the size_pos pass
    float _screen_scale;
    //TODO: if there is more type-specific data later on consider adding more structs and another union for that!
} UI_Text_Config;

typedef struct {
    i32 row;
    i32 column;
} Texture_Atlas_Cell;

typedef enum {
    UI_IMAGE_TEX_MODE_FULL,
    UI_IMAGE_TEX_MODE_ATLAS_CELL_INDEX,
    UI_IMAGE_TEX_MODE_ATLAS_ROW_COLUMN,
    UI_IMAGE_TEX_MODE_BY_VALUE,
} UI_Image_Tex_Mode;

typedef struct {
    UI_Image_Tex_Mode mode;

    union {
        i32 cell_index;
        Texture_Atlas_Cell cell;
        Tex_Coords value;
    } data;
} UI_Image_Tex_Coords;

static UI_Image_Tex_Coords ui_image_tex_coords_atlas_row_colum(
    const i32 row, const i32 column
) {
    return (UI_Image_Tex_Coords){
        .mode = UI_IMAGE_TEX_MODE_ATLAS_ROW_COLUMN,
        .data = {
            .cell = {
                .row = row,
                .column = column
            }
        }
    };
}

static UI_Image_Tex_Coords ui_image_tex_coords_atlas_cell_index(
    const i32 cell_index
) {
    return (UI_Image_Tex_Coords){
        .mode = UI_IMAGE_TEX_MODE_ATLAS_CELL_INDEX,
        .data = {
            .cell_index = cell_index
        }
    };
}

static UI_Image_Tex_Coords ui_image_tex_coords_by_value(
    const Tex_Coords tex_coords
) {
    return (UI_Image_Tex_Coords){
        .mode = UI_IMAGE_TEX_MODE_BY_VALUE,
        .data = {
            .value = tex_coords
        }
    };
}

typedef struct {
    i32 id;
    //index inside the texture resources array! not to be confused with opengl ids!
    UI_Image_Tex_Coords coords;
} UI_Image_Texture;

typedef struct {
    u32 id;
    UI_Image_Texture texture;
    UI_Element_Layout layout;
    vec3 color;
    vec2 pivot;
    bool blocks_cursor;
} UI_Image_Config;

typedef struct {
    size_t index;
    size_t depth;
    UI_Element_Type type;
    UI_Element_Layout layout;

    union {
        UI_Container_Config container;
        UI_Text_Config text;
        UI_Image_Config image;
    } config;

    size_t first_child_index;
    size_t child_count;

    //Calculated during the size_pos pass
    vec2 _adjust_pos; // in square coords
    vec2 _adjusted_size; // in square coords
    //Converted during the size_pos Pass
    vec2 _screen_pos; //adjusted_pos converted to screen coords
    vec2 _screen_size; //adjusted_size converted to screen coords
} UI_Element;

// This is for converting the 'virtual 1000x1000 pixel' values to the actual
// ui_viewport's pixel values
typedef struct {
    float scale_fac; //decimal factor - to avoid division by 1000.f everywhere
    float size; //actual viewport pixel size of a square edge
    vec2 center;
    vec2 origin; //Bottom Left Pos
} UI_Render_Square;

typedef struct {
    bool is_hovering;
    size_t hover_element_index; //only valid when is_hovering is true
    u32 hover_id;
    //will be passed this to the next frame so that it can then be visualized by the layout (game.c)
    u32 down_id;
    bool is_start_touch;
} UI_Context_Input;

typedef struct {
    i32 unused;
} UI_Context_Debug;

struct UI_Context {
    vec2 viewport_size;
    vec2 cursor_pos;
    UI_Element elements[UI_MAX_ELEMENTS];
    size_t tree_depth;
    size_t elem_count;
    size_t temp_depth;
    UI_Element temp_elem;
    UI_Element temp_queue[UI_MAX_ELEMENTS];
    size_t temp_queue_count;
    Arena string_arena;
    UI_Render_Square square;
    UI_Context_Input input;
    UI_Context_Debug debug;
};

static void ui_element_start() {
    SDL_assert(ui_ctx->elem_count + 1 <= UI_MAX_ELEMENTS);
    const size_t new_index = ui_ctx->elem_count;

    //If we already have a temp elem - enqueue it as we are going one depth
    //level lower
    if (ui_ctx->temp_elem.type != UI_ELEMENT_TYPE_NONE) {
        if (ui_ctx->temp_elem.child_count == 0) {
            ui_ctx->temp_elem.first_child_index = new_index;
        }
        ui_ctx->temp_elem.child_count++;
        ui_ctx->temp_queue[ui_ctx->temp_queue_count++] = ui_ctx->temp_elem;
    }
    ui_ctx->temp_elem = (UI_Element){
        .index = new_index,
        .type = UI_ELEMENT_TYPE_CONTAINER,
        .depth = ui_ctx->temp_depth,
    };
    ui_ctx->elem_count++;
    ui_ctx->temp_depth++;
    ui_ctx->tree_depth = SDL_max(ui_ctx->temp_depth, ui_ctx->tree_depth);
}

static void ui_element_end() {
    ui_ctx->elements[ui_ctx->temp_elem.index] = ui_ctx->temp_elem;
    if (ui_ctx->temp_queue_count > 0) {
        ui_ctx->temp_queue_count--;
        ui_ctx->temp_elem = ui_ctx->temp_queue[ui_ctx->temp_queue_count];
    }
    ui_ctx->temp_depth--;
}

static void ui_element_set_layout(const UI_Element_Layout layout) {
    ui_ctx->temp_elem.layout = layout;
}

static void ui_container_element(const UI_Container_Config config) {
    ui_ctx->temp_elem.config.container = config;
    ui_element_set_layout(config.layout);
}

static void ui_text_element(
    const String text,
    const UI_Text_Config text_config
) {
    ui_element_start();
    ui_ctx->temp_elem.type = UI_ELEMENT_TYPE_TEXT;
    ui_ctx->temp_elem.config.text = text_config;
    ui_ctx->temp_elem.config.text.text = text;
    ui_element_set_layout(text_config.layout);
    ui_element_end();
}

static void ui_image_element(
    const UI_Image_Config config
) {
    ui_element_start();
    ui_ctx->temp_elem.type = UI_ELEMENT_TYPE_IMAGE;
    ui_ctx->temp_elem.config.image = config;
    ui_element_set_layout(config.layout);
    ui_element_end();
}

/* PUBLIC API ******************************************************************/
//TODO: as we now needed to link the gamelib to SDL the log functions are redundant
void log_msg(const char* fmt, ...);
void log_warning(const char* fmt, ...);
void log_error(const char* fmt, ...);

typedef struct {
    void (*log_msg)(const char* fmt, ...);
    void (*log_warning)(const char* fmt, ...);
    void (*log_error)(const char* fmt, ...);
} CRLF_API;

#endif //C_ROGUELIKE_FRAMEWORK_H
