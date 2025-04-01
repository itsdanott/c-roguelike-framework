/*
* C ROGUELIKE FRAMEWORK ********************************************************

@@@   @@@    @     @@@
@     @ @    @     @
@     @@@    @     @@@
@@@   @  @   @@@   @

Lightweight C99 framework made for 7drl 2025.

*******************************************************************************/

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "third-party/stb/stb_image.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "third-party/stb/stb_truetype.h"

#include <glad/glad.h>

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "c_roguelike_framework.h"

#include "game/game.h"

/* CRLF CONFIG*****************************************************************/
//use this define to make use of a dedicated game viewport (e.g. for 3d rendering)
// #define CRLF_USE_GAMEVIEWPORT

//use this define to add a scissor test to discard all stuff that is outside the
//game's square viewport
#define CRLF_USE_SQUARE_SCISSOR

/* DEBUG DEFINES **************************************************************/
#if !defined(__LEAK_DETECTION__)
#define CRLF_malloc SDL_malloc
#define CRLF_free SDL_free
#else
#define CRLF_malloc malloc
#define CRLF_free free
#if defined(__MSVC_CRT_LEAK_DETECTION__)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
#endif

/* GLOBALS ********************************************************************/
const char* APP_TITLE      = "ROGUELIKE GAME";
const char* APP_VERSION    = "0.1.0";
const char* APP_IDENTIFIER = "com.otone.roguelike";

#if defined(SDL_PLATFORM_EMSCRIPTEN)
#define APP_WINDOW_WIDTH  360
#define APP_WINDOW_HEIGHT 360
#else
#define APP_WINDOW_WIDTH  640
#define APP_WINDOW_HEIGHT 640
#endif

#define TICK_RATE_IN_MS 16
const float DELTA_TIME = ((float)TICK_RATE_IN_MS / (float)SDL_MS_PER_SECOND);

UI_Context* ui_ctx;

#define CRLF_TEXTURE_SIZE 128
#define FONT_TEXTURE_SIZE CRLF_TEXTURE_SIZE
#define FONT_TAB_SIZE 6
#define FONT_SPACE_SIZE 3
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
    "layout(location = 4) in int inTextureId;\n"
    "out vec2 TexCoords;\n"
    "out vec3 Color;\n"
    "flat out int TextureId;\n"
    "uniform mat4 projection;\n"
    "void main(){\n"
    "    gl_Position = projection * vec4(inPos.xy, inSortOrder, 1.0);\n"
    "    Color = inColor;\n"
    "    TexCoords = inTexCoord;\n"
    "    TextureId = inTextureId;\n"
    "}";
const char* rect_shader_frag =
    "in vec2 TexCoords;\n"
    "in vec3 Color;\n"
    "flat in int TextureId;\n"
    "out vec4 FragColor;\n"
    "uniform mediump sampler2DArray textureArray;\n"
    "uniform float alphaClipThreshold;\n"
    "void main() {\n"
    "    //TODO: Find out how stb_tt deals with the y-axis for the glyphs. For now we'll simply hardcode the flip here\n"
    "    vec4 sampleColor = texture(textureArray, vec3(TexCoords.x, 1.0 - TexCoords.y, float(TextureId)));\n"
    "    if(sampleColor.a < alphaClipThreshold) {\n"
    "        discard;\n"
    "    }\n"
    "    FragColor = vec4(sampleColor.rgb * Color, 1.0);\n"
    "}";
const char* viewport_shader_vert =
    "layout (location = 0) in vec2 inPos;\n"
    "layout (location = 1) in vec2 inTexCoords;\n"
    "out vec2 TexCoords;\n"
    "void main() {\n"
    "    gl_Position = vec4(inPos.x, inPos.y, 0.0, 1.0);\n"
    "    TexCoords = inTexCoords;\n"
    "}";

const char* viewport_shader_frag =
    "out vec4 FragColor;\n"
    "in vec2 TexCoords;\n"
    "uniform sampler2D viewportTexture;\n"
    "void main() {\n"
    "    vec4 fragColor = texture(viewportTexture, TexCoords);\n"
    "    float gamma = 2.2;\n"
    "    fragColor.rgb = pow(fragColor.rgb, vec3(1.0/gamma));\n"
    "    FragColor = fragColor;\n"
    "}";

#define MAX_PATH_LEN 1024

const char PATH_SLASH =
#if defined(SDL_PLATFORM_WIN32)
    '\\';
#else
    '/';
#endif

char TEMP_PATH[MAX_PATH_LEN];
//For desktop this is 32 - but we'll go with the lowest common denominator: web
#define MAX_TEXTURE_SLOTS 16

#define RECT_BUFFER_CAPACITY 2048
#define RECT_VERTEX_BUFFER_CAPACITY (RECT_BUFFER_CAPACITY*6)

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
    char   str[MAX_PATH_LEN];
} Path;

void asset_path_init(const char* base_path, Path* asset_path) {
    const char*  asset_str = "assets";
    const size_t str_len   = SDL_strlen(base_path) + SDL_strlen(asset_str) + 4;
    SDL_assert(str_len < MAX_PATH_LEN);

#if defined(SDL_PLATFORM_EMSCRIPTEN)
    SDL_snprintf(asset_path->str, MAX_PATH_LEN, "%s%s%c",
     base_path, asset_str, PATH_SLASH);
#else
    SDL_snprintf(
        asset_path->str, MAX_PATH_LEN, "%s..%c%s%c",
        base_path, PATH_SLASH, asset_str, PATH_SLASH
    );
#endif
    asset_path->length = SDL_strlen(asset_path->str);
}

char* temp_path_append(const char* base, const char* file) {
    SDL_assert(SDL_strlen(base) + SDL_strlen(file) < MAX_PATH_LEN);
    SDL_snprintf(TEMP_PATH, MAX_PATH_LEN, "%s%s", base, file);
    return TEMP_PATH;
}

/* TEXTURE ********************************************************************/
/*
    For 7drl 2025 we want to use a SINGLE texture array for all things ui.
 */
typedef enum {
    TEXTURE_RAW_SOURCE_STB_IMAGE, //loaded via STB_Image
    TEXTURE_RAW_SOURCE_DYNAMIC,   //loaded from Memory
} Raw_Texture_Source;

typedef struct {
    i32 rows;
    i32 columns;
} Texture_Atlas;

typedef struct {
    i32                width, height, channels;
    u8*                data;
    Raw_Texture_Source source;
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

Texture_Config default_texture_config() {
    return (Texture_Config){
        .filter = false,
        .repeat = true,
        .gamma_correction = false,
    };
}

Texture_Config default_texture_config_gammacorrect() {
    return (Texture_Config){
        .filter = false,
        .repeat = true,
        .gamma_correction = true,
    };
}

Raw_Texture* raw_texture_rgba_from_single_channel(
    const u8* single_channel_data,
    const i32 width, const i32 height
) {
    Raw_Texture* raw_texture = CRLF_malloc(sizeof(Raw_Texture));
    *raw_texture             = (Raw_Texture){
        .width = width,
        .height = height,
        .channels = 4,
        .data = CRLF_malloc(sizeof(u8) * width * height * 4),
        .source = TEXTURE_RAW_SOURCE_DYNAMIC,
    };

    for (i32 i = 0; i < width * height; i++) {
        const u8 gray_scale          = single_channel_data[i];
        raw_texture->data[i * 4 + 0] = gray_scale; //R
        raw_texture->data[i * 4 + 1] = gray_scale; //G
        raw_texture->data[i * 4 + 2] = gray_scale; //B
        raw_texture->data[i * 4 + 3] = gray_scale; //A
    }

    return raw_texture;
}

Raw_Texture* raw_texture_from_file(
    const char* file_path
) {
    SDL_PathInfo path_info;
    if (!SDL_GetPathInfo(file_path, &path_info) || path_info.type !=
        SDL_PATHTYPE_FILE) {
        SDL_LogError(0, "Invalid Path(%s): %s", file_path, SDL_GetError());
        return NULL;
    }

    Raw_Texture* texture = CRLF_malloc(sizeof(Raw_Texture));
    *texture             = (Raw_Texture){
        .source = TEXTURE_RAW_SOURCE_STB_IMAGE,
    };
    texture->data = stbi_load(
        file_path, &texture->width, &texture->height,
        &texture->channels, 0
    );
    return texture;
}

void raw_texture_free(Raw_Texture* texture) {
    SDL_assert(texture != NULL);
    SDL_assert(texture->data != NULL);
    switch (texture->source) {
    case TEXTURE_RAW_SOURCE_DYNAMIC:
        CRLF_free(texture->data);
        break;
    case TEXTURE_RAW_SOURCE_STB_IMAGE:
        stbi_image_free(texture->data);
        break;
    }
    texture->data = NULL;

    CRLF_free(texture);
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
    const i32  channels,
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
            //BUG: GL_SRGB_ALPHA in combination with texImage3D causes a crash in web:
            //INVALID_VALUE: texImage3D: invalid internalformat
            //GL_INVALID_OPERATION: Level of detail outside of range.
            //GL_INVALID_OPERATION: Texture format does not support mipmap generation.
#if defined(SDL_PLATFORM_EMSCRIPTEN)
            .internal_format = gamma_correct ? GL_SRGB8_ALPHA8 : GL_RGBA,
#else
            .internal_format = gamma_correct ? GL_SRGB_ALPHA : GL_RGBA,
#endif
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
            target,GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR
        );
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
}

GL_Texture gl_texture_from_raw_texture(
    const Raw_Texture*   raw_texture,
    const Texture_Config config
) {
    GL_Texture texture = {0};
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4); //<-only relevant for web?
    glGenTextures(1, &texture.id);
    SDL_assert(texture.id > 0);
    gl_texture_bind(&texture, 0);
    const GL_Texture_Format format = gl_texture_get_format(
        raw_texture->channels,
        config.gamma_correction
    );
    texture_apply_config(GL_TEXTURE_2D, config);

    glTexImage2D(
        GL_TEXTURE_2D, 0, format.internal_format, raw_texture->width,
        raw_texture->height, 0, format.format, GL_UNSIGNED_BYTE,
        raw_texture->data
    );

    glGenerateMipmap(GL_TEXTURE_2D);

    return texture;
}

void gl_texture_delete(const GL_Texture* texture) {
    SDL_assert(texture != NULL);
    SDL_assert(texture->id > 0);
    glDeleteTextures(1, &texture->id);
}

/* TEXTURE ARRAY **************************************************************/
typedef struct {
    u32            id;
    i32            num_textures;
    i32            width, height, channels;
    Texture_Config config;
} GL_Texture_Array;

void gl_texture_array_bind(
    const GL_Texture_Array* texture_array,
    const u32               slot
) {
    SDL_assert(texture_array != NULL);
    SDL_assert(texture_array->id > 0);
    SDL_assert(slot < MAX_TEXTURE_SLOTS);
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture_array->id);
}

GL_Texture_Array gl_texture_array_generate(
    Raw_Texture**        textures,
    const i32            num_textures,
    const i32            width,
    const i32            height,
    const i32            channels,
    const Texture_Config config,
    const bool           free_raw_textures
) {
    for (int i = 0; i < num_textures; i++) {
        SDL_assert(textures[i] != NULL);
        SDL_assert(textures[i]->width == width);
        SDL_assert(textures[i]->height == height);
        SDL_assert(textures[i]->channels == channels);
    }

    GL_Texture_Array texture_array = (GL_Texture_Array){
        .num_textures = num_textures,
        .width = width,
        .height = height,
        .channels = channels,
        .config = config,
    };

    const GL_Texture_Format format = gl_texture_get_format(
        channels, config.gamma_correction
    );

    glGenTextures(1, &texture_array.id);
    gl_texture_array_bind(&texture_array, 0);
    glTexImage3D(
        GL_TEXTURE_2D_ARRAY,
        0,
        format.internal_format,
        width,
        height,
        texture_array.num_textures,
        0,
        format.format,
        GL_UNSIGNED_BYTE,
        NULL
    );

    for (int i = 0; i < num_textures; i++) {
        glTexSubImage3D(
            GL_TEXTURE_2D_ARRAY,
            0, 0, 0,
            i, width, height, 1,
            format.format,
            GL_UNSIGNED_BYTE,
            &textures[i]->data[0]
        );
    }
    texture_apply_config(GL_TEXTURE_2D_ARRAY, config);
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

    if (free_raw_textures) {
        for (int i = 0; i < num_textures; i++) {
            raw_texture_free(textures[i]);
        }
    }

    return texture_array;
}

void texture_array_free(const GL_Texture_Array* texture_array) {
    SDL_assert(texture_array != NULL);
    SDL_assert(texture_array->id > 0);
    glDeleteTextures(1, &texture_array->id);
}

/* FONT ***********************************************************************/
typedef enum {
    FONT_TEXTURE_TYPE_SINGLE,
    FONT_TEXTURE_TYPE_ARRAY,
} Font_Texture_Type;

typedef struct {
    stbtt_fontinfo     info;
    stbtt_pack_context pack_context;
    stbtt_packedchar   char_data[FONT_UNICODE_RANGE];
    Font_Texture_Type  texture_type;
    float              size;

    union {
        GL_Texture texture;
        i32        texture_id;
    } texture_union;
} Font;

//Loads the ttf, packs the glyphs it to an atlas and generates a rgba texture
//If we will switch to texture arrays later on we'll need to split the logic.
Raw_Texture* font_load_raw_texture(
    const char* file_path,
    Font*       font,
    const float size
) {
    SDL_assert(font != NULL);
    SDL_PathInfo path_info;
    if (!SDL_GetPathInfo(file_path, &path_info) || path_info.type !=
        SDL_PATHTYPE_FILE) {
        SDL_LogError(0, "Invalid Path: %s", SDL_GetError());
        return NULL;
    }

    SDL_IOStream* io_stream = SDL_IOFromFile(file_path, "r");
    size_t        data_size = 0;
    u8*           file_data = SDL_LoadFile_IO(io_stream, &data_size, false);
    SDL_CloseIO(io_stream);
    *font = (Font){0};
    if (!stbtt_InitFont(&font->info, file_data, 0)) {
        SDL_LogError(0, "Failed init font!");
        return NULL;
    }

    u8 pixels[FONT_TEXTURE_SIZE * FONT_TEXTURE_SIZE];
    if (!stbtt_PackBegin(
        &font->pack_context, &pixels[0],
        FONT_TEXTURE_SIZE, FONT_TEXTURE_SIZE,
        0, 1, NULL
    )) {
        SDL_LogError(0, "Failed to font pack begin!");
        return NULL;
    }

    font->size = size;
    if (!stbtt_PackFontRange(
        &font->pack_context, file_data, 0, size,
        FONT_UNICODE_START, FONT_UNICODE_RANGE,
        font->char_data
    )) {
        SDL_LogError(0, "Failed to pack font range!");
        return NULL;
    }

    stbtt_PackEnd(&font->pack_context);
    CRLF_free(file_data);

    Raw_Texture* raw_texture = raw_texture_rgba_from_single_channel(
        pixels, FONT_TEXTURE_SIZE, FONT_TEXTURE_SIZE
    );

    return raw_texture;
}

bool font_load_single(const char* file_path, Font* font, const float size) {
    Raw_Texture* raw_texture = font_load_raw_texture(file_path, font, size);
    if (raw_texture == NULL)
        return false;

    font->texture_type          = FONT_TEXTURE_TYPE_SINGLE;
    font->texture_union.texture = gl_texture_from_raw_texture(
        raw_texture, default_texture_config()
    );
    raw_texture_free(raw_texture);

    return true;
}

Raw_Texture* font_load_for_array(
    const char* file_path, Font* font, const float size, const i32 texture_id
) {
    Raw_Texture* raw_texture = font_load_raw_texture(file_path, font, size);
    font->texture_type = FONT_TEXTURE_TYPE_ARRAY;
    font->texture_union.texture_id = texture_id;
    return raw_texture;
}

void font_delete(const Font* font) {
    SDL_assert(font != NULL);
    if (font->texture_type == FONT_TEXTURE_TYPE_SINGLE) {
        gl_texture_delete(&font->texture_union.texture);
    }
}

/* SHADER *********************************************************************/
typedef enum {
    SHADER_TYPE_VERTEX,
    SHADER_TYPE_FRAGMENT,
} Shader_Type;

typedef struct {
    u32         id;
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
        SDL_LogError(0, "shader compilation failed: %s", infoLog);
        return false;
    }

    return true;
}

Shader compile_shader(const char* source, const Shader_Type type) {
    Shader shader         = {
        .type = type,
    };
    GLenum gl_shader_type = 0;
    switch (type) {
    case SHADER_TYPE_VERTEX:
        gl_shader_type = GL_VERTEX_SHADER;
        break;
    case SHADER_TYPE_FRAGMENT:
        gl_shader_type = GL_FRAGMENT_SHADER;
        break;
    }
    shader.id          = glCreateShader(gl_shader_type);
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
    program.id             = glCreateProgram();
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

/* VIEWPORT *******************************************************************/
/*
    Viewports are frame buffers on which we can render to.
    They can be any size, and are scaled up or down to fit the screen when their
    when rendering their framebuffers to the window.

    For now, we are using a framebuffer divisor - but we might switch to a hard
    coded pixel dimension.

    Still to be decided: How to handle coordinates between different resolutions
    especially for UI layout and mouse click input handling.

    Viewport Layers:
    1. Game
    2. UI

 */
typedef struct {
    bool  is_initialized;
    vec2  screen_pos;
    ivec2 display_size;
    ivec2 frame_buffer_size;
    float aspect_ratio;
    u32   frame_buffer;
    u32   frame_buffer_texture;
    u32   render_buffer;

    //These members should be configured first
    vec4 clear_color;
    i32  frame_buffer_divisor;
    bool has_blending;
    bool has_depth_buffer;
    bool floating_point_precision;
} Viewport;

#if defined(CRLF_USE_GAMEVIEWPORT)
Viewport default_viewport_game() {
    return (Viewport){
        .screen_pos = (vec2){256, 0},
        .clear_color = (vec4){0.f, 0.f, 0.f, 1.f},
        .frame_buffer_divisor = 2,
        .has_blending = false,
        .has_depth_buffer = true,
        .floating_point_precision = false,
    };
}
#endif

Viewport default_viewport_ui() {
    return (Viewport){
        .clear_color =
#if defined(CRLF_USE_GAMEVIEWPORT)
         (vec4){0},//transparent
#else
        (vec4){0, 0, 0, 1},
#endif
        .frame_buffer_divisor = 1,
        .has_blending = true,
        .has_depth_buffer = true,
        .floating_point_precision = false,
    };
}

void viewport_cleanup(const Viewport* viewport) {
    if (!viewport->is_initialized) return;
    glDeleteFramebuffers(1, &viewport->frame_buffer);
    glDeleteTextures(1, &viewport->frame_buffer_texture);
    glDeleteRenderbuffers(1, &viewport->render_buffer);
}

i32 viewport_get_internal_format(
    const bool has_blending,
    const bool floating_point_precision
) {
    if (floating_point_precision) {
        return has_blending ? GL_RGBA16 : GL_RGB16;
    }
    return has_blending ? GL_RGBA : GL_RGB;
}

void viewport_generate(
    Viewport*   viewport,
    const ivec2 display_size
) {
    viewport_cleanup(viewport);
    viewport->display_size      = display_size;
    viewport->frame_buffer_size = (ivec2){
        display_size.x / viewport->frame_buffer_divisor,
        display_size.y / viewport->frame_buffer_divisor,
    };
    viewport->aspect_ratio = (float)display_size.x / (float)display_size.y;

    glGenFramebuffers(1, &viewport->frame_buffer);
    glBindFramebuffer(GL_FRAMEBUFFER, viewport->frame_buffer);

    glGenTextures(1, &viewport->frame_buffer_texture);
    glBindTexture(GL_TEXTURE_2D, viewport->frame_buffer_texture);
    glTexImage2D(
        GL_TEXTURE_2D, 0,
        viewport_get_internal_format(
            viewport->has_blending,
            viewport->floating_point_precision
        ),
        viewport->frame_buffer_size.x,
        viewport->frame_buffer_size.y,
        0,
        viewport->has_blending ? GL_RGBA : GL_RGB,
        //BUG: Floating point precision using GL_FLOAT is not supported in web, check GL ES 3.0 specs for an equivalent!
        viewport->floating_point_precision ? GL_FLOAT : GL_UNSIGNED_BYTE,
        NULL
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
        viewport->frame_buffer_texture, 0
    );

    glGenRenderbuffers(1, &viewport->render_buffer);
    glBindRenderbuffer(GL_RENDERBUFFER, viewport->render_buffer);

    if (viewport->has_depth_buffer) {
        glRenderbufferStorage(
            GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
            viewport->frame_buffer_size.x,
            viewport->frame_buffer_size.y
        );
        glFramebufferRenderbuffer(
            GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
            GL_RENDERBUFFER, viewport->render_buffer
        );
    }

    SDL_assert(
        glCheckFramebufferStatus(GL_FRAMEBUFFER) ==
        GL_FRAMEBUFFER_COMPLETE
    );

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    viewport->is_initialized = true;
}

void viewport_bind(const Viewport* viewport) {
    glViewport(
        0, 0,
        viewport->frame_buffer_size.x,
        viewport->frame_buffer_size.y
    );
    glBindFramebuffer(GL_FRAMEBUFFER, viewport->frame_buffer);
    glClearColor(
        viewport->clear_color.r,
        viewport->clear_color.g,
        viewport->clear_color.b,
        viewport->clear_color.a
    );
    if (viewport->has_depth_buffer) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
    } else {
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);
    }
}

//unbinds the current framebuffer and gets back to the full screen
void viewport_unbind(const i32 width, const i32 height) {
    glDisable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
}

void viewport_renderer_init(Renderer* renderer) {
    const float vertices[] = {
        //Position(XY)  TexCoord(XY)
        -1.0f, -1.0f, 0.0f, 0.0f,
        1.0f, -1.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 1.0f, 1.0f,

        1.0f, 1.0f, 1.0f, 1.0f,
        -1.0f, 1.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f,
    };
    renderer_init(renderer);
    renderer_bind(renderer);

    glBufferData(
        GL_ARRAY_BUFFER, sizeof(float) * 24,
        &vertices[0], GL_STATIC_DRAW
    );
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), NULL);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
        (void*)(2 * sizeof(float))
    );
}

void viewport_render_to_window(
    const Viewport*       viewport,
    const Renderer*       renderer,
    const Shader_Program* viewport_shader
) {
    if (viewport->has_blending) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    //TODO: make use of viewport screen_pos (apply matrix / offset in shader)

    glUseProgram(viewport_shader->id);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, viewport->frame_buffer_texture);
    glUniform1i(
        glGetUniformLocation(viewport_shader->id, "viewportTexture"), 0
    );
    glBindVertexArray(renderer->vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    if (viewport->has_blending)
        glDisable(GL_BLEND);
}

/* TEXTURE COORD QUADS ********************************************************/
typedef struct {
    vec2 min, max;
} Tex_Quad;

// Assuming row-major ordering (cells are filled left to right, then top to bottom)
// row, colum value range 0-1
Tex_Quad tex_quad_from_cell(
    const i32 row, const i32 column, const i32 rows, const i32 columns
) {
    SDL_assert(row < rows);
    SDL_assert(column < columns);

    const float cell_width  = 1.0f / (float)columns;
    const float cell_height = 1.0f / (float)rows;

    const vec2 min = (vec2){
        (float)column * cell_width,
        (float)row * cell_height
    };
    const vec2 max = (vec2){
        min.x + cell_width,
        min.y + cell_height
    };

    return (Tex_Quad){
        .min = min,
        .max = max,
    };
}

// Assuming row-major ordering (cells are filled left to right, then top to bottom)
// row, colum value range 0-1
Tex_Coords tex_coords_from_cell(
    const i32 row, const i32 column, const i32 rows, const i32 columns
) {
    const Tex_Quad quad = tex_quad_from_cell(
        row, column, rows, columns
    );

    return (Tex_Coords){
        .bottom_left = {quad.min.x, quad.min.y},
        .bottom_right = {quad.max.x, quad.min.y},
        .top_left = {quad.min.x, quad.max.y},
        .top_right = {quad.max.x, quad.max.y},
    };
}

// row, colum value range 0-1
void quad_row_and_column_from_cell_index(
    const i32 cell_index, const i32 columns, i32* row, i32* column
) {
    *row    = cell_index / columns;
    *column = cell_index % columns;
}

Tex_Quad tex_quad_from_cell_index(
    const i32 cell_index, const i32 rows, const i32 columns
) {
    i32 row, column;
    quad_row_and_column_from_cell_index(cell_index, columns, &row, &column);
    return tex_quad_from_cell(row, column, rows, columns);
}

Tex_Coords tex_coords_from_cell_index(
    const i32 cell_index, const i32 rows, const i32 columns
) {
    i32 row, column;
    quad_row_and_column_from_cell_index(cell_index, columns, &row, &column);
    return tex_coords_from_cell(row, column, rows, columns);
}

Tex_Coords tex_coords_mul_float(
    const Tex_Coords tex_coords,
    const float      factor
) {
    return (Tex_Coords){
        .bottom_left = vec2_mul_float(tex_coords.bottom_left, factor),
        .bottom_right = vec2_mul_float(tex_coords.bottom_right, factor),
        .top_left = vec2_mul_float(tex_coords.top_left, factor),
        .top_right = vec2_mul_float(tex_coords.top_right, factor),
    };
}

Tex_Coords tex_coords_mul_vec2(
    const Tex_Coords tex_coords,
    const vec2       factor
) {
    return (Tex_Coords){
        .bottom_left = vec2_mul_vec2(tex_coords.bottom_left, factor),
        .bottom_right = vec2_mul_vec2(tex_coords.bottom_right, factor),
        .top_left = vec2_mul_vec2(tex_coords.top_left, factor),
        .top_right = vec2_mul_vec2(tex_coords.top_right, factor),
    };
}

Tex_Coords tex_coords_add_vec2(
    const Tex_Coords tex_coords,
    const vec2       vec
) {
    return (Tex_Coords){
        .bottom_left = vec2_add_vec2(tex_coords.bottom_left, vec),
        .bottom_right = vec2_add_vec2(tex_coords.bottom_right, vec),
        .top_left = vec2_add_vec2(tex_coords.top_left, vec),
        .top_right = vec2_add_vec2(tex_coords.top_right, vec),
    };
}

Tex_Coords tex_coords_sub_vec2(
    const Tex_Coords tex_coords,
    const vec2       vec
) {
    return (Tex_Coords){
        .bottom_left = vec2_sub_vec2(tex_coords.bottom_left, vec),
        .bottom_right = vec2_sub_vec2(tex_coords.bottom_right, vec),
        .top_left = vec2_sub_vec2(tex_coords.top_left, vec),
        .top_right = vec2_sub_vec2(tex_coords.top_right, vec),
    };
}

Tex_Coords tex_coords_map_to_quad(
    const Tex_Coords tex_coords,
    const Tex_Quad*  quad
) {
    const vec2 scale = vec2_sub_vec2(quad->max, quad->min);
    return tex_coords_add_vec2(
        tex_coords_mul_vec2(tex_coords, scale), quad->min
    );
}

/* RECT RENDERING *************************************************************/
/*
    Instead of the previous 'single-buffer-partial-update-only-on-change'
    caching approach we're aiming for an immediate mode approach this time.
 */
typedef struct {
    vec2  pos;
    vec2  size;
    vec3  color;
    float sort_order;
    //the vertices of the rect will be aligned relative to the pivot
    //value range 0-1, where 0 = left/bottom, 1 = right/top, {0.5,0.5} = center
    vec2       pivot;
    i32        texture_id;
    Tex_Coords tex_coords;
} Rect;

typedef struct {
    Rect   rects[RECT_BUFFER_CAPACITY];
    size_t curr_len;
} Rect_Buffer;

void add_rect_to_buffer(Rect_Buffer* rect_buffer, const Rect rect) {
    SDL_assert(rect_buffer->curr_len + 1 < RECT_BUFFER_CAPACITY);
    rect_buffer->rects[rect_buffer->curr_len] = rect;
    rect_buffer->curr_len += 1;
}

void add_rect_to_buffer_quadmap(
    Rect_Buffer*    rect_buffer,
    Rect            rect,
    const Tex_Quad* quad
) {
    SDL_assert(quad != NULL);
    rect.tex_coords = tex_coords_map_to_quad(rect.tex_coords, quad);
    add_rect_to_buffer(rect_buffer, rect);
}

void reset_rect_buffer(Rect_Buffer* rect_buffer) {
    rect_buffer->curr_len = 0;
}

typedef struct {
    vec2  pos;
    vec3  color;
    vec2  tex_coord;
    float sort_order; //Value Range SORT_ORDER_MIN - SORT_ORDER_MAX
    i32   texture_id;
} Rect_Vertex;

typedef struct {
    Rect_Vertex vertices[RECT_VERTEX_BUFFER_CAPACITY];
    size_t      curr_len;
} Rect_Vertex_Buffer;

void build_rect_vertex_buffer(
    const Rect_Buffer*  rect_buffer,
    Rect_Vertex_Buffer* vertex_buffer
) {
    size_t vertex_index     = 0;
    vertex_buffer->curr_len = 0;
    if (rect_buffer->curr_len == 0) return;
    for (size_t rect_index = 0; rect_index < rect_buffer->curr_len; rect_index
         ++) {
        Rect        rect   = rect_buffer->rects[rect_index];
        Rect_Vertex vertex = (Rect_Vertex){
            .color = rect.color,
            .sort_order = rect.sort_order,
            .texture_id = rect.texture_id,
        };
        const vec2 pivot_offset = vec2_mul_vec2(rect.pivot, rect.size);

#define MAKE_CORNER_VERTEX(corner, sub_x, sub_y)                               \
        vertex.pos = vec2_sub_vec2(                                            \
            vec2_add_vec2(                                                     \
                rect.pos, (vec2){sub_x * rect.size.x, sub_y * rect.size.y}     \
            ), pivot_offset);                                                  \
        vertex.tex_coord = rect.tex_coords.corner;                             \
        const Rect_Vertex corner = vertex;
        MAKE_CORNER_VERTEX(bottom_left, 0, 0)
        MAKE_CORNER_VERTEX(bottom_right, 1, 0)
        MAKE_CORNER_VERTEX(top_left, 0, 1)
        MAKE_CORNER_VERTEX(top_right, 1, 1)
#undef MAKE_CORNER_VERTEX
        vertex_buffer->vertices[vertex_index + 0] = bottom_left;
        vertex_buffer->vertices[vertex_index + 1] = bottom_right;
        vertex_buffer->vertices[vertex_index + 2] = top_right;
        vertex_buffer->vertices[vertex_index + 3] = top_right;
        vertex_buffer->vertices[vertex_index + 4] = top_left;
        vertex_buffer->vertices[vertex_index + 5] = bottom_left;
        vertex_index += 6;
    }
    SDL_assert(vertex_index <= RECT_VERTEX_BUFFER_CAPACITY);
    vertex_buffer->curr_len = vertex_index;
}

//This assumes shader and texture(s) are already bound.
void draw_rects(
    const Rect_Vertex_Buffer* vertex_buffer,
    const Renderer*           renderer
) {
    if (vertex_buffer->curr_len == 0) return;
    renderer_bind(renderer);
    glBufferSubData(
        GL_ARRAY_BUFFER,
        0,
        (GLsizeiptr)(sizeof(Rect_Vertex) * vertex_buffer->curr_len),
        &vertex_buffer->vertices[0]
    );
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertex_buffer->curr_len);
}

/* TEXT RENDERING *************************************************************/
float get_font_height(const Font* font, const float scale) {
    return font->size * scale;
}

UI_Text_Dimension get_text_dimension(
    const char* text,
    const Font* font,
    const float scale
) {
    float width      = 0;
    float curr_width = 0;
    i32   num_lines  = 1;

    while (*text) {
        const i32 codepoint = (unsigned char)*text;
        if (codepoint == '\n') {
            width      = SDL_max(curr_width, width);
            curr_width = 0;
            num_lines++;
        } else {
            const i32 char_index = codepoint - FONT_UNICODE_START;
            if (char_index >= 0) {
                curr_width += font->char_data[char_index].xadvance;
            }
        }

        text++;
    }
    width = SDL_max(curr_width, width);
    width *= scale;
    const float font_height = get_font_height(font, scale);

    return (UI_Text_Dimension){
        .width = width,
        .height = font_height * (float)num_lines,
        .num_lines = num_lines,
        .font_height = font_height,
    };
}

float get_text_width(const char* text, const Font* font, const float scale) {
    float width      = 0;
    float curr_width = 0;
    while (*text) {
        const i32 codepoint = (unsigned char)*text;
        if (codepoint == '\n') {
            width      = SDL_max(curr_width, width);
            curr_width = 0;
        } else {
            const i32 char_index = codepoint - FONT_UNICODE_START;
            if (char_index >= 0) {
                curr_width += font->char_data[char_index].xadvance;
            }
        }
        text++;
    }
    width = SDL_max(curr_width, width);

    return width * scale;
}

void render_text(
    const String text,
    const Font*  font,
    const vec2   pos,
    const vec3   color,
    const float  scale,
    const float  sort_order,
    Rect_Buffer* rect_buffer
) {
    float      x              = 0, y = 0;
    size_t     glyph_iterator = 0;
    const vec2 adjusted_pos   = pos;
    //TODO: precalculate the text bounds and add vh centering functionality!

    SDL_assert(font->texture_type == FONT_TEXTURE_TYPE_ARRAY);

    Rect rect = (Rect){
        .color = color,
        .pivot = {0.0f, 0.0f},
        .sort_order = sort_order,
        .texture_id = font->texture_union.texture_id,
    };

    for (size_t i = 0; i < text.length; i++) {
        const char c = text.chars[i];
        switch (c) {
        case '\n':
            x = 0;
            y += font->size;
            continue;
        case '\t':
            x += font->size / 3.f; //FONT_TAB_SIZE;
            continue;
        case ' ':
            x += font->size / 6.f; //FONT_SPACE_SIZE;
            continue;
        default: break;
        }

        stbtt_aligned_quad quad = {0};
        stbtt_GetPackedQuad(
            font->char_data, FONT_TEXTURE_SIZE,
            FONT_TEXTURE_SIZE, (i32)c - FONT_UNICODE_START,
            &x, &y, &quad, true
        );

        quad.x0 *= scale;
        quad.x1 *= scale;
        quad.y0 *= scale;
        quad.y1 *= scale;

        quad.x0 = quad.x0 + adjusted_pos.x;
        quad.x1 = quad.x1 + adjusted_pos.x;
        quad.y0 = -quad.y0 + adjusted_pos.y;
        quad.y1 = -quad.y1 + adjusted_pos.y;

        rect.pos  = (vec2){quad.x0, quad.y1};
        rect.size = (vec2){
            SDL_fabsf(quad.x0 - quad.x1),
            SDL_fabsf(quad.y0 - quad.y1)
        };

        rect.tex_coords.bottom_left  = (vec2){quad.s0, 1.0f - quad.t1};
        rect.tex_coords.bottom_right = (vec2){quad.s1, 1.0f - quad.t1};
        rect.tex_coords.top_left     = (vec2){quad.s0, 1.0f - quad.t0};
        rect.tex_coords.top_right    = (vec2){quad.s1, 1.0f - quad.t0};

        SDL_assert(
            rect_buffer->curr_len + glyph_iterator <
            RECT_BUFFER_CAPACITY
        );
        rect_buffer->rects[rect_buffer->curr_len + glyph_iterator] = rect;
        glyph_iterator += 1;
    }

    rect_buffer->curr_len += glyph_iterator;
}

//NOTE: This is highly ineffcient - it duplicates the glyphs 4 times, introducing
//overdraw and vertex redundancy. A shader based solution would be better.
void render_text_outlined(
    const String text,
    const Font*  font,
    const vec2   pos,
    const vec3   color,
    const float  scale,
    float        sort_order,
    Rect_Buffer* rect_buffer,
    const float  outline_offset,
    const vec3   outline_color
) {
    render_text(text, font, pos, color, scale, sort_order, rect_buffer);
    sort_order -= 0.1f;

    render_text(
        text, font, (vec2){pos.x + outline_offset, pos.y},
        outline_color, scale, sort_order, rect_buffer
    );
    render_text(
        text, font, (vec2){pos.x - outline_offset, pos.y},
        outline_color, scale, sort_order, rect_buffer
    );
    render_text(
        text, font, (vec2){pos.x, pos.y + outline_offset},
        outline_color, scale, sort_order, rect_buffer
    );
    render_text(
        text, font, (vec2){pos.x, pos.y - outline_offset},
        outline_color, scale, sort_order, rect_buffer
    );
}

/* NINE SLICE *****************************************************************/
typedef struct {
    i32      texture_id;
    float    total_size;
    float    border_size;
    Tex_Quad quad;
    i32*     id_ptr;
} Nine_Slice;

void render_nine_slice(
    Rect_Buffer*      rect_buffer,
    const vec2        pos, // centered (equivalent to pivot = 0.5, 0.5)
    const vec2        size,
    const vec3        color,
    const float       sort_order,
    const Nine_Slice* nine_slice,
    bool              render_center
) {
    const vec2 half_size   = vec2_mul_float(size, .5f);
    const vec2 border_size = (vec2){
        nine_slice->border_size,
        nine_slice->border_size
    };
    const float bs_normalized = nine_slice->border_size / nine_slice->
        total_size;

    Rect rect = (Rect){
        .color = color,
        .size = border_size,
        .sort_order = sort_order,
        .texture_id = nine_slice->texture_id,
    };
    //TODO: assert for size < border_size

    //bottom left
    rect.pivot      = (vec2){0.f, 0.f};
    rect.pos        = vec2_sub_vec2(pos, half_size);
    rect.tex_coords = tex_coords_mul_float(
        default_tex_coords(),
        bs_normalized
    );
    add_rect_to_buffer_quadmap(rect_buffer, rect, &nine_slice->quad);

    //bottom right
    rect.pos        = vec2_add_vec2(rect.pos, (vec2){size.x, 0});
    rect.pivot      = (vec2){1.f, 0.f};
    rect.tex_coords = tex_coords_add_vec2(
        rect.tex_coords,
        (vec2){1.f - bs_normalized, 0.f}
    );
    add_rect_to_buffer_quadmap(rect_buffer, rect, &nine_slice->quad);

    //top right
    rect.pos        = vec2_add_vec2(rect.pos, (vec2){0, size.y});
    rect.pivot      = (vec2){1.f, 1.f};
    rect.tex_coords = tex_coords_add_vec2(
        rect.tex_coords,
        (vec2){0.f, 1.f - bs_normalized}
    );
    add_rect_to_buffer_quadmap(rect_buffer, rect, &nine_slice->quad);

    //top left
    rect.pos        = vec2_sub_vec2(rect.pos, (vec2){size.x, 0.f});
    rect.pivot      = (vec2){0.f, 1.f};
    rect.tex_coords = tex_coords_sub_vec2(
        rect.tex_coords,
        (vec2){1.f - bs_normalized, 0.f}
    );
    add_rect_to_buffer_quadmap(rect_buffer, rect, &nine_slice->quad);

    //bottom
    rect.pos   = vec2_sub_vec2(pos, (vec2){0, half_size.y});
    rect.pivot = (vec2){.5f, 0.f};
    rect.size  = (vec2){
        size.x - nine_slice->border_size * 2.f,
        nine_slice->border_size,
    };
    rect.tex_coords = (Tex_Coords){
        .bottom_left = (vec2){bs_normalized, 0.f},
        .bottom_right = (vec2){1.f - bs_normalized, 0.f},
        .top_left = (vec2){bs_normalized, bs_normalized},
        .top_right = (vec2){1.f - bs_normalized, bs_normalized},
    };
    add_rect_to_buffer_quadmap(rect_buffer, rect, &nine_slice->quad);

    //top
    rect.pos        = vec2_add_vec2(pos, (vec2){0, half_size.y});
    rect.pivot      = (vec2){.5f, 1.f};
    rect.tex_coords = tex_coords_add_vec2(
        rect.tex_coords, (vec2){
            0.f, 1.f - bs_normalized,
        }
    );
    add_rect_to_buffer_quadmap(rect_buffer, rect, &nine_slice->quad);

    //left
    rect.pos   = vec2_sub_vec2(pos, (vec2){half_size.x, 0.f});
    rect.pivot = (vec2){0.f, 0.5f};
    rect.size  = (vec2){
        nine_slice->border_size,
        size.y - nine_slice->border_size * 2.f,
    };
    rect.tex_coords = (Tex_Coords){
        .bottom_left = (vec2){0.f, bs_normalized},
        .bottom_right = (vec2){bs_normalized, bs_normalized},
        .top_left = (vec2){0.f, 1.f - bs_normalized},
        .top_right = (vec2){bs_normalized, 1.f - bs_normalized},
    };
    add_rect_to_buffer_quadmap(rect_buffer, rect, &nine_slice->quad);

    //right
    rect.pos        = vec2_add_vec2(pos, (vec2){half_size.x, 0.f});
    rect.pivot      = (vec2){1.f, 0.5f};
    rect.tex_coords = tex_coords_add_vec2(
        rect.tex_coords, (vec2){
            1.f - bs_normalized, 0.f
        }
    );
    add_rect_to_buffer_quadmap(rect_buffer, rect, &nine_slice->quad);

    //center
    if (render_center) {
        rect.pos        = pos;
        rect.size       = vec2_sub_float(size, nine_slice->border_size * 2.f);
        rect.pivot      = (vec2){.5f, .5f};
        rect.tex_coords = (Tex_Coords){
            .bottom_left = {bs_normalized, bs_normalized},
            .bottom_right = {1.f - bs_normalized, bs_normalized},
            .top_left = {bs_normalized, 1.f - bs_normalized},
            .top_right = {1.f - bs_normalized, 1.f - bs_normalized},
        };
        add_rect_to_buffer_quadmap(rect_buffer, rect, &nine_slice->quad);
    }
}

/* TEXTURE RESOURCE ***********************************************************/
/*For simplicity, we pack ALL things texture into a single gl texture array.
To handle the different types of textures like atlantes, fonts and normal images
we use the opaque concept of Texture_Resources.
*/

typedef enum {
    TEXTURE_TYPE_DEFAULT,
    TEXTURE_TYPE_FONT,
    TEXTURE_TYPE_ATLAS,
} Texture_Type;

typedef struct {
    const char*  file_name;
    Texture_Type type;

    union {
        //This is a bit un intuitive to have the font in here but works for now..
        Font          font;
        Texture_Atlas atlas;
    } data;

    i32* res_id;
} Texture_Resource;

Texture_Resource texture_resource_default(
    const char* file_name, i32* res_id
) {
    return (Texture_Resource){
        .file_name = file_name,
        .res_id = res_id,
        .type = TEXTURE_TYPE_DEFAULT,
    };
}

Texture_Resource texture_resource_font(
    const char* file_name, const float font_size, i32* res_id
) {
    return (Texture_Resource){
        .file_name = file_name,
        .res_id = res_id,
        .type = TEXTURE_TYPE_FONT,
        .data = {
            .font = {
                .size = font_size,
            }
        }
    };
}

Texture_Resource texture_resource_atlas(
    const char* file_name, const i32 rows, const i32 columns, i32* res_id
) {
    return (Texture_Resource){
        .file_name = file_name,
        .res_id = res_id,
        .type = TEXTURE_TYPE_ATLAS,
        .data = {
            .atlas = (Texture_Atlas){
                .rows = rows, .columns = columns,
            }
        }
    };
}

/* RESOURCES ******************************************************************/
typedef struct {
    Texture_Resource* textures;
    i32               num_textures;
    Nine_Slice*       nine_slices;
    i32               num_nine_slices;
} Resources;

void resources_cleanup(const Resources* resources) {
    for (int i = 0; i < resources->num_textures; i++) {
        switch (resources->textures[i].type) {
        case TEXTURE_TYPE_DEFAULT:
            break;
        case TEXTURE_TYPE_FONT:
            font_delete(&resources->textures[i].data.font);
            break;
        case TEXTURE_TYPE_ATLAS:
            break;
        }
    }
    CRLF_free(resources->textures);

    CRLF_free(resources->nine_slices);
}

/* MOUSE **********************************************************************/
typedef struct {
    float pos_x, pos_y;
} Mouse;

/* WINDOW *********************************************************************/
typedef struct {
    SDL_Window*   sdl;
    SDL_GLContext gl_context;
    i32           width, height;
    bool          fullscreen;
} Window;

/* UI ******* *****************************************************************/
/*
CRLF provides a simple immediate mode UI.

The ui layout approach is inspired by clay: https://github.com/nicbarker/clay

We use macros to define a UI tree
hierarchy structure:

//TODO: update to new macro style
UI_LAYOUT(
    UI_LAYOUT(
        UI_TEXT("NEW GAME");
        UI_IMAGE(TEXTURE_NEW_GAME_ICON);
    );
    UI_LAYOUT(
        UI_TEXT("LOAD GAME");
        UI_IMAGE(TEXTURE_LOAD_GAME_ICON);
    );
);

These instructions are equivalent to:

ui_element_start();
    ui_element_start();
        ui_element_start();
        ui_element_set_text("NEW GAME");
        ui_element_end();
        ui_element_start();
        ui_element_set_image(TEXTURE_NEW_GAME_ICON);
        ui_element_end();
    ui_element_end();
    ui_element_start();
        ui_element_start();
        ui_element_set_text("LOAD GAME");
        ui_element_end();
        ui_element_start();
        ui_element_set_image(TEXTURE_LOAD_GAME_ICON);
        ui_element_end();
    ui_element_end();
ui_element_end();


The instr result in the tree below:

Tree (Depth First Indices)

     0
     |
   -----
   |   |
   1   4
   |   |
  --- ---
  | | | |
  2 3 5 6

This isn't ideal as there are gaps in between the child indices, and as we want
to use first_child_id in combination with child_count for easy child access we
need to reorder the tree indices by breadth-first.
The desired tree looks like this:

Tree (Breadth-First Indices)

     0
     |
   -----
   |   |
   1   2
   |   |
  --- ---
  | | | |
  3 4 5 6

IMPROVE: Ideally we would already construct the tree with breadth-first indexing
instead of having to re-index.

These defines can be used for debugging the UI

UI_DEBUG_TEXT_ORIGIN
Renders the pivot / origin of the UI_Text

UI_DEBUG_TEXT_BOTTOM_LEFT
Rrenders the bottom left point of the UI_Text after alignment has been applied

UI_DEBUG_TEXT_RECT
Renders the full UI_Text rect in a single color
*/

u32 ui_element_get_id(const UI_Element* element) {
    if (element == NULL) return 0;
    switch (element->type) {
    default: SDL_assert(0);
        return 0;
    case UI_ELEMENT_TYPE_CONTAINER:
        return element->config.container.id;
    case UI_ELEMENT_TYPE_TEXT:
        return element->config.text.id;
    case UI_ELEMENT_TYPE_IMAGE:
        return element->config.image.id;
    }
}

u32 ui_element_get_id_by_index(const size_t index) {
    return ui_element_get_id(&ui_ctx->elements[index]);
}

void init_ui_context_ptr(UI_Context* ui_context) {
    SDL_assert(ui_context != NULL);
    *ui_context = (UI_Context){
        .viewport_size = (vec2){APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT},
        .elements = {0},
        .temp_queue = {0},
    };
}

void ui_context_init() {
    ui_ctx = CRLF_malloc(sizeof(UI_Context));
    init_ui_context_ptr(ui_ctx);
    ui_ctx->string_arena = arena_init(UI_STRING_ARENA_SIZE);
}

void ui_context_cleanup() {
    arena_cleanup(&ui_ctx->string_arena);
    CRLF_free(ui_ctx);
}

void ui_context_clear() {
    ui_ctx->elem_count       = 0;
    ui_ctx->tree_depth       = 0;
    ui_ctx->temp_depth       = 0;
    ui_ctx->temp_elem        = (UI_Element){0};
    ui_ctx->temp_queue_count = 0;
    ui_ctx->debug            = (UI_Context_Debug){0};
    arena_clear(&ui_ctx->string_arena);
}

void ui_tree_reindex_depth_first_to_breadth_first() {
    if (ui_ctx->elem_count == 0) return;

    //First we calculate the element counts for each depth
    size_t depth_elem_counts[UI_MAX_ELEMENTS] = {0};
    for (size_t i = 0; i < ui_ctx->elem_count; i++) {
        const UI_Element* element = &ui_ctx->elements[i];
        depth_elem_counts[element->depth]++;
    }

    //Then we assign new indices by summing up all depths before the current one
    size_t new_indices[UI_MAX_ELEMENTS]         = {0};
    size_t per_depth_iterators[UI_MAX_ELEMENTS] = {0};
    for (size_t i = 0; i < ui_ctx->elem_count; i++) {
        const UI_Element* element = &ui_ctx->elements[i];
        //OPTIMIZE: this summing part can be cached out of this loop
        size_t depth_sum = 0;
        for (size_t depth = 0; depth < element->depth; depth++) {
            depth_sum += depth_elem_counts[depth];
        }
        new_indices[i] = depth_sum + per_depth_iterators[element->depth];
        per_depth_iterators[element->depth]++;
    }

    //And finally swap the indices and the elements in the array
    UI_Element reindexed_elements[UI_MAX_ELEMENTS];
    for (size_t i = 0; i < ui_ctx->elem_count; i++) {
        UI_Element element = ui_ctx->elements[i];
        if (element.child_count > 0) {
            element.first_child_index = new_indices[element.first_child_index];
        }
        element.index                      = new_indices[i];
        reindexed_elements[new_indices[i]] = element;
    }

    for (size_t i = 0; i < ui_ctx->elem_count; i++) {
        ui_ctx->elements[i] = reindexed_elements[i];
    }
}

void ui_context_print(const size_t index, const i32 depth) {
    if (index >= ui_ctx->elem_count) return;
    for (int i = 0; i < depth; i++) printf("  ");
    const UI_Element* elem = &ui_ctx->elements[index];

    switch (elem->type) {
    case UI_ELEMENT_TYPE_CONTAINER:
        printf("LAYOUT:\n");
        for (size_t i = 0; i < elem->child_count; i++) {
            ui_context_print(elem->first_child_index + i, depth + 1);
        }
        break;
    case UI_ELEMENT_TYPE_TEXT:
        printf("TEXT: %s\n", elem->config.text.text.chars);
        break;
    case UI_ELEMENT_TYPE_IMAGE:
        printf("IMAGE: %d\n", elem->config.image.texture.id);
        break;
    default: SDL_assert(0);
    }
}

typedef struct {
    vec2 min;
    vec2 max;
} UI_Box;

bool point_inside_box(const UI_Box box, const vec2 point) {
    return point.x >= box.min.x && point.y >= box.min.y &&
        point.x <= box.max.x && point.y <= box.max.y;
}

//Adjusts the ui elements position and converts from square to screen coordinates
void ui_context_pos_size_pass(
    Resources*        resources,
    const size_t      index,
    const UI_Element* parent
) {
    if (index >= ui_ctx->elem_count) return;
    UI_Element* element = &ui_ctx->elements[index];

    const bool is_root     = parent == NULL;
    const vec2 parent_pos  = is_root ? VEC2(500, 500) : parent->_adjust_pos;
    const vec2 parent_size = is_root
                                 ? VEC2(1000, 1000)
                                 : parent->_adjusted_size;

    element->_adjust_pos = VEC2(
        parent_pos.x + float_lerp(-.5f, .5f, element->layout.anchor.x) *
        parent_size.x + element->layout.offset.x,

        parent_pos.y + float_lerp(-.5f, .5f, element->layout.anchor.y) *
        parent_size.y + element->layout.offset.y
    );
    element->_adjusted_size = element->layout.size;

    element->_screen_pos = VEC2(
        ui_ctx->square.origin.x + element->_adjust_pos.x * ui_ctx->square.
        scale_fac,
        ui_ctx->square.origin.y + element->_adjust_pos.y * ui_ctx->square.
        scale_fac
    );
    element->_screen_size = VEC2(
        element->_adjusted_size.x * ui_ctx->square.scale_fac,
        element->_adjusted_size.y * ui_ctx->square.scale_fac
    );

    switch (element->type) {
    default: SDL_assert(0);
        break;
    /* CONTAINER **************************************************************/
    case UI_ELEMENT_TYPE_CONTAINER:
        for (size_t i = 0; i < element->child_count; i++) {
            ui_context_pos_size_pass(
                resources, element->first_child_index + i, element
            );
        }
        break;

    /* TEXT *******************************************************************/
    //NOTE: The order of execution here is crucial - don't change!
    case UI_ELEMENT_TYPE_TEXT: {
        const Font* font = &resources->textures[element->config.text.font].data.
            font;
        element->config.text._screen_scale = font->size * element->config.text.
            scale * ui_ctx->square.scale_fac;
        const float text_scale = element->config.text._screen_scale;

        UI_Text_Dimension* txt = &element->config.text._dimension;
        *txt                   = get_text_dimension(
            element->config.text.text.chars, font, text_scale
        );
        element->_screen_size = VEC2(txt->width, txt->height);

#if defined(UI_DEBUG_TEXT_ORIGIN)
        render_nine_slice(
            rect_buffer, pos_converted,VEC2_ZERO, element->config.text.color,
            (float)element->depth, 2, &nine_slice_rounded,true
        );
#endif

        //TODO: add support for wrapping (inserting line breaks to fit the rect)
        switch (element->config.text.align.x) {
        case UI_ALIGNMENT_X_CENTER:
            element->_screen_pos.x -= txt->width * 0.5f;
            break;
        case UI_ALIGNMENT_X_LEFT:
            element->_screen_pos.x -= txt->width;
            break;
        case UI_ALIGNMENT_X_RIGHT:
            break;
        }

        switch (element->config.text.align.y) {
        case UI_ALIGNMENT_Y_CENTER:
            element->_screen_pos.y += txt->height * 0.5f - txt->font_height *
                0.75f;
            break;
        case UI_ALIGNMENT_Y_BOTTOM:
            element->_screen_pos.y += txt->height - txt->font_height * 1.25f;
            break;
        case UI_ALIGNMENT_Y_TOP:
            element->_screen_pos.y -= txt->font_height * 0.25f;
            break;
        }
        break;
    }
    /* IMAGE ******************************************************************/
    case UI_ELEMENT_TYPE_IMAGE:
        break;
    }
}

void ui_context_input_pass_element_hover_check(
    const UI_Box      box,
    const UI_Element* element
) {
    const bool is_hovering = point_inside_box(box, ui_ctx->cursor_pos);

    if (is_hovering) {
        if (!ui_ctx->input.is_hovering) {
            ui_ctx->input.is_hovering         = true;
            ui_ctx->input.hover_element_index = element->index;
            //TODO: touch edge case
        } else if (ui_ctx->elements[ui_ctx->input.hover_element_index].depth <
            element->depth) {
            ui_ctx->input.hover_element_index = element->index;
        }
    }
}

void ui_context_input_pass_recursion(
    const size_t index
) {
    if (index >= ui_ctx->elem_count) return;
    const UI_Element* element = &ui_ctx->elements[index];

    switch (element->type) {
    default: SDL_assert(0);
        break;
    /* CONTAINER **************************************************************/
    case UI_ELEMENT_TYPE_CONTAINER:
        if (element->config.container.blocks_cursor) {
            const UI_Box box = (UI_Box){
                .min = VEC2(
                    element->_screen_pos.x - element->_screen_size.x * 0.5f,
                    element->_screen_pos.y - element->_screen_size.y * 0.5f
                ),
                .max = VEC2(
                    element->_screen_pos.x + element->_screen_size.x * 0.5f,
                    element->_screen_pos.y + element->_screen_size.y * 0.5f
                )
            };
            ui_context_input_pass_element_hover_check(box, element);
        }

        for (size_t i = 0; i < element->child_count; i++) {
            ui_context_input_pass_recursion(element->first_child_index + i);
        }
        break;

    /* TEXT *******************************************************************/
    case UI_ELEMENT_TYPE_TEXT:
        //no interactions with text atm
        break;
    /* IMAGE ******************************************************************/
    case UI_ELEMENT_TYPE_IMAGE:
        if (element->config.image.blocks_cursor) {
            const vec2 min = VEC2(
                element->_screen_pos.x - element->_screen_size.x * element->
                config.image.pivot.x,
                element->_screen_pos.y - element->_screen_size.y * element->
                config.image.pivot.y
            );
            const UI_Box box = (UI_Box){
                .min = min,
                .max = VEC2(
                    min.x + element->_screen_size.x,
                    min.y + element->_screen_size.y
                )
            };
            ui_context_input_pass_element_hover_check(box, element);
        }
        break;
    }
}

//Identifies interactions (hovered element, scroll etc)
void ui_context_input_pass() {
    const bool was_start_touch = ui_ctx->input.is_start_touch;
    const u32  temp_down_id    = ui_ctx->input.down_id;
    ui_ctx->input              = (UI_Context_Input){0};
    ui_ctx->input.down_id      = temp_down_id;

    ui_context_input_pass_recursion(0);

    ui_ctx->input.hover_id = 0;
    if (ui_ctx->input.is_hovering) {
        ui_ctx->input.hover_id = ui_element_get_id_by_index(
            ui_ctx->input.hover_element_index
        );
        if (was_start_touch) {
            ui_ctx->input.down_id = ui_ctx->input.hover_id;
            SDL_Log("TOUCH ID: %u", ui_ctx->input.down_id);
        }
    }
}

//Adds the UI layout to the rect buffer
void ui_context_rect_render_pass(
    Rect_Buffer* rect_buffer,
    Resources*   resources,
    const size_t index,
    const float  sort_order_override
) {
    if (index >= ui_ctx->elem_count) return;
    UI_Element* element    = &ui_ctx->elements[index];
    const float sort_order = CRLF_SORT_ORDER_CLAMPED(
        sort_order_override != 0 ? sort_order_override + (float)element->depth :
        (float)element->depth
    );
    switch (element->type) {
    default: SDL_assert(0);
        break;
    /* CONTAINER **************************************************************/
    case UI_ELEMENT_TYPE_CONTAINER:
        if (!element->config.container.is_hidden) {
            render_nine_slice(
                rect_buffer,
                element->_screen_pos,
                element->_screen_size,
                element->config.container.bg_color,
                CRLF_SORT_ORDER_CLAMPED(
                    sort_order + element->config.container.sort_order_override
                ),
                &resources->nine_slices[element->config.container.
                                                 nine_slice_id],
                !element->config.container.is_slice_center_hidden
            );
        }
        for (size_t i = 0; i < element->child_count; i++) {
            ui_context_rect_render_pass(
                rect_buffer, resources, element->first_child_index + i,
                sort_order_override + element->config.container.
                                               sort_order_override
            );
        }
        break;

    /* TEXT *******************************************************************/
    case UI_ELEMENT_TYPE_TEXT: {
        const UI_Text_Dimension* txt = &element->config.text._dimension;
#if defined(UI_DEBUG_TEXT_BOTTOM_LEFT)
        render_nine_slice(
            rect_buffer, pos_converted,VEC2_ZERO, element->config.text.color,
            (float)element->depth, 2, &nine_slice_rounded,true
        );
#endif

#if defined(UI_DEBUG_TEXT_RECT)
        add_rect_to_buffer(
            rect_buffer,(Rect){
                .pos = pos_converted,
                .pivot = {.0f, .25f},//For text we need 0.25 y-pivot
                .size = size_converted,
                .color = element->config.text.color,
                .sort_order = sort_order-0.1f,
                .texture_id = 3,
                .tex_coords = default_rect_tex_coords()
        });
#endif

        if (element->config.text.bg_slice) {
            render_nine_slice(
                rect_buffer,
                vec2_add_vec2(
                    element->_screen_pos, VEC2(
                        txt->width *.5f,
                        - (txt->height * 0.5f - txt->font_height * 0.75f)
                    )
                ),
                element->_screen_size,
                element->config.text.color,
                (float)element->depth,
                &resources->nine_slices[element->config.text.bg_slice_id],
                true
            );
        }

        if (element->config.text.outline > 0.f) {
            render_text_outlined(
                element->config.text.text,
                &resources->textures[element->config.text.font].data.font,
                element->_screen_pos,
                element->config.text.color,
                element->config.text._screen_scale,
                sort_order + .1f, rect_buffer,
                element->config.text.outline * ui_ctx->square.scale_fac,
                element->config.text.outline_color
            );
        } else {
            render_text(
                element->config.text.text,
                &resources->textures[element->config.text.font].data.font,
                element->_screen_pos,
                element->config.text.color,
                element->config.text._screen_scale,
                sort_order + .1f, rect_buffer
            );
        }
        break;
    }
    /* IMAGE ******************************************************************/
    case UI_ELEMENT_TYPE_IMAGE: {
        const i32  tex_id     = element->config.image.texture.id;
        Tex_Coords tex_coords = {0};
        switch (element->config.image.texture.coords.mode) {
        default: SDL_assert(0);
            break;
        case UI_IMAGE_TEX_MODE_FULL:
            tex_coords = default_tex_coords();
            break;
        case UI_IMAGE_TEX_MODE_ATLAS_CELL_INDEX:
            tex_coords = tex_coords_from_cell_index(
                element->config.image.texture.coords.data.cell_index,
                resources->textures[tex_id].data.atlas.rows,
                resources->textures[tex_id].data.atlas.columns
            );
            break;
        case UI_IMAGE_TEX_MODE_ATLAS_ROW_COLUMN:
            tex_coords = tex_coords_from_cell(
                element->config.image.texture.coords.data.cell.row,
                element->config.image.texture.coords.data.cell.column,
                resources->textures[tex_id].data.atlas.rows,
                resources->textures[tex_id].data.atlas.columns
            );
            break;
        case UI_IMAGE_TEX_MODE_BY_VALUE:
            tex_coords = element->config.image.texture.coords.data.value;
            break;
        }

        add_rect_to_buffer(
            rect_buffer, (Rect){
                .pos = element->_screen_pos,
                .pivot = element->config.image.pivot,
                .size = element->_screen_size,
                .color = element->config.image.color,
                .sort_order = sort_order,
                .texture_id = element->config.image.texture.id,
                .tex_coords = tex_coords,
            }
        );
        break;
    }
    }
}

/* HOT RELOADING **************************************************************/
/*  For instant response while working on gameplay and ui this adds simple hot
	reloading capabilities to debug builds.

	It works like so:
		0. Compile game lib as game.dll
		1. Run the framework as a debug build
		2. During initialization game.dll is duplicated to temp_game.dll
		3. temp_game.dll is loaded
		4. During app window focus gain game.dll is checked:
		   whenever game.dll changes steps 2 and 3 are repeated

	This prevents permission denied errors and allows for instant responsens to
	changes in the gameplay code.

	Hot reloading is limited to changes in game.c though.
	Any changes to struct layouts or other memory specifics in game.h will
	cause undefined behaviour and issues!
*/

#if defined(__DEBUG__)
typedef bool (*Game_Init_Func)(
    CRLF_API*, Game*, UI_Context*, Game_Resource_IDs*


);

typedef void (*Game_Tick_Func)(Game*, float);
typedef void (*Game_Draw_Func)(Game*);
typedef void (*Game_Cleanup_Func)(Game*);
typedef void (*Game_UI_Input_Func)(Game*, u32);
typedef void (*Game_Keyboard_Input_Func)(Game*, SDL_KeyboardEvent);

typedef struct {
    Path                     lib_path;
    Path                     temp_lib_path;
    SDL_Time                 modify_time;
    SDL_SharedObject*        lib_obj;
    Game_Init_Func           game_init;
    Game_Tick_Func           game_tick;
    Game_Draw_Func           game_draw;
    Game_Cleanup_Func        game_cleanup;
    Game_UI_Input_Func       game_ui_input;
    Game_Keyboard_Input_Func game_keyboard_input;
} Hot_Reload;

//This copies the game lib to the temp lib in order to support hot reloading
//(To avoid permission denied errors when re-compiling the game lib we need a
//temp duplicate)
bool duplicate_game_lib(
    Hot_Reload* hot_reload
) {
    SDL_PathInfo path_info;
    if (!SDL_GetPathInfo(hot_reload->lib_path.str, &path_info) || path_info.type
        != SDL_PATHTYPE_FILE) {
        SDL_LogError(0, "Invalid game lib path: %s", SDL_GetError());
        return false;
    }

    if (!SDL_CopyFile(
            hot_reload->lib_path.str,
            hot_reload->temp_lib_path.str
        )
    ) {
        SDL_LogError(0, "Failed to copy temp game lib: %s", SDL_GetError());
        return false;
    }
    hot_reload->modify_time = path_info.modify_time;
    return true;
}

bool load_game_lib(
    CRLF_API*          api,
    Game*              game,
    Game_Resource_IDs* res_ids,
    Hot_Reload*        hot_reload
) {
    SDL_PathInfo path_info;
    if (!SDL_GetPathInfo(hot_reload->lib_path.str, &path_info) || path_info
        .type != SDL_PATHTYPE_FILE) {
        SDL_LogError(0, "Invalid game lib path: %s", SDL_GetError());
        return false;
    }

    if (path_info.modify_time <= hot_reload->modify_time) { return true; }

    if (hot_reload->game_cleanup != NULL) {
        hot_reload->game_cleanup(game);
    }
    SDL_UnloadObject(hot_reload->lib_obj);

    if (!duplicate_game_lib(hot_reload))
        return false;

    hot_reload->modify_time = path_info.modify_time;
    hot_reload->lib_obj     = SDL_LoadObject(hot_reload->temp_lib_path.str);
    if (hot_reload->lib_obj == NULL) {
        SDL_LogError(0, "load game lib object: %s", SDL_GetError());
        return false;
    }

#define LOAD_GAME_LIB_FUNC_PTR(func, func_t) {                                 \
    hot_reload->func = (func_t)SDL_LoadFunction(hot_reload->lib_obj, #func);   \
    if (hot_reload->func == NULL) {                                            \
        SDL_LogError(0, "load_game_lib: %s", SDL_GetError());                  \
        return false;                                                          \
    }}
    LOAD_GAME_LIB_FUNC_PTR(game_init, Game_Init_Func)
    LOAD_GAME_LIB_FUNC_PTR(game_tick, Game_Tick_Func)
    LOAD_GAME_LIB_FUNC_PTR(game_draw, Game_Draw_Func)
    LOAD_GAME_LIB_FUNC_PTR(game_cleanup, Game_Cleanup_Func)
    LOAD_GAME_LIB_FUNC_PTR(game_ui_input, Game_UI_Input_Func)
    LOAD_GAME_LIB_FUNC_PTR(game_keyboard_input, Game_Keyboard_Input_Func)

#undef LOAD_GAME_LIB_FUNC_PTR

    if (!hot_reload->game_init(api, game, ui_ctx, res_ids)) {
        log_error("Hot reload successful but game_init failed!");
        return false;
    }

    log_msg("Hot reload successful");
    return true;
}

void hot_reload_init_lib_paths(
    const char* base_path,
    Path*       lib_path,
    Path*       temp_lib_path
) {
    //NOTE: hard coded filename as this is only for development!
    const char* lib_name =
#if defined(SDL_PLATFORM_WIN32)
        "game.dll";
#elif defined(SDL_PLATFORM_APPLE)
	"game.dylib";
#elif defined (SDL_PLATFORM_LINUX)
	"game.so";
#else
	#error("unsupported platform")
#endif

    //Base Path
    {
        const size_t str_len = SDL_strlen(base_path) + SDL_strlen(lib_name);
        SDL_assert(str_len < MAX_PATH_LEN);
        SDL_snprintf(lib_path->str, MAX_PATH_LEN, "%s%s", base_path, lib_name);
        lib_path->length = SDL_strlen(lib_path->str);
    }

    //Temp Path
    {
        const char*  temp_str     = "temp_";
        const size_t temp_str_len = SDL_strlen(base_path) + SDL_strlen(temp_str)
            + SDL_strlen(lib_name);
        SDL_assert(temp_str_len < MAX_PATH_LEN);
        SDL_snprintf(
            temp_lib_path->str, MAX_PATH_LEN, "%s%s%s", base_path, temp_str,
            lib_name
        );
        temp_lib_path->length = SDL_strlen(temp_lib_path->str);
    }
}

bool hot_reload_init(
    Hot_Reload*        hot_reload,
    const char*        base_path,
    CRLF_API*          api,
    Game*              game,
    Game_Resource_IDs* res_ids
) {
    hot_reload_init_lib_paths(
        base_path,
        &hot_reload->lib_path,
        &hot_reload->temp_lib_path
    );

    if (!duplicate_game_lib(hot_reload))
        return false;

    //Hacky way to force loading
    hot_reload->modify_time = 0;

    if (!load_game_lib(api, game, res_ids, hot_reload))
        return false;

    return true;
}

void hot_reload_cleanup(const Hot_Reload* hot_reload) {
    if (hot_reload == NULL) return;

    if (hot_reload->lib_obj != NULL) {
        SDL_UnloadObject(hot_reload->lib_obj);
    }
    //TODO: consider deleting the temp dll file
}
#endif

/* PUBLIC API IMPLEMENTATION***************************************************/
void log_msg(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    SDL_LogMessageV(
        SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, fmt, args
    );
    va_end(args);
}

void log_warning(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    SDL_LogMessageV(
        SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_WARN, fmt, args
    );
    va_end(args);
}

void log_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    SDL_LogMessageV(
        SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR, fmt, args
    );
    va_end(args);
}

/* APP ************************************************************************/
typedef struct {
    Window window;
    Game   game;
    Mouse  mouse;
    u64    last_tick;
    Path   asset_path;

    Renderer           rect_renderer;
    Shader_Program     rect_shader;
    Rect_Buffer        rect_buffer;
    Rect_Vertex_Buffer rect_vertex_buffer;

#if defined(CRLF_USE_GAMEVIEWPORT)
    Viewport viewport_game;
#endif
    Viewport       viewport_ui;
    Renderer       viewport_renderer;
    Shader_Program viewport_shader;

    GL_Texture_Array  texture_array;
    bool              has_focus;
    Game_Resource_IDs res_id;
    Resources         resources;
    i32               num_tex_res;
    CRLF_API          api;
#if defined(__DEBUG__)
    Hot_Reload hot_reload;
#endif
} App;

//NOTE: switched from initializer func to ptr based approach as this exceeded
//the stack size (unvealed by compiling with MSVC, 1MB Stack size default)
void init_app_ptr(App* app) {
    *app = (App){
        .window = (Window){
            .width = APP_WINDOW_WIDTH,
            .height = APP_WINDOW_HEIGHT,
        },
#if defined(CRLF_USE_GAMEVIEWPORT)
        .viewport_game = default_viewport_game(),
#endif
        .viewport_ui = default_viewport_ui(),
        .has_focus = false,
        .api = (CRLF_API){
            .log_msg = log_msg,
            .log_error = log_error,
            .log_warning = log_warning,
        },
#if defined(__DEBUG__)
        .hot_reload = {0},
#endif
    };
}

static bool app_init(App* app) {
    const char* base_path = SDL_GetBasePath();
    asset_path_init(base_path, &app->asset_path);
    ui_context_init();

#if defined(__DEBUG__)
    if (!hot_reload_init(
        &app->hot_reload, base_path, &app->api, &app->game, &app->res_id
    ))
        return false;
#else
    if (!game_init(&app->api, &app->game, ui_ctx, &app->res_id)) return false;
#endif

    /* TEXTURE ****************************************************************/
    Texture_Resource texture_resources[] = {
        texture_resource_font("Born2bSportyV2.ttf", 16, &app->res_id.font1),
        texture_resource_atlas(
            "nine_slice.png", 4, 4, &app->res_id.nine_slice
        ),
        texture_resource_default("logo_crlf.png", &app->res_id.logo_crlf),
        texture_resource_atlas("tiles.png", 4, 4, &app->res_id.tiles),
        texture_resource_default(
            "placeholder.png", &app->res_id.tex_placeholder
        ),
        texture_resource_atlas("characters.png", 4, 4, &app->res_id.characters),
    };

    const i32 num_textures = sizeof(texture_resources) / sizeof(
        Texture_Resource);
    Raw_Texture** raw_textures = CRLF_malloc(
        num_textures * sizeof(Raw_Texture*)
    );

    for (int i = 0; i < num_textures; i++) {
        Texture_Resource* tex_res = &texture_resources[i];
        *tex_res->res_id          = i;
        switch (texture_resources[i].type) {
        case TEXTURE_TYPE_DEFAULT:
        case TEXTURE_TYPE_ATLAS:
            raw_textures[i] = raw_texture_from_file(
                temp_path_append(app->asset_path.str, tex_res->file_name)
            );
            break;
        case TEXTURE_TYPE_FONT:
            raw_textures[i] = font_load_for_array(
                temp_path_append(app->asset_path.str, tex_res->file_name),
                &tex_res->data.font, tex_res->data.font.size, i
            );
            break;
        }
    }

    app->texture_array = gl_texture_array_generate(
        &raw_textures[0], num_textures,
        128, 128, 4, default_texture_config_gammacorrect(), true
    );

    //NOTE: contents of raw_texture are freed via gl_texture_array_generate!
    CRLF_free(raw_textures);

    app->resources.num_textures = num_textures;
    const size_t tex_res_size   = num_textures * sizeof(Texture_Resource);
    app->resources.textures     = CRLF_malloc(tex_res_size);
    SDL_memcpy(app->resources.textures, &texture_resources[0], tex_res_size);

    /* NINE SLICE *************************************************************/
    const Nine_Slice nine_slices[] = {
        (Nine_Slice){
            .texture_id = app->res_id.nine_slice,
            .total_size = 32.f,
            .border_size = 10.f,
            .quad = tex_quad_from_cell(2, 0, 4, 4),
            .id_ptr = &app->res_id.nine_slice_rounded_black,
        },
        {
            .texture_id = app->res_id.nine_slice,
            .total_size = 32.f,
            .border_size = 10.f,
            .quad = tex_quad_from_cell(2, 1, 4, 4),
            .id_ptr = &app->res_id.nine_slice_square01_black,
        }
    };
    const i32 num_nine_slices = sizeof(nine_slices) / sizeof(Nine_Slice);
    for (int i = 0; i < num_nine_slices; i++) {
        *nine_slices[i].id_ptr = i;
    }

    app->resources.num_nine_slices = num_nine_slices;
    const size_t nine_slices_size  = num_nine_slices * sizeof(Nine_Slice);
    app->resources.nine_slices     = CRLF_malloc(nine_slices_size);
    SDL_memcpy(app->resources.nine_slices, &nine_slices[0], nine_slices_size);

    /* SHADER******************************************************************/
    app->rect_shader = compile_shader_program(
        rect_shader_vert, rect_shader_frag
    );
    app->viewport_shader = compile_shader_program(
        viewport_shader_vert, viewport_shader_frag
    );

    viewport_renderer_init(&app->viewport_renderer);

#if defined(CRLF_USE_GAMEVIEWPORT)
    viewport_generate(
        &app->viewport_game,
        (ivec2){app->window.width, app->window.height}
    );
#endif
    viewport_generate(
        &app->viewport_ui,
        (ivec2){app->window.width, app->window.height}
    );

    // Hello Triangle Example:
    // renderer_init(&app->test_renderer);
    // renderer_bind(&app->test_renderer);
    // const float vertices[] = {
    //     // positions            // colors
    //     0.0f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f,
    //     0.5f, -0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
    //     -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
    // };
    //
    // glBufferData(
    //     GL_ARRAY_BUFFER, sizeof(float) * 18,
    //     &vertices[0], GL_STATIC_DRAW
    // );
    // glEnableVertexAttribArray(0);
    // glVertexAttribPointer(0, 3, GL_FLOAT,GL_FALSE, 6 * sizeof(float), NULL);
    // glEnableVertexAttribArray(1);
    // glVertexAttribPointer(
    //     1, 3, GL_FLOAT,GL_FALSE, 6 * sizeof(float),
    //     (void*)(3 * sizeof(float))
    // );

    renderer_init(&app->rect_renderer);
    renderer_bind(&app->rect_renderer);
    const GLsizei rect_vertex_size = sizeof(Rect_Vertex);
    glBufferData(
        GL_ARRAY_BUFFER, rect_vertex_size * RECT_VERTEX_BUFFER_CAPACITY,
        NULL, GL_STATIC_DRAW
    );
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0, 2, GL_FLOAT, GL_FALSE, rect_vertex_size,
        (void*)offsetof(Rect_Vertex, pos)
    );
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1, 3, GL_FLOAT, GL_FALSE, rect_vertex_size,
        (void*)offsetof(Rect_Vertex, color)
    );
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        2, 2, GL_FLOAT, GL_FALSE, rect_vertex_size,
        (void*)offsetof(Rect_Vertex, tex_coord)
    );
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(
        3, 1, GL_FLOAT, GL_FALSE, rect_vertex_size,
        (void*)offsetof(Rect_Vertex, sort_order)
    );
    glEnableVertexAttribArray(4);
    glVertexAttribIPointer(
        4, 1, GL_INT, rect_vertex_size,
        (void*)offsetof(Rect_Vertex, texture_id)
    );

    // SDL_Log("Game: %d", test_game());

    return true;
}

static void app_tick(App* app) {
#if defined(__DEBUG__)
    app->hot_reload.game_tick(&app->game, DELTA_TIME);
#else
    game_tick(&app->game, DELTA_TIME);
#endif
    ui_ctx->time += DELTA_TIME;
}

static void app_draw(App* app) {
    /* GAME RENDER PASS *******************************************************/
#if defined(CRLF_USE_GAMEVIEWPORT)
    viewport_bind(&app->viewport_game);
    //E.g. Hello Triangle
    // glUseProgram(app->test_shader.id);
    // renderer_bind(&app->test_renderer);
    // glDrawArrays(GL_TRIANGLES, 0, 3);
#endif

    /* UI *********************************************************************/
    const float window_width = (float)app->window.width;
    const float window_height = (float)app->window.height;
    const float viewport_width = (float)app->viewport_ui.frame_buffer_size.x;
    const float viewport_height = (float)app->viewport_ui.frame_buffer_size.y;
    const int   framebuffer_size_min = SDL_min(
        app->viewport_ui.frame_buffer_size.x,
        app->viewport_ui.frame_buffer_size.y
    );
    const float square_size = (float)(framebuffer_size_min - (
        framebuffer_size_min % 2));

    const vec2 square_center = (vec2){
        viewport_width * 0.5f,
        viewport_height * 0.5f,
    };
    ui_ctx->square = (UI_Render_Square){
        .scale_fac = 0.001f * square_size, //div by 1000
        .size = square_size,
        .center = square_center,
        .origin = (vec2){
            square_center.x - square_size * .5f,
            square_center.y - square_size * .5f,
        }
    };
    ui_ctx->cursor_pos =
        (vec2){
            (app->mouse.pos_x / window_width) * viewport_width,
            viewport_height - (app->mouse.pos_y / window_height) *
            viewport_height,
        };

#if defined(CRLF_USE_SQUARE_SCISSOR)
    glEnable(GL_SCISSOR_TEST);
    const int   viewport_min    = SDL_min(viewport_width, viewport_height);
    const ivec2 viewport_center = (ivec2){
        (int)(viewport_width * 0.5f), (int)(viewport_height * 0.5f)
    };
    glScissor(
        viewport_center.x - viewport_min / 2,
        viewport_center.y - viewport_min / 2, viewport_min, viewport_min
    );
#endif

    viewport_bind(&app->viewport_ui);
    reset_rect_buffer(&app->rect_buffer);

    ui_ctx->viewport_size = ivec2_to_vec2(app->viewport_ui.frame_buffer_size);

    //TODO: split into game_draw and game_draw_ui (if we will have 3d after the jam)
#if defined(__DEBUG__)
    app->hot_reload.game_draw(&app->game);
#else
    game_draw(&app->game);
#endif

    ui_tree_reindex_depth_first_to_breadth_first();
    ui_context_pos_size_pass(&app->resources, 0, NULL);
    ui_context_input_pass();
    ui_context_rect_render_pass(&app->rect_buffer, &app->resources, 0, 0);
    ui_context_clear();

#if defined(CRLF_USE_SQUARE_SCISSOR)
    glDisable(GL_SCISSOR_TEST);
#endif

    /* UI BOILERPLATE **********************************************************/
    build_rect_vertex_buffer(&app->rect_buffer, &app->rect_vertex_buffer);
    glUseProgram(app->rect_shader.id);
    const mat4 ortho_mat = mat4_ortho(
        0.f, viewport_width, 0.f, viewport_height, CRLF_SORT_ORDER_MIN,
        CRLF_SORT_ORDER_MAX
    );
    const i32 projection_loc = glGetUniformLocation(
        app->rect_shader.id, "projection"
    );
    glUniformMatrix4fv(
        projection_loc, 1, GL_FALSE,
        (const GLfloat*)(&ortho_mat.matrix[0])
    );
    const i32 texture_array_loc = glGetUniformLocation(
        app->rect_shader.id, "textureArray"
    );
    glUniform1i(texture_array_loc, 0);
    gl_texture_array_bind(&app->texture_array, 0);
    const i32 loc_alpha_clip_threshold = glGetUniformLocation(
        app->rect_shader.id, "alphaClipThreshold"
    );
    glUniform1f(loc_alpha_clip_threshold, 0.5f);
    draw_rects(&app->rect_vertex_buffer, &app->rect_renderer);

    /* SCREEN *****************************************************************/
    viewport_unbind(app->window.width, app->window.height);
    glClear(GL_COLOR_BUFFER_BIT);

#if defined(CRLF_USE_GAMEVIEWPORT)
    viewport_render_to_window(
        &app->viewport_game, &app->viewport_renderer,
        &app->viewport_shader
    );
#endif

    viewport_render_to_window(
        &app->viewport_ui, &app->viewport_renderer,
        &app->viewport_shader
    );

    SDL_GL_SwapWindow(app->window.sdl);
}

static void app_cleanup(App* app) {
    resources_cleanup(&app->resources);
    texture_array_free(&app->texture_array);

    delete_shader_program(&app->rect_shader);
    delete_shader_program(&app->viewport_shader);

#if defined(__DEBUG__)
    hot_reload_cleanup(&app->hot_reload);
#endif

    ui_context_cleanup();
#if defined(CRLF_USE_GAMEVIEWPORT)
    viewport_cleanup(&app->viewport_game);
#endif
    viewport_cleanup(&app->viewport_ui);
    renderer_cleanup(&app->rect_renderer);
    renderer_cleanup(&app->viewport_renderer);
    SDL_GL_DestroyContext(app->window.gl_context);
    if (app->window.sdl)
        SDL_DestroyWindow(app->window.sdl);
    CRLF_free(app);
}

static void app_event_mouse_down(
    App*                       app,
    const SDL_MouseButtonEvent event
) {
    switch (event.button) {
    case SDL_BUTTON_LEFT:
        ui_ctx->input.down_id = ui_ctx->input.hover_id;
        return;

    case SDL_BUTTON_RIGHT:
    case SDL_BUTTON_MIDDLE:
    case SDL_BUTTON_X1:
    case SDL_BUTTON_X2:
    default: return;
    }
}

static void app_event_mouse_up(
    App*                       app,
    const SDL_MouseButtonEvent event
) {
    switch (event.button) {
    case SDL_BUTTON_LEFT:
        if (ui_ctx->input.down_id == 0) return;

        if (ui_ctx->input.down_id == ui_ctx->input.hover_id) {
            const u32 id = ui_ctx->input.down_id;
#if defined(__DEBUG__)
            app->hot_reload.game_ui_input(&app->game, id);
#else
            game_ui_input(&app->game, id);
#endif
        }

        ui_ctx->input.down_id = 0;
        return;

    case SDL_BUTTON_RIGHT:
    case SDL_BUTTON_MIDDLE:
    case SDL_BUTTON_X1:
    case SDL_BUTTON_X2:
    default: return;
    }
}

static void app_event_key_down(App* app, const SDL_KeyboardEvent event) {
#if defined(__DEBUG__)
    app->hot_reload.game_keyboard_input(&app->game, event);
#else
    game_keyboard_input(&app->game, event);
#endif

#if defined(__DEBUG__)
    switch (event.key) {
    case SDLK_SPACE:
        SDL_GetWindowFullscreenMode(app->window.sdl);
        SDL_SetWindowFullscreen(app->window.sdl, !app->window.fullscreen);
        return;
    case SDLK_MINUS:
        if (app->viewport_ui.frame_buffer_divisor == 1) return;
        app->viewport_ui.frame_buffer_divisor -= 1;
        viewport_generate(
            &app->viewport_ui, (ivec2){app->window.width, app->window.height}
        );
        return;
    case SDLK_EQUALS:
        app->viewport_ui.frame_buffer_divisor += 1;
        viewport_generate(
            &app->viewport_ui, (ivec2){app->window.width, app->window.height}
        );
        return;
    default:
#endif


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
#if defined(__MSVC_CRT_LEAK_DETECTION__)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    if (!SDL_SetAppMetadata(
            APP_TITLE,
            APP_VERSION,
            APP_IDENTIFIER
        )
    ) {
        SDL_LogError(0, "Failed to set SDL AppMetadata: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO)) {
        SDL_LogError(0, "Failed to initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    App* app = CRLF_malloc(sizeof(App));
    if (app == NULL) {
        SDL_LogError(0, "Failed to allocate APP memory: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    init_app_ptr(app);
    *appstate = app;

#if !defined(SDL_PLATFORM_EMSCRIPTEN)
    //Part of the UI simplification fallback approach @ day 4 (end-jam)
    //make 1:1 aspect ratio fit the display for desktop
    const SDL_DisplayID    display = SDL_GetPrimaryDisplay();
    const SDL_DisplayMode* display_mode = SDL_GetDesktopDisplayMode(display);
    const i32              smaller_display_size = (i32)((float)SDL_min(
        display_mode->w, display_mode->h
    ) * 0.9f);
    app->window.width  = smaller_display_size;
    app->window.height = smaller_display_size;
#endif

    SDL_Window* window = SDL_CreateWindow(
        APP_TITLE,
        app->window.width, app->window.height,
        /*SDL_WINDOW_MAXIMIZED |*/ SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        SDL_LogError(0, "Failed to create Window: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

#if !defined(SDL_PLATFORM_EMSCRIPTEN)
    app->window.sdl = window;
    if (!SDL_SetWindowSurfaceVSync(window, 1)) {
        SDL_LogWarn(0, "Failed to set window vsync! %s", SDL_GetError());
    }
#endif

#if defined (SDL_PLATFORM_EMSCRIPTEN)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_PROFILE_MASK,
        SDL_GL_CONTEXT_PROFILE_CORE
    );
#endif
    //TODO: test if OPENGL_FORWARD_COMPAT is required for mac with sdl

    app->window.gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, app->window.gl_context);

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        SDL_LogError(0, "Failed to load OpenGL via Glad: %s", SDL_GetError());
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
    App* app = appstate;
    if (!app->has_focus) return SDL_APP_CONTINUE;

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
        app->window.width  = event->window.data1;
        app->window.height = event->window.data2;
#if defined(CRLF_USE_GAMEVIEWPORT)
        viewport_generate(
            &app->viewport_game,
            (ivec2){app->window.width, app->window.height}
        );
#endif
        viewport_generate(
            &app->viewport_ui,
            (ivec2){app->window.width, app->window.height}
        );
        break;

    case SDL_EVENT_KEY_DOWN:
        app_event_key_down(app, event->key);
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        app_event_mouse_down(app, event->button);
        break;

    case SDL_EVENT_MOUSE_BUTTON_UP:
        app_event_mouse_up(app, event->button);
        break;

    case SDL_EVENT_FINGER_DOWN:
        ui_ctx->input.is_start_touch = true;
        break;

    case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
        app->window.fullscreen = true;
        break;

    case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
        app->window.fullscreen = false;
        break;

    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        app->has_focus = true;
        app->last_tick = SDL_GetTicks();

#if defined(__DEBUG__)
        if (!load_game_lib(
            &app->api, &app->game, &app->res_id, &app->hot_reload
        )) {
            SDL_LogError(0, "Hot Reload failed!");
            return SDL_APP_FAILURE;
        }
#endif
        break;

    case SDL_EVENT_WINDOW_FOCUS_LOST:
        app->has_focus = false;
        break;

    default: break;
    }

#if !defined(SDL_PLATFORM_EMSCRIPTEN)
    if (app->game.quit_requested)
        return SDL_APP_SUCCESS;
#endif

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
    SDL_Quit();
#if defined(__MSVC_CRT_LEAK_DETECTION__)
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
    _CrtDumpMemoryLeaks();
#endif
}
