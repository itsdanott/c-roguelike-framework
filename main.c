/*
* C ROGUELIKE FRAMEWORK ********************************************************
    This is a lightweight C framework in preparation for 7drl 2025.
    It also serves as a learning project to get started with SDL3.

    For simplicity, we use GL 3.3 core for all desktop platforms and GL ES 3.0
    for web as the feature set of these two gl versions is quite close to each
    other.

    Target platforms:
        - Windows
        - Linux
        - macOS
        - Emscripten

    Third Party Libs:
        - SDL3
        - miniaudio
        - stb_image
        - stb_truetype
        - FastNoise Lite

    TODO:
        - Arena allocator
        - replace sdl func defs with SDL ones to avoid C runtime library

*******************************************************************************/

#define STB_IMAGE_IMPLEMENTATION
#include "third-party/stb/stb_image.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "third-party/stb/stb_truetype.h"

#include <glad/glad.h>

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* INTEGERS *******************************************************************/
typedef Sint8 i8;
typedef Sint16 i16;
typedef Sint32 i32;
typedef Sint64 i64;
typedef Uint8 u8;
typedef Uint16 u16;
typedef Uint32 u32;
typedef Uint64 u64;

/* GLOBALS ********************************************************************/
const char* APP_TITLE = "ROGUELIKE GAME";
const char* APP_VERSION = "0.1.0";
const char* APP_IDENTIFIER = "com.otone.roguelike";

#define SDL_WINDOW_WIDTH  800
#define SDL_WINDOW_HEIGHT 600

#define TARGET_FPS 30
const u64 TICK_RATE_IN_MS = (u64)(1.0f / (float)TARGET_FPS * 1000.0f);
const float DELTA_TIME = (float)TICK_RATE_IN_MS / 1000.0f;

#define FONT_SIZE 18 //18 is the upper limit for Tiny.ttf and 128px Texture size
#define FONT_TEXTURE_SIZE 128
#define FONT_TAB_SIZE 1.5f
#define FONT_SPACE_SIZE 0.25f
//see https://en.wikipedia.org/wiki/List_of_Unicode_characters#Control_codes
//for the entire Unicode range we use 96 characters
#define FONT_UNICODE_START 32
#define FONT_UNICODE_RANGE 96

const char* GLSL_SOURCE_HEADER =
#if defined(SDL_PLATFORM_EMSCRIPTEN)
    "#version 300 es\n"
#else
    "#version 330 core\n"
#endif
    "precision mediump float;\n";
const char* test_shader_vert =
    "layout(location=0) in vec3 inPos;\n"
    "layout(location=1) in vec3 inCol;\n"
    "out vec3 Color;\n"
    "void main() {\n"
    "   gl_Position = vec4(inPos, 1.0);\n"
    "   Color = inCol;\n"
    "}";
const char* test_shader_frag =
    "in vec3 Color;\n"
    "out vec4 FragColor;\n"
    "void main() {\n"
    "   FragColor = vec4(Color.rgb, 1.0);\n"
    "}";
const char* rect_shader_vert =
    "layout(location = 0) in vec2 inPos;\n"
    "layout(location = 1) in vec3 inColor;\n"
    "layout(location = 2) in vec2 inTexCoord;\n"
    "layout(location = 3) in float inSortOrder;\n"
    "out vec2 TexCoords;\n"
    "out vec3 Color;\n"
    "uniform mat4 projection;\n"
    "void main(){\n"
    "    gl_Position = projection * vec4(inPos.xy, inSortOrder, 1.0);\n"
    "    Color = inColor;\n"
    "    TexCoords = inTexCoord;\n"
    "}";
const char* rect_shader_frag =
    "in vec2 TexCoords;\n"
    "in vec3 Color;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D textureSampler;\n"
    "uniform float alphaClipThreshold;\n"
    "void main() {\n"
    "    //TODO: Find out how stb_tt deals with the y-axis for the glyphs. For now we'll simply hardcode the flip here\n"
    "    vec4 sampleColor = texture(textureSampler, vec2(TexCoords.x, 1.0 - TexCoords.y));\n"
    "    if(sampleColor.a < alphaClipThreshold) {\n"
    "        discard;\n"
    "    }\n"
    "    FragColor = vec4(sampleColor.rgb * Color, 1.0);\n"
    "}";

#define MAX_PATH_LEN 1024

const char PATH_SLASH =
#if defined(SDL_PLATFORM_WIN32)
    '\\';
#else
    '/';
#endif

char TEMP_PATH[MAX_PATH_LEN];

const char* FONT_PATH = "font-tiny/tiny.ttf";

//For desktop this is 32 - but we'll go with the lowest denominator: web
#define MAX_TEXTURE_SLOTS 16

#define RECT_BUFFER_CAPACITY 128
#define RECT_VERTEX_BUFFER_CAPACITY (RECT_BUFFER_CAPACITY*6)

/* MISC ***********************************************************************/
typedef struct {
    float pos_x;
    float pos_y;
} Player;

typedef struct {
    Player player;
} Game;

typedef struct {
    float pos_x, pos_y;
} Mouse;

typedef struct {
    SDL_Window* sdl;
    SDL_GLContext gl_context;
    i32 width, height;
    bool fullscreen;
} Window;

/* MATH ***********************************************************************/
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

vec2 vec2_mul_float(const vec2 vec, const float fac) {
    return (vec2){
        vec.x * fac,
        vec.y * fac,
    };
}

vec2 vec2_mul_vec2(const vec2 a, const vec2 b) {
    return (vec2){
        a.x * b.x,
        a.y * b.y,
    };
}

vec2 vec2_add_vec2(const vec2 a, const vec2 b) {
    return (vec2){
        a.x + b.x,
        a.y + b.y,
    };
}

vec2 vec2_sub_vec2(const vec2 a, const vec2 b) {
    return (vec2){
        a.x - b.x,
        a.y - b.y,
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

mat4 mat4_ortho(
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

/* COLORS *********************************************************************/
#define COLOR_RED       (vec3){1.0f,0.0f,0.0f}
#define COLOR_GREEN     (vec3){0.0f,1.0f,0.0f}
#define COLOR_BLUE      (vec3){0.0f,0.0f,1.0f}
#define COLOR_WHITE     (vec3){1.0f,1.0f,1.0f}

/* RENDERER *******************************************************************/
typedef struct {
    u32 vao, vbo;
} Renderer;

void renderer_init(Renderer* renderer) {
    glGenBuffers(1, &renderer->vbo);
    SDL_assert(renderer->vbo != 0);

    glGenVertexArrays(1, &renderer->vao);
    SDL_assert(renderer->vao != 0);
}

void renderer_bind(const Renderer* renderer) {
    SDL_assert(renderer != NULL);
    SDL_assert(renderer->vao != 0);
    SDL_assert(renderer->vbo != 0);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
    glBindVertexArray(renderer->vao); //TODO: really required for drawing?
}

void renderer_cleanup(const Renderer* renderer) {
    SDL_assert(renderer->vbo != 0);
    glDeleteBuffers(1, &renderer->vbo);

    SDL_assert(renderer->vao != 0);
    glDeleteVertexArrays(1, &renderer->vao);
}

/* PATH ***********************************************************************/
typedef struct {
    size_t length;
    char str[MAX_PATH_LEN];
} Path;

void asset_path_init(Path* asset_path) {
    const char* base_path = SDL_GetBasePath();
    const char* asset_str = "assets";
    const size_t str_len = SDL_strlen(base_path) + SDL_strlen(asset_str) + 4;
    SDL_assert(str_len < MAX_PATH_LEN);

#if defined(SDL_PLATFORM_EMSCRIPTEN)
    SDL_snprintf(asset_path->str, MAX_PATH_LEN, "%s%s%c",
     base_path, asset_str, PATH_SLASH);
#else
    SDL_snprintf(asset_path->str, MAX_PATH_LEN, "%s..%c%s%c",
                 base_path, PATH_SLASH, asset_str, PATH_SLASH);
#endif
    asset_path->length = SDL_strlen(asset_path->str);
}

char* temp_path_append(const char* base, const char* file) {
    SDL_assert(SDL_strlen(base) + SDL_strlen(file) < MAX_PATH_LEN);
    SDL_snprintf(TEMP_PATH, MAX_PATH_LEN, "%s%s", base, file);
    return TEMP_PATH;
}

/* TEXTURE ********************************************************************/
typedef enum {
    //loaded via STB_Image
    TEXTURE_RAW_SOURCE_STB_IMAGE,
    //loaded from Memory
    TEXTURE_RAW_SOURCE_DYNAMIC,
} Texture_Raw_Source;

typedef struct {
    i32 width, height, channels;
    u8* data;
    Texture_Raw_Source source;
} Raw_Texture;

typedef struct {
    u32 id;
    i32 width, height, channels;
} GL_Texture;

typedef struct {
    i32 internal_format;
    u32 format;
} GL_Texture_Format;

typedef struct {
    bool filter;
    bool repeat;
    bool gamma_correction;
} Texture_Config;

Texture_Config TEXTURE_CONFIG_DEFAULT = (Texture_Config){
    .filter = false,
    .repeat = true,
    .gamma_correction = false,
};

Texture_Config TEXTURE_CONFIG_DEFAULT_GAMMACORRECT = (Texture_Config){
    .filter = false,
    .repeat = true,
    .gamma_correction = true,
};

Raw_Texture raw_texture_rgba_from_single_channel(
    const u8* single_channel_data,
    const i32 width, const i32 height
) {
    u8* data = SDL_malloc(sizeof(u8) * width * height * 4);
    for (i32 i = 0; i < width * height; i++) {
        u8 gray_scale = single_channel_data[i];
        data[i * 4 + 0] = gray_scale; //R
        data[i * 4 + 1] = gray_scale; //G
        data[i * 4 + 2] = gray_scale; //B
        data[i * 4 + 3] = gray_scale; //A
    }
    return (Raw_Texture){
        .width = width,
        .height = height,
        .channels = 4,
        .data = data,
        .source = TEXTURE_RAW_SOURCE_DYNAMIC,
    };
}

void raw_texture_free(Raw_Texture* texture) {
    SDL_assert(texture != NULL);
    SDL_assert(texture->data != NULL);
    SDL_free(texture->data);
    texture->data = NULL;
    texture = NULL;
}

void gl_texture_bind(const GL_Texture* texture, const u32 slot) {
    SDL_assert(texture != NULL);
    SDL_assert(texture->id > 0);
    SDL_assert(slot < MAX_TEXTURE_SLOTS);

    // TODO: optimize via caching the active texture!
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, texture->id);
}

GL_Texture_Format gl_texture_get_format(
    const i32 channels,
    const bool gamma_correct
) {
    switch (channels) {
    case 1:
#if defined(SDL_PLATFORM_EMSCRIPTEN)
        return (GL_Texture_Format){
            .internal_format = GL_LUMINANCE,
            .format = GL_LUMINANCE,
        };
#else
        return (GL_Texture_Format){
            .internal_format = GL_RED,
            .format = GL_RED,
        };
#endif
    case 3:
        return (GL_Texture_Format){
            .internal_format = gamma_correct ? GL_SRGB : GL_RGB,
            .format = GL_RGB,
        };
    case 4:
        return (GL_Texture_Format){
            .internal_format = gamma_correct ? GL_SRGB_ALPHA : GL_RGBA,
            .format = GL_RGBA,
        };
    default:
        SDL_assert(false);
    }
    return (GL_Texture_Format){0};
}

void texture_apply_config(const u32 target, const Texture_Config config) {
    if (config.repeat) {
        glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_REPEAT);
    } else {
        glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    if (config.filter) {
        glTexParameteri(
            target,GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
}

GL_Texture gl_texture_from_raw_texture(
    const Raw_Texture* raw_texture,
    const Texture_Config config
) {
    GL_Texture texture = {0};
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4); //<-only relevant for web?
    glGenTextures(1, &texture.id);
    SDL_assert(texture.id > 0);
    gl_texture_bind(&texture, 0);
    GL_Texture_Format format = gl_texture_get_format(raw_texture->channels,
        config.gamma_correction);
    texture_apply_config(GL_TEXTURE_2D, config);

    glTexImage2D(GL_TEXTURE_2D, 0, format.internal_format, raw_texture->width,
                 raw_texture->height, 0, format.format, GL_UNSIGNED_BYTE,
                 raw_texture->data);

    glGenerateMipmap(GL_TEXTURE_2D);

    return texture;
}

void gl_texture_delete(const GL_Texture* texture) {
    SDL_assert(texture != NULL);
    SDL_assert(texture->id > 0);
    glDeleteTextures(1, &texture->id);
}

/* FONT ***********************************************************************/
typedef struct {
    stbtt_fontinfo info;
    stbtt_pack_context pack_context;
    stbtt_packedchar char_data[FONT_UNICODE_RANGE];
    GL_Texture texture;
} Font;

//Loads the ttf, packs the glyphs it to an atlas and generates a rgba texture
//If we will switch to texture arrays later on we'll need to split the logic.
bool font_load(const char* file_path, Font* font) {
    SDL_assert(font != NULL);

    SDL_Log("Path: %s", file_path);

    SDL_IOStream* io_stream = SDL_IOFromFile(file_path, "r");
    size_t data_size = 0;
    u8* file_data = SDL_LoadFile_IO(io_stream, &data_size, false);
    SDL_CloseIO(io_stream);
    *font = (Font){0};
    if (!stbtt_InitFont(&font->info, file_data, 0)) {
        SDL_Log("Failed init font!");
        return false;
    }

    u8 pixels[FONT_TEXTURE_SIZE * FONT_TEXTURE_SIZE];
    if (!stbtt_PackBegin(
        &font->pack_context, &pixels[0],
        FONT_TEXTURE_SIZE, FONT_TEXTURE_SIZE,
        0, 1, NULL)) {
        SDL_Log("Failed to font pack begin!");
        return false;
    }

    if (!stbtt_PackFontRange(&font->pack_context, file_data, 0, FONT_SIZE,
                             FONT_UNICODE_START, FONT_UNICODE_RANGE,
                             font->char_data)) {
        SDL_Log("Failed to pack font range!");
        return false;
    }

    stbtt_PackEnd(&font->pack_context);
    SDL_free(file_data);

    Raw_Texture font_raw_texture = raw_texture_rgba_from_single_channel(
        pixels, FONT_TEXTURE_SIZE,FONT_TEXTURE_SIZE);
    font->texture = gl_texture_from_raw_texture(&font_raw_texture,
                                                TEXTURE_CONFIG_DEFAULT);
    raw_texture_free(&font_raw_texture);

    return true;
}

void font_delete(const Font* font) {
    SDL_assert(font != NULL);
    gl_texture_delete(&font->texture);
}

/* SHADER *********************************************************************/
typedef enum {
    SHADER_TYPE_VERTEX,
    SHADER_TYPE_FRAGMENT,
} Shader_Type;

typedef struct {
    u32 id;
    Shader_Type type;
} Shader;

typedef struct {
    u32 id;
} Shader_Program;

bool check_shader_compilation(const u32 shader) {
    int success;
    int shaderType;

    glGetShaderiv(shader, GL_SHADER_TYPE, &shaderType);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        SDL_Log("shader compilation failed: %s", infoLog);
        return false;
    }

    return true;
}

Shader compile_shader(const char* source, const Shader_Type type) {
    Shader shader = {0};
    GLenum gl_shader_type = 0;
    switch (type) {
    case SHADER_TYPE_VERTEX:
        gl_shader_type = GL_VERTEX_SHADER;
        break;
    case SHADER_TYPE_FRAGMENT:
        gl_shader_type = GL_FRAGMENT_SHADER;
        break;
    }
    shader.id = glCreateShader(gl_shader_type);
    const char* strs[] = {GLSL_SOURCE_HEADER, source};
    glShaderSource(shader.id, 2, strs, NULL);
    glCompileShader(shader.id);
    SDL_assert(check_shader_compilation(shader.id));
    return shader;
}

void delete_shader(Shader* shader) {
    glDeleteShader(shader->id);
    shader->id = 0;
}

Shader_Program link_shaders(const Shader* vertex, const Shader* fragment) {
    Shader_Program program = {0};
    program.id = glCreateProgram();
    glAttachShader(program.id, vertex->id);
    glAttachShader(program.id, fragment->id);
    glLinkProgram(program.id);
    return program;
}

Shader_Program compile_shader_program(const char* vert, const char* frag) {
    Shader vert_shader = compile_shader(vert, SHADER_TYPE_VERTEX);
    Shader frag_shader = compile_shader(frag, SHADER_TYPE_FRAGMENT);
    const Shader_Program shader = link_shaders(&vert_shader, &frag_shader);
    delete_shader(&vert_shader);
    delete_shader(&frag_shader);
    return shader;
}

void delete_shader_program(const Shader_Program* shader) {
    SDL_assert(shader != NULL);
    SDL_assert(shader->id != 0);
    glDeleteProgram(shader->id);
}

/* RECT RENDERING *************************************************************/
/*
    Instead of the previous 'single-buffer-partial-update-only-on-change'
    caching approach we're aiming for an immediate mode approach this time.
 */

typedef struct {
    vec2 pos;
    vec2 size;
    vec3 color;
    float sort_order;
    //the vertices of the rect will be aligned relative to the pivot
    //value range 0-1, where 0 = left/bottom, 1 = right/top, {0.5,0.5} = center
    vec2 pivot;
    //0 = Bottom-Left | 1 = Bottom-Right | 2 = Top-Right | 3 = Top-Left
    vec2 tex_coords[4];
} Rect;

typedef struct {
    Rect rects[RECT_BUFFER_CAPACITY];
    size_t curr_len;
} Rect_Buffer;

void render_text(
    const char* text,
    const Font* font,
    const vec2 pos,
    const vec3 color,
    const float scale,
    const float sort_order,
    Rect_Buffer* rect_buffer
) {
    float x = 0, y = 0;
    i32 glyph_iterator = 0;
    const size_t text_len = SDL_strlen(text);
    const vec2 adjusted_pos = pos;
    //TODO: precalculate the text bounds and add vh centering functionality!

    Rect rect = (Rect){
        .color = color,
        .pivot = {0.0f, 0.0f},
        .sort_order = sort_order,
    };

    for (int i = 0; i < text_len; i++) {
        const char c = text[i];
        switch (c) {
        case '\n':
            x = 0;
            y += FONT_SIZE * scale;
            continue;

        case '\t':
            x += FONT_TAB_SIZE * scale;
            continue;

        //BUG: this does not scale properly
        // case ' ':
        //     x += FONT_SPACE_SIZE * scale;
        //     continue;
        default: break;
        }

        stbtt_aligned_quad quad = {0};
        stbtt_GetPackedQuad(font->char_data, FONT_TEXTURE_SIZE,
                            FONT_TEXTURE_SIZE, (i32)c - FONT_UNICODE_START,
                            &x, &y, &quad, true);

        quad.x0 *= scale;
        quad.x1 *= scale;
        quad.y0 *= scale;
        quad.y1 *= scale;

        quad.x0 = quad.x0 + adjusted_pos.x;
        quad.x1 = quad.x1 + adjusted_pos.x;
        quad.y0 = -quad.y0 + adjusted_pos.y;
        quad.y1 = -quad.y1 + adjusted_pos.y;

        rect.pos = (vec2){quad.x0, quad.y1};
        rect.size = (vec2){
            SDL_fabsf(quad.x0 - quad.x1),
            SDL_fabsf(quad.y0 - quad.y1)
        };

        rect.tex_coords[0] = (vec2){quad.s0, 1.0f - quad.t1};
        rect.tex_coords[1] = (vec2){quad.s1, 1.0f - quad.t1};
        rect.tex_coords[2] = (vec2){quad.s0, 1.0f - quad.t0};
        rect.tex_coords[3] = (vec2){quad.s1, 1.0f - quad.t0};

        rect_buffer->rects[rect_buffer->curr_len + glyph_iterator] = rect;
        SDL_assert(
            rect_buffer->curr_len + glyph_iterator < RECT_BUFFER_CAPACITY);
        glyph_iterator += 1;
    }

    rect_buffer->curr_len += glyph_iterator;
}

typedef struct {
    vec2 pos;
    vec3 color;
    vec2 tex_coord;
    float sort_order;
} Rect_Vertex;

typedef struct {
    Rect_Vertex vertices[RECT_VERTEX_BUFFER_CAPACITY];
    size_t curr_len;
} Rect_Vertex_Buffer;

void build_rect_vertex_buffer(
    const Rect_Buffer* rect_buffer,
    Rect_Vertex_Buffer* vertex_buffer
) {
    size_t vertex_index = 0;
    vertex_buffer->curr_len = 0;
    if (rect_buffer->curr_len == 0) return;
    for (int rect_index = 0; rect_index < rect_buffer->curr_len; rect_index++) {
        Rect rect = rect_buffer->rects[rect_index];
        vec2 half_size = vec2_mul_float(rect.size, 0.5f);
        Rect_Vertex vertex = (Rect_Vertex){
            .color = rect.color,
            .sort_order = rect.sort_order,
        };
        const vec2 pivot_offset = vec2_mul_vec2(rect.pivot, rect.size);

        vertex.pos = vec2_sub_vec2(rect.pos, pivot_offset);
        vertex.tex_coord = rect.tex_coords[0];
        const Rect_Vertex bottom_left = vertex;

        vertex.pos = vec2_sub_vec2(
            vec2_add_vec2(rect.pos, (vec2){rect.size.x, 0}), pivot_offset);
        vertex.tex_coord = rect.tex_coords[1];
        const Rect_Vertex bottom_right = vertex;

        vertex.pos = vec2_sub_vec2(
            vec2_add_vec2(rect.pos, (vec2){0, rect.size.y}), pivot_offset);
        vertex.tex_coord = rect.tex_coords[2];
        const Rect_Vertex top_left = vertex;

        vertex.pos = vec2_sub_vec2(
            vec2_add_vec2(rect.pos, rect.size), pivot_offset);
        vertex.tex_coord = rect.tex_coords[3];
        const Rect_Vertex top_right = vertex;

        //this can be improved to avoid vertex redundancy using an EBO
        vertex_buffer->vertices[vertex_index + 0] = bottom_left;
        vertex_buffer->vertices[vertex_index + 1] = bottom_right;
        vertex_buffer->vertices[vertex_index + 2] = top_left;

        vertex_buffer->vertices[vertex_index + 3] = top_left;
        vertex_buffer->vertices[vertex_index + 4] = top_right;
        vertex_buffer->vertices[vertex_index + 5] = bottom_right;

        vertex_index += 6;
    }
    SDL_assert(vertex_index <= RECT_VERTEX_BUFFER_CAPACITY);
    vertex_buffer->curr_len = vertex_index;
}

//This assumes shader and texture(s) are already bound.
void draw_rects(
    const Rect_Vertex_Buffer* vertex_buffer,
    const Renderer* renderer
) {
    if (vertex_buffer->curr_len == 0) return;
    renderer_bind(renderer);
    glBufferSubData(
        GL_ARRAY_BUFFER,
        0,
        (GLsizeiptr)(sizeof(Rect_Vertex) * vertex_buffer->curr_len),
        &vertex_buffer->vertices[0]);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertex_buffer->curr_len);
}

/* APP ************************************************************************/
typedef struct {
    Window window;
    Game game;
    Mouse mouse;
    u64 last_tick;
    Renderer test_renderer;
    Shader_Program test_shader;
    Path asset_path;
    Font font;
    Renderer rect_renderer;
    Shader_Program rect_shader;
    Rect_Buffer rect_buffer;
    Rect_Vertex_Buffer rect_vertex_buffer;
} App;

static App default_app() {
    return (App){
        .window = (Window){
            .width = SDL_WINDOW_WIDTH,
            .height = SDL_WINDOW_HEIGHT,
        },
    };
}

static bool app_init(App* app) {
    asset_path_init(&app->asset_path);
    const char* font_path = temp_path_append(app->asset_path.str, "tiny.ttf");
    if (!font_load(font_path, &app->font))
        return false;

    app->test_shader = compile_shader_program(
        test_shader_vert, test_shader_frag);
    app->rect_shader = compile_shader_program(
        rect_shader_vert, rect_shader_frag);

    renderer_init(&app->test_renderer);
    renderer_bind(&app->test_renderer);
    const float vertices[] = {
        // positions            // colors
        0.0f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f,
        0.5f, -0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
        -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
    };

    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 18,
                 &vertices[0], GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT,GL_FALSE, 6 * sizeof(float), NULL);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT,GL_FALSE, 6 * sizeof(float),
                          (void*)(3 * sizeof(float)));

    renderer_init(&app->rect_renderer);
    renderer_bind(&app->rect_renderer);
    const GLsizei rect_vertex_size = sizeof(Rect_Vertex);
    glBufferData(
        GL_ARRAY_BUFFER, rect_vertex_size * RECT_VERTEX_BUFFER_CAPACITY,
        NULL, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, rect_vertex_size,
                          (void*)offsetof(Rect_Vertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, rect_vertex_size,
                          (void*)offsetof(Rect_Vertex, color));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, rect_vertex_size,
                          (void*)offsetof(Rect_Vertex, tex_coord));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, rect_vertex_size,
                          (void*)offsetof(Rect_Vertex, sort_order));

    return true;
}

static void app_tick(App* app) {
    app->game.player.pos_x = app->mouse.pos_x;
    app->game.player.pos_y = app->mouse.pos_y;
}

static void app_draw(App* app) {
    glClearColor(0.125f, 0.125f, 0.125f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, app->window.width, app->window.height);

    //Hello Triangle
    glUseProgram(app->test_shader.id);
    renderer_bind(&app->test_renderer);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    //Rects
    app->rect_buffer.curr_len = 0;
    render_text(
        "Hello 7DRL 2025!",
        &app->font,
        (vec2){
            (float)app->window.width * 0.5f,
            (float)app->window.height * 0.5f,
        },
        COLOR_WHITE,
        2.0f, 0.0f, &app->rect_buffer);
    render_text(
        "This is another line!",
        &app->font,
        (vec2){
            (float)app->window.width * 0.5f,
            (float)app->window.height * 0.5f + FONT_SIZE * 1.5f,
        },
        COLOR_RED,
        2.0f, 0.0f, &app->rect_buffer);
    render_text(
        "+123456790 # HP!",
        &app->font,
        (vec2){
            (float)app->window.width * 0.5f,
            (float)app->window.height * 0.5f + FONT_SIZE * 3.0f,
        },
        COLOR_GREEN,
        2.0f, 0.0f, &app->rect_buffer);
    render_text(
        "!@#$%^&*()-+./,",
        &app->font,
        (vec2){
            (float)app->window.width * 0.5f,
            (float)app->window.height * 0.5f + FONT_SIZE * 4.5f,
        },
        COLOR_BLUE,
        2.0f, 0.0f, &app->rect_buffer);

    build_rect_vertex_buffer(&app->rect_buffer, &app->rect_vertex_buffer);
    glUseProgram(app->rect_shader.id);
    const mat4 ortho_mat = mat4_ortho(
        0, (float)app->window.width, 0, (float)app->window.height, -1.0f, 1.0f);
    const i32 projection_loc = glGetUniformLocation(
        app->rect_shader.id, "projection");
    glUniformMatrix4fv(projection_loc, 1, GL_FALSE,
                       (const GLfloat*)(&ortho_mat.matrix[0]));
    const i32 texture_loc = glGetUniformLocation(
        app->rect_shader.id, "textureSampler");
    glUniform1i(texture_loc, 0);
    gl_texture_bind(&app->font.texture, 0);
    const i32 loc_alpha_clip_threshold = glGetUniformLocation(
        app->rect_shader.id, "alphaClipThreshold");
    glUniform1f(loc_alpha_clip_threshold, 0.5f);
    draw_rects(&app->rect_vertex_buffer, &app->rect_renderer);

    SDL_GL_SwapWindow(app->window.sdl);
}

static void app_cleanup(App* app) {
    delete_shader_program(&app->test_shader);
    delete_shader_program(&app->rect_shader);
    font_delete(&app->font);
    renderer_cleanup(&app->test_renderer);
    renderer_cleanup(&app->rect_renderer);
    SDL_GL_DestroyContext(app->window.gl_context);
    if (app->window.sdl)
        SDL_DestroyWindow(app->window.sdl);
    SDL_free(app);
}

static void app_event_key_down(const App* app, const SDL_KeyboardEvent event) {
    switch (event.key) {
    case SDLK_SPACE:
        SDL_GetWindowFullscreenMode(app->window.sdl);
        SDL_SetWindowFullscreen(app->window.sdl, !app->window.fullscreen);
        return;
    default: return;
    }
}

/* SDL callbacks **************************************************************/
/*
 *  This will be called once before anything else. argc/argv work like they
 *  always do.
 *
 *  If this returns SDL_APP_CONTINUE, the app runs.
 *
 *  If it returns SDL_APP_FAILURE,the app calls SDL_AppQuit and terminates with
 *  an exit code that reports an error to the platform.
 *
 *  If it returns SDL_APP_SUCCESS, the app calls SDL_AppQuit and terminates with
 *  an exit code that reports success to the platform.
 *
 *  This function should not go into an infinite mainloop;
 *  it should do any one-time startup it requires and then return.
 *
 *  If you want to, you can assign a pointer to *appstate, and this pointer
 *  will be made available to you in later functions calls in their appstate
 *  parameter.
 *
 *  This allows you to avoid global variables, but is totally optional.
 *  If you don't set this, the pointer will be NULL in later function calls.
*/
SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv) {
    if (!SDL_SetAppMetadata(
            APP_TITLE,
            APP_VERSION,
            APP_IDENTIFIER)
    ) {
        SDL_Log("Failed to set SDL AppMetadata: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO)) {
        SDL_Log("Failed to initialize SDL: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    App* app = SDL_malloc(sizeof(App));
    if (app == NULL) {
        SDL_Log("Failed to set allocate APP memory: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    *app = default_app();
    *appstate = app;

    SDL_Window* window = SDL_CreateWindow(
        APP_TITLE,
        SDL_WINDOW_WIDTH, SDL_WINDOW_HEIGHT,
        SDL_WINDOW_MAXIMIZED | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        SDL_Log("Failed to create Window: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    app->window.sdl = window;

#if defined (SDL_PLATFORM_EMSCRIPTEN)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);
#endif
    //TODO: test if OPENGL_FORWARD_COMPAT is required for mac with sdl

    app->window.gl_context = SDL_GL_CreateContext(window);

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        SDL_Log("Failed to load OpenGL via Glad: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!app_init(app))
        return SDL_APP_FAILURE;

    app->last_tick = SDL_GetTicks();

    return SDL_APP_CONTINUE;
}

/* This is called over and over, possibly at the refresh rate of the display or
 * some other metric that the platform dictates. This is where the heart of your
 * app runs. It should return as quickly as reasonably possible, but it's not a
 * "run one memcpy and that's all the time you have" sort of thing.
 *
 * The app should do any game updates, and render a frame of video.
 *
 * If it returns SDL_APP_FAILURE, SDL will call SDL_AppQuit and terminate the
 * process with an exit code that reports an error to the platform.
 *
 * If it returns SDL_APP_SUCCESS, the app calls SDL_AppQuit and terminates with
 * an exit code that reports success to the platform.
 *
 * If it returns SDL_APP_CONTINUE, then SDL_AppIterate will be called again at
 * some regular frequency.
 *
 * The platform may choose to run this more or less (perhaps less in the
 * background, etc.), or it might just call this function in a loop as fast
 * as possible.
 *
 * You do not check the event queue in this function (SDL_AppEvent exists
 * for that).
 */
SDL_AppResult SDL_AppIterate(void* appstate) {
    App* app = (App*)appstate;
    const u64 now = SDL_GetTicks();

    SDL_GetMouseState(&app->mouse.pos_x, &app->mouse.pos_y);

    while ((now - app->last_tick) >= TICK_RATE_IN_MS) {
        app_tick(app);
        app->last_tick += TICK_RATE_IN_MS;
    }

    app_draw(app);
    return SDL_APP_CONTINUE;
}

/*
 * This will be called whenever an SDL event arrives.
 * Your app should not call SDL_PollEvent, SDL_PumpEvent, etc., as SDL
 * will manage all this for you.
 *
 * Return values are the same as from SDL_AppIterate(), so you can terminate in
 * response to SDL_EVENT_QUIT, etc.
 */
// ReSharper disable once CppParameterMayBeConstPtrOrRef
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    App* app = (App*)appstate;
    switch (event->type) {
    case SDL_EVENT_QUIT: return SDL_APP_SUCCESS;

    case SDL_EVENT_WINDOW_RESIZED:
        SDL_Log(
            "Window Resized: callback data: %dx%d",
            event->window.data1, event->window.data2
        );
        app->window.width = event->window.data1;
        app->window.height = event->window.data2;
        break;

    case SDL_EVENT_KEY_DOWN:
        app_event_key_down(app, event->key);
        break;

    case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
        app->window.fullscreen = true;
        break;

    case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
        app->window.fullscreen = false;
        break;

    default: return SDL_APP_CONTINUE;
    }
    return SDL_APP_CONTINUE;
}

/* This is called once before terminating the app--assuming the app isn't being
 * forcibly killed or crashed--as a last chance to clean up.
 *
 * After this returns, SDL will call SDL_Quit so the app doesn't have to
 * (but it's safe for the app to call it, too).
 *
 * Process termination proceeds as if the app returned normally from main(), so
 * atexit handles will run, if your platform supports that.
 *
 * If you set *appstate during SDL_AppInit, this is where you should free that
 * data, as this pointer will not be provided to your app again.
 *
 * The SDL_AppResult value that terminated the app is provided here, in case
 * it's useful to know if this was a successful or failing run of the app.
 */
void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    if (appstate == NULL) return;

    App* app = (App*)appstate;

    app_cleanup(app);
}