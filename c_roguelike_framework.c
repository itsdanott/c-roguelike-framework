/*
* C ROGUELIKE FRAMEWORK ********************************************************

	@@@		@@@		@		@@@
	@		@ @		@		@
	@		@@@		@		@@@
	@@@		@  @	@@@		@

	Lightweight C99 framework in preparation for 7drl 2025.

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
		- replace stb function defs with SDL ones to avoid C runtime library
		- UI Layout persistence accross aspect ratios and resolutions
		- Textbox alignment(pivots)
		- Add miniaudio
		- Add FNL
		- Tilemap rendering
		- Game simulation

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

#if defined(SDL_PLATFORM_EMSCRIPTEN)
#define SDL_WINDOW_WIDTH  480
#define SDL_WINDOW_HEIGHT 240
#else
#define SDL_WINDOW_WIDTH  640
#define SDL_WINDOW_HEIGHT 480
#endif

const u64 TICK_RATE_IN_MS = 16;
const float DELTA_TIME = (float)TICK_RATE_IN_MS / SDL_MS_PER_SECOND;

//FONT_SIZE-Values for 128px FONT_TEXTURE_SIZE
//16 - Born2bSportyV2.ttf
//18 - Tiny.ttf
#define FONT_TEXTURE_SIZE 128
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

const char* FONT_PATH = "font-tiny/tiny.ttf";

//For desktop this is 32 - but we'll go with the lowest common denominator: web
#define MAX_TEXTURE_SLOTS 16

#define RECT_BUFFER_CAPACITY 2048
#define RECT_VERTEX_BUFFER_CAPACITY (RECT_BUFFER_CAPACITY*6)
#define RECT_MAX_SORT_ORDER 128.f
#define RECT_MIN_SORT_ORDER (-RECT_MAX_SORT_ORDER)

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

vec2 vec2_mul_float(const vec2 vec, const float fac) {
	return (vec2){
		vec.x * fac,
		vec.y * fac,
	};
}

vec3 vec3_mul_float(const vec3 vec, const float fac) {
	return (vec3){
		vec.x * fac,
		vec.y * fac,
		vec.z * fac,
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

vec2 vec2_sub_float(const vec2 a, const float b) {
	return (vec2){
		a.x - b,
		a.y - b,
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

float float_lerp(const float a, const float b, const float t) {
	return (1 - t) * a + t * b;
}

vec2 vec2_lerp(const vec2 a, const vec2 b, const float t) {
	return (vec2){
		float_lerp(a.x, b.x, t),
		float_lerp(a.y, b.y, t),
	};
}

vec3 vec3_lerp(const vec3 a, const vec3 b, const float t) {
	return (vec3){
		float_lerp(a.x, b.x, t),
		float_lerp(a.y, b.y, t),
		float_lerp(a.z, b.z, t),
	};
}

/* COLORS *********************************************************************/
#define COLOR_RED			(vec3){1.00f,0.00f,0.00f}
#define COLOR_GREEN			(vec3){0.00f,1.00f,0.00f}
#define COLOR_BLUE			(vec3){0.00f,0.00f,1.00f}
#define COLOR_YELLOW		(vec3){1.00f,1.00f,0.00f}
#define COLOR_MAGENTA		(vec3){1.00f,0.00f,1.00f}
#define COLOR_BLACK			(vec3){0.00f,0.00f,0.00f}
#define COLOR_GRAY			(vec3){0.50f,0.50f,0.50f}
#define COLOR_GRAY_BRIGHT	(vec3){0.75f,0.75f,0.75f}
#define COLOR_GRAY_DARK		(vec3){0.25f,0.25f,0.25f}
#define COLOR_WHITE			(vec3){1.00f,1.00f,1.00f}

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

void asset_path_init(const char* base_path, Path* asset_path) {
	const char* asset_str = "assets";
	const size_t str_len = SDL_strlen(base_path) + SDL_strlen(asset_str) + 4;
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

const Texture_Config TEXTURE_CONFIG_DEFAULT = (Texture_Config){
	.filter = false,
	.repeat = true,
	.gamma_correction = false,
};

const Texture_Config TEXTURE_CONFIG_DEFAULT_GAMMACORRECT = (Texture_Config){
	.filter = false,
	.repeat = true,
	.gamma_correction = true,
};

Raw_Texture* raw_texture_rgba_from_single_channel(
	const u8* single_channel_data,
	const i32 width, const i32 height
) {
	Raw_Texture* raw_texture = SDL_malloc(sizeof(Raw_Texture));
	*raw_texture = (Raw_Texture){
		.width = width,
		.height = height,
		.channels = 4,
		.data = SDL_malloc(sizeof(u8) * width * height * 4),
		.source = TEXTURE_RAW_SOURCE_DYNAMIC,
	};

	for (i32 i = 0; i < width * height; i++) {
		const u8 gray_scale = single_channel_data[i];
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
		SDL_LogError(0,"Invalid Path: %s", SDL_GetError());
		return NULL;
	}

	Raw_Texture* texture = SDL_malloc(sizeof(Raw_Texture));
	*texture = (Raw_Texture){
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
		SDL_free(texture->data);
		break;
	case TEXTURE_RAW_SOURCE_STB_IMAGE:
		stbi_image_free(texture->data);
	}
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
			target,GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR
		);
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
	GL_Texture_Format format = gl_texture_get_format(
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
	u32 id;
	i32 num_textures;
	i32 width, height, channels;
	Texture_Config config;
} GL_Texture_Array;

void gl_texture_array_bind(
	const GL_Texture_Array* texture_array,
	const u32 slot
) {
	SDL_assert(texture_array != NULL);
	SDL_assert(texture_array->id > 0);
	SDL_assert(slot < MAX_TEXTURE_SLOTS);
	glActiveTexture(GL_TEXTURE0 + slot);
	glBindTexture(GL_TEXTURE_2D_ARRAY, texture_array->id);
}

GL_Texture_Array gl_texture_array_generate(
	Raw_Texture** textures,
	const i32 num_textures,
	const i32 width,
	const i32 height,
	const i32 channels,
	const Texture_Config config,
	const bool free_raw_textures
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
	stbtt_fontinfo info;
	stbtt_pack_context pack_context;
	stbtt_packedchar char_data[FONT_UNICODE_RANGE];
	Font_Texture_Type texture_type;

	i32 size;
	union {
		GL_Texture texture;
		i32 texture_id;
	} texture_union;
} Font;

//Loads the ttf, packs the glyphs it to an atlas and generates a rgba texture
//If we will switch to texture arrays later on we'll need to split the logic.
Raw_Texture* font_load_raw_texture(
	const char* file_path,
	Font* font,
	const i32 size
) {
	SDL_assert(font != NULL);
	SDL_PathInfo path_info;
	if (!SDL_GetPathInfo(file_path, &path_info) || path_info.type !=
	SDL_PATHTYPE_FILE) {
		SDL_LogError(0,"Invalid Path: %s", SDL_GetError());
		return NULL;
	}

	SDL_IOStream* io_stream = SDL_IOFromFile(file_path, "r");
	size_t data_size = 0;
	u8* file_data = SDL_LoadFile_IO(io_stream, &data_size, false);
	SDL_CloseIO(io_stream);
	*font = (Font){0};
	if (!stbtt_InitFont(&font->info, file_data, 0)) {
		SDL_LogError(0,"Failed init font!");
		return NULL;
	}

	u8 pixels[FONT_TEXTURE_SIZE * FONT_TEXTURE_SIZE];
	if (!stbtt_PackBegin(
		&font->pack_context, &pixels[0],
		FONT_TEXTURE_SIZE, FONT_TEXTURE_SIZE,
		0, 1, NULL
	)) {
		SDL_LogError(0,"Failed to font pack begin!");
		return NULL;
	}

	font->size = size;
	if (!stbtt_PackFontRange(
		&font->pack_context, file_data, 0, size,
		FONT_UNICODE_START, FONT_UNICODE_RANGE,
		font->char_data
	)) {
		SDL_LogError(0,"Failed to pack font range!");
		return NULL;
	}

	stbtt_PackEnd(&font->pack_context);
	SDL_free(file_data);

	Raw_Texture* raw_texture = raw_texture_rgba_from_single_channel(
		pixels, FONT_TEXTURE_SIZE, FONT_TEXTURE_SIZE
	);

	return raw_texture;
}

bool font_load_single(const char* file_path, Font* font, const i32 size) {
	Raw_Texture* raw_texture = font_load_raw_texture(file_path, font, size);
	if (raw_texture == NULL)
		return false;

	font->texture_type = FONT_TEXTURE_TYPE_SINGLE;
	font->texture_union.texture = gl_texture_from_raw_texture(
		raw_texture, TEXTURE_CONFIG_DEFAULT
	);
	raw_texture_free(raw_texture);

	return true;
}

Raw_Texture* font_load_for_array(
	const char* file_path, Font* font, const i32 size,  const i32 texture_id
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
		SDL_LogError(0,"shader compilation failed: %s", infoLog);
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
	bool is_initialized;
	vec2 screen_pos;
	ivec2 display_size;
	ivec2 frame_buffer_size;
	float aspect_ratio;
	u32 frame_buffer;
	u32 frame_buffer_texture;
	u32 render_buffer;

	//to be initially configured
	vec4 clear_color;
	i32 frame_buffer_divisor;
	bool has_blending;
	bool has_depth_buffer;
	bool floating_point_precision;
} Viewport;

Viewport default_viewport_game() {
	return (Viewport){
		.screen_pos = (vec2){256, 0},
		.clear_color = (vec4){0.f, 0.f, 0.f, 1.f},
		.frame_buffer_divisor = 4,
		.has_blending = false,
		.has_depth_buffer = true,
		.floating_point_precision = false,
	};
}

Viewport default_viewport_ui() {
	return (Viewport){
		.clear_color = (vec4){0},
		.frame_buffer_divisor = 2,
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
	Viewport* viewport,
	const ivec2 display_size
) {
	viewport_cleanup(viewport);
	viewport->display_size = display_size;
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
	const Viewport* viewport,
	const Renderer* renderer,
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

/* RECT TEX COORDS ************************************************************/
//To avoid C's array weirdness we'll use a struct for tex_coords instead of an
//array(as we would in e.g. Odin)
typedef struct {
	vec2 bottom_left, bottom_right, top_left, top_right;
} Tex_Coords;

typedef struct {
	vec2 min, max;
} Tex_Coords_Quad;

const Tex_Coords RECT_DEFAULT_TEX_COORDS = (Tex_Coords){
	.bottom_left = {0.0f, 0.0f},
	.bottom_right = {1.0f, 0.0f},
	.top_left = {0.0f, 1.0f},
	.top_right = {1.0f, 1.0f},
};

Tex_Coords tex_coords_mul_float(
	const Tex_Coords tex_coords,
	const float factor
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
	const vec2 factor
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
	const vec2 vec
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
	const vec2 vec
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
	const Tex_Coords_Quad* quad
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
	vec2 pos;
	vec2 size;
	vec3 color;
	float sort_order;
	//the vertices of the rect will be aligned relative to the pivot
	//value range 0-1, where 0 = left/bottom, 1 = right/top, {0.5,0.5} = center
	vec2 pivot;
	i32 texture_id;
	Tex_Coords tex_coords;
} Rect;

typedef struct {
	Rect rects[RECT_BUFFER_CAPACITY];
	size_t curr_len;
} Rect_Buffer;

void add_rect_to_buffer(Rect_Buffer* rect_buffer, const Rect rect) {
	SDL_assert(rect_buffer->curr_len + 1 < RECT_BUFFER_CAPACITY);
	rect_buffer->rects[rect_buffer->curr_len] = rect;
	rect_buffer->curr_len += 1;
}

void add_rect_to_buffer_quadmap(
	Rect_Buffer* rect_buffer,
	Rect rect,
	const Tex_Coords_Quad* quad
) {
	SDL_assert(quad != NULL);
	rect.tex_coords = tex_coords_map_to_quad(rect.tex_coords, quad);
	add_rect_to_buffer(rect_buffer, rect);
}

void reset_rect_buffer(Rect_Buffer* rect_buffer) {
	rect_buffer->curr_len = 0;
}

typedef struct {
	vec2 pos;
	vec3 color;
	vec2 tex_coord;
	//Min-Val = 0
	float sort_order;
	i32 texture_id;
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
	for (size_t rect_index = 0; rect_index < rect_buffer->curr_len; rect_index++) {
		Rect rect = rect_buffer->rects[rect_index];
		Rect_Vertex vertex = (Rect_Vertex){
			.color = rect.color,
			.sort_order = rect.sort_order,
			.texture_id = rect.texture_id,
		};
		const vec2 pivot_offset = vec2_mul_vec2(rect.pivot, rect.size);

		//Bottom Left
		vertex.pos = vec2_sub_vec2(rect.pos, pivot_offset);
		vertex.tex_coord = rect.tex_coords.bottom_left;
		const Rect_Vertex bottom_left = vertex;

		//Bottom Right
		vertex.pos = vec2_sub_vec2(
			vec2_add_vec2(rect.pos, (vec2){rect.size.x, 0}), pivot_offset
		);
		vertex.tex_coord = rect.tex_coords.bottom_right;
		const Rect_Vertex bottom_right = vertex;

		//Top Left
		vertex.pos = vec2_sub_vec2(
			vec2_add_vec2(rect.pos, (vec2){0, rect.size.y}), pivot_offset
		);
		vertex.tex_coord = rect.tex_coords.top_left;
		const Rect_Vertex top_left = vertex;

		//Top Right
		vertex.pos = vec2_sub_vec2(
			vec2_add_vec2(rect.pos, rect.size), pivot_offset
		);
		vertex.tex_coord = rect.tex_coords.top_right;
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
		&vertex_buffer->vertices[0]
	);
	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertex_buffer->curr_len);
}

/* TEXT RENDERING *************************************************************/
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
	size_t glyph_iterator = 0;
	const size_t text_len = SDL_strlen(text);
	const vec2 adjusted_pos = pos;
	//TODO: precalculate the text bounds and add vh centering functionality!

	SDL_assert(font->texture_type == FONT_TEXTURE_TYPE_ARRAY);

	Rect rect = (Rect){
		.color = color,
		.pivot = {0.0f, 0.0f},
		.sort_order = sort_order,
		.texture_id = font->texture_union.texture_id,
	};

	for (size_t i = 0; i < text_len; i++) {
		const char c = text[i];
		switch (c) {
		case '\n':
			x = 0;
			y += font->size;
			continue;
		case '\t':
			x += FONT_TAB_SIZE;
			continue;
		case ' ':
			x += FONT_SPACE_SIZE;
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

		rect.pos = (vec2){quad.x0, quad.y1};
		rect.size = (vec2){
			SDL_fabsf(quad.x0 - quad.x1),
			SDL_fabsf(quad.y0 - quad.y1)
		};

		rect.tex_coords.bottom_left = (vec2){quad.s0, 1.0f - quad.t1};
		rect.tex_coords.bottom_right = (vec2){quad.s1, 1.0f - quad.t1};
		rect.tex_coords.top_left = (vec2){quad.s0, 1.0f - quad.t0};
		rect.tex_coords.top_right = (vec2){quad.s1, 1.0f - quad.t0};

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
	const char* text,
	const Font* font,
	const vec2 pos,
	const vec3 color,
	const float scale,
	float sort_order,
	Rect_Buffer* rect_buffer,
	const float outline_offset,
	const vec3 outline_color
) {
	render_text(text, font, pos, color, scale, sort_order, rect_buffer);
	sort_order -= 1.0f;

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
	float total_size;
	float border_size;
	Tex_Coords_Quad quad;
} Nine_Slice;

void render_nine_slice(
	Rect_Buffer* rect_buffer,
	const vec2 pos,
	const vec2 size,
	const vec3 color,
	const float sort_order,
	const i32 texture_id,
	const Nine_Slice* nine_slice,
	bool render_center
) {
	const vec2 half_size = vec2_mul_float(size, .5f);
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
		.texture_id = texture_id,
	};
	//TODO: assert for size < border_size

	//bottom left
	rect.pivot = (vec2){0.f, 0.f};
	rect.pos = vec2_sub_vec2(pos, half_size);
	rect.tex_coords = tex_coords_mul_float(
		RECT_DEFAULT_TEX_COORDS,
		bs_normalized
	);
	add_rect_to_buffer_quadmap(rect_buffer, rect, &nine_slice->quad);

	//bottom right
	rect.pos = vec2_add_vec2(rect.pos, (vec2){size.x, 0});
	rect.pivot = (vec2){1.f, 0.f};
	rect.tex_coords = tex_coords_add_vec2(
		rect.tex_coords,
		(vec2){1.f - bs_normalized, 0.f}
	);
	add_rect_to_buffer_quadmap(rect_buffer, rect, &nine_slice->quad);

	//top right
	rect.pos = vec2_add_vec2(rect.pos, (vec2){0, size.y});
	rect.pivot = (vec2){1.f, 1.f};
	rect.tex_coords = tex_coords_add_vec2(
		rect.tex_coords,
		(vec2){0.f, 1.f - bs_normalized}
	);
	add_rect_to_buffer_quadmap(rect_buffer, rect, &nine_slice->quad);

	//top left
	rect.pos = vec2_sub_vec2(rect.pos, (vec2){size.x, 0.f});
	rect.pivot = (vec2){0.f, 1.f};
	rect.tex_coords = tex_coords_sub_vec2(
		rect.tex_coords,
		(vec2){1.f - bs_normalized, 0.f}
	);
	add_rect_to_buffer_quadmap(rect_buffer, rect, &nine_slice->quad);

	//bottom
	rect.pos = vec2_sub_vec2(pos, (vec2){0, half_size.y});
	rect.pivot = (vec2){.5f, 0.f};
	rect.size = (vec2){
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
	rect.pos = vec2_add_vec2(pos, (vec2){0, half_size.y});
	rect.pivot = (vec2){.5f, 1.f};
	rect.tex_coords = tex_coords_add_vec2(
		rect.tex_coords, (vec2){
			0.f, 1.f - bs_normalized,
		}
	);
	add_rect_to_buffer_quadmap(rect_buffer, rect, &nine_slice->quad);

	//left
	rect.pos = vec2_sub_vec2(pos, (vec2){half_size.x, 0.f});
	rect.pivot = (vec2){0.f, 0.5f};
	rect.size = (vec2){
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
	rect.pos = vec2_add_vec2(pos, (vec2){half_size.x, 0.f});
	rect.pivot = (vec2){1.f, 0.5f};
	rect.tex_coords = tex_coords_add_vec2(
		rect.tex_coords, (vec2){
			1.f - bs_normalized, 0.f
		}
	);
	add_rect_to_buffer_quadmap(rect_buffer, rect, &nine_slice->quad);

	//center
	if (render_center) {
		rect.pos = pos;
		rect.size = vec2_sub_float(size, nine_slice->border_size * 2.f);
		rect.pivot = (vec2){.5f, .5f};
		rect.tex_coords = (Tex_Coords){
			.bottom_left = {bs_normalized, bs_normalized},
			.bottom_right = {1.f - bs_normalized, bs_normalized},
			.top_left = {bs_normalized, 1.f - bs_normalized},
			.top_right = {1.f - bs_normalized, 1.f - bs_normalized},
		};
		add_rect_to_buffer_quadmap(rect_buffer, rect, &nine_slice->quad);
	}
}

/* UI LAYOUT ******************************************************************/
typedef struct {
	vec2 pos;
	vec2 size;
	vec2 pivot;
} Box_Layout;

/* MOUSE **********************************************************************/
typedef struct {
	float pos_x, pos_y;
} Mouse;

/* WINDOW *********************************************************************/
typedef struct {
	SDL_Window* sdl;
	SDL_GLContext gl_context;
	i32 width, height;
	bool fullscreen;
} Window;

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
*/

#if defined(__DEBUG__)
typedef bool (*Game_Init_Func)(CRLF_API*);
typedef void (*Game_Tick_Func)(Game*);
typedef void (*Game_Draw_Func)(Game*);
typedef void (*Game_Cleanup_Func)(Game*);

typedef struct {
	Path lib_path;
	Path temp_lib_path;
	SDL_Time modify_time;
	SDL_SharedObject* lib_obj;
	Game_Init_Func game_init_func;
	Game_Tick_Func game_tick_func;
	Game_Draw_Func game_draw_func;
	Game_Cleanup_Func game_cleanup_func;
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
		hot_reload->temp_lib_path.str)
	) {
		SDL_LogError(0, "Failed to copy temp game lib: %s", SDL_GetError());
		return false;
	}
	hot_reload->modify_time = path_info.modify_time;
	return true;
}

bool load_game_lib(
	CRLF_API* api,
	Game* game,
	Hot_Reload* hot_reload
) {
	SDL_PathInfo path_info;
	if (!SDL_GetPathInfo(hot_reload->lib_path.str, &path_info) || path_info
		.type != SDL_PATHTYPE_FILE) {
		SDL_LogError(0, "Invalid game lib path: %s", SDL_GetError());
		return false;
	}

	if (path_info.modify_time <= hot_reload->modify_time) { return true; }

	if (hot_reload->game_cleanup_func != NULL) {
		hot_reload->game_cleanup_func(game);
	}
	SDL_UnloadObject(hot_reload->lib_obj);

	if (!duplicate_game_lib(hot_reload))
		return false;

	hot_reload->modify_time = path_info.modify_time;
	hot_reload->lib_obj = SDL_LoadObject(hot_reload->temp_lib_path.str);
	if (hot_reload->lib_obj == NULL) {
		SDL_LogError(0, "load game lib object: %s", SDL_GetError());
		return false;
	}

	hot_reload->game_init_func = (Game_Init_Func)SDL_LoadFunction(
		hot_reload->lib_obj, "game_init");
	if (hot_reload->game_init_func == NULL) {
		SDL_LogError(0, "load_game_lib: %s", SDL_GetError());
		return false;
	}

	hot_reload->game_tick_func = (Game_Tick_Func)SDL_LoadFunction(
		hot_reload->lib_obj, "game_tick");
	if (hot_reload->game_tick_func == NULL) {
		SDL_LogError(0, "load_game_lib: %s", SDL_GetError());
		return false;
	}

	hot_reload->game_draw_func = (Game_Draw_Func)SDL_LoadFunction(
		hot_reload->lib_obj, "game_draw");
	if (hot_reload->game_draw_func == NULL) {
		SDL_LogError(0, "load_game_lib: %s", SDL_GetError());
		return false;
	}

	hot_reload->game_cleanup_func = (Game_Cleanup_Func)SDL_LoadFunction(
		hot_reload->lib_obj, "game_cleanup");
	if (hot_reload->game_cleanup_func == NULL) {
		SDL_LogError(0, "load_game_lib: %s", SDL_GetError());
		return false;
	}

	if (!hot_reload->game_init_func(api)) {
		CRLF_LogError("Hot reload successful but game_init failed!");
		return false;
	}

	CRLF_Log("Hot reload successful");
	return true;
}

void hot_reload_init_lib_paths(
	const char* base_path,
	Path* lib_path,
	Path* temp_lib_path
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
		const char* temp_str = "temp_";
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
	Hot_Reload* hot_reload,
	const char* base_path,
	CRLF_API* api,
	Game* game
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

	if (!load_game_lib(api, game, hot_reload))
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
void CRLF_Log(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	SDL_LogMessageV(
		SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, fmt, args
	);
	va_end(args);
}

void CRLF_LogWarning(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	SDL_LogMessageV(
		SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_WARN, fmt, args
	);
	va_end(args);
}

void CRLF_LogError(const char* fmt, ...) {
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
	Game game;
	Mouse mouse;
	u64 last_tick;
	Renderer test_renderer;
	Shader_Program test_shader;
	Path asset_path;
	Font font1, font2;
	Renderer rect_renderer;
	Shader_Program rect_shader;
	Rect_Buffer rect_buffer;
	Rect_Vertex_Buffer rect_vertex_buffer;
	Viewport viewport_game;
	Viewport viewport_ui;
	Renderer viewport_renderer;
	Shader_Program viewport_shader;
	GL_Texture_Array texture_array;
	bool has_focus;
	CRLF_API api;
#if defined(__DEBUG__)
	Hot_Reload hot_reload;
#endif
} App;

static App default_app() {
	return (App){
		.window = (Window){
			.width = SDL_WINDOW_WIDTH,
			.height = SDL_WINDOW_HEIGHT,
		},
		.viewport_game = default_viewport_game(),
		.viewport_ui = default_viewport_ui(),
		.has_focus = false,
		.api = (CRLF_API) {
			.CRLF_Log = CRLF_Log,
			.CRLF_LogError = CRLF_LogError,
			.CRLF_LogWarning = CRLF_LogWarning,
		},
#if defined(__DEBUG__)
		.hot_reload = {0},
#endif
	};
}

static bool app_init(App* app) {
	const char* base_path = SDL_GetBasePath();
	asset_path_init(base_path, &app->asset_path);
#if defined(__DEBUG__)
	if (!hot_reload_init(&app->hot_reload, base_path, &app->api, &app->game))
		return false;
#else
	if (!game_init(&app->api)) return false;
#endif

	Raw_Texture* raw_textures[] = {
		font_load_for_array(
			temp_path_append(app->asset_path.str, "Born2bSportyV2.ttf"),
			&app->font1, 16, 0
		),
		font_load_for_array(
			temp_path_append(app->asset_path.str, "tiny.ttf"), &app->font2, 18, 1
		),
		raw_texture_from_file(
			temp_path_append(app->asset_path.str, "nine_slice_test.png")
		),
	};
	app->texture_array = gl_texture_array_generate(
		&raw_textures[0], 3,
		128, 128, 4, TEXTURE_CONFIG_DEFAULT, true
	);

	app->test_shader = compile_shader_program(
		test_shader_vert, test_shader_frag
	);
	app->rect_shader = compile_shader_program(
		rect_shader_vert, rect_shader_frag
	);
	app->viewport_shader = compile_shader_program(
		viewport_shader_vert, viewport_shader_frag
	);

	viewport_renderer_init(&app->viewport_renderer);
	viewport_generate(
		&app->viewport_game,
		(ivec2){app->window.width, app->window.height}
	);
	viewport_generate(
		&app->viewport_ui,
		(ivec2){app->window.width, app->window.height}
	);

	renderer_init(&app->test_renderer);
	renderer_bind(&app->test_renderer);
	const float vertices[] = {
		// positions            // colors
		0.0f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f,
		0.5f, -0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
		-0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
	};

	glBufferData(
		GL_ARRAY_BUFFER, sizeof(float) * 18,
		&vertices[0], GL_STATIC_DRAW
	);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT,GL_FALSE, 6 * sizeof(float), NULL);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(
		1, 3, GL_FLOAT,GL_FALSE, 6 * sizeof(float),
		(void*)(3 * sizeof(float))
	);

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
	app->hot_reload.game_tick_func(&app->game);
#else
	game_tick(&app->game);
#endif
}

static void app_draw(App* app) {
	/* GAME *******************************************************************/
	viewport_bind(&app->viewport_game);

	//Hello Triangle
	glUseProgram(app->test_shader.id);
	renderer_bind(&app->test_renderer);
	glDrawArrays(GL_TRIANGLES, 0, 3);

	/* UI *********************************************************************/
	viewport_bind(&app->viewport_ui);
	reset_rect_buffer(&app->rect_buffer);

	//Rects
	const float viewport_width = (float)app->viewport_ui.frame_buffer_size.x;
	const float viewport_height = (float)app->viewport_ui.frame_buffer_size.y;
	const float window_width = (float)app->window.width;
	const float window_height = (float)app->window.height;

	const float font_scale = 1.f;
	//this approach would allow for consistent font size among all viewport
	//divisors - but it will come with other caveats - we still need to
	//figure out a way to define the layout - maybe checkout clay for
	//that purpose? (https://github.com/nicbarker/clay)
	// const float font_scale = ((float)FONT_SIZE / (float)app->viewport_ui.frame_buffer_divisor) * 0.25f;

	const Font* font = &app->font1;
	render_text(
		"Hello 7DRL 2025!",
		font,
		(vec2){
			(app->mouse.pos_x / window_width) * viewport_width,
			viewport_height - (app->mouse.pos_y / window_height) *
			viewport_height,
		},
		COLOR_WHITE,
		font_scale, 1.0f, &app->rect_buffer
	);

	const Nine_Slice nine_slice = {
		.total_size = 32.f,
		.border_size = 10.f,
		.quad = {
			.min = (vec2){0.f, .75f},
			.max = (vec2){.25f, 1.f},
		}
	};
	const Nine_Slice nine_slice_square = {
		.total_size = 32.f,
		.border_size = 10.f,
		.quad = {
			.min = (vec2){.25f, .75f},
			.max = (vec2){.5f, 1.f},
		}
	};

	render_nine_slice(
		&app->rect_buffer,
		(vec2){
			viewport_width* .5f,
			viewport_height *.5f,
		},
		(vec2){viewport_width, viewport_height},
		COLOR_RED,
		0.f,
		2,
		&nine_slice_square,
		false
	);
	render_nine_slice(
		&app->rect_buffer,
		(vec2){
			viewport_width * .75f + viewport_width * .25f * 0.5f,
			viewport_height * .5f
		},
		(vec2){viewport_width * .25f, viewport_height},
		COLOR_RED,
		0.f,
		2,
		&nine_slice,
		true
	);
	float h =font->size+nine_slice.border_size;
	float px =viewport_width * .75f + viewport_width * .25f * 0.5f;
	render_nine_slice(
		&app->rect_buffer,
		(vec2){
			px,
			viewport_height - h,
		},
		(vec2){viewport_width * .2f, h},
		COLOR_WHITE,
		0.5f,
		2,
		&nine_slice,
		true
		);
	render_text(
		"7DRL 2025",
		font,
		(vec2){
			px-32.f,
			viewport_height - h-nine_slice.border_size*.5f,
		},
		COLOR_YELLOW,
		font_scale, 1.0f, &app->rect_buffer
	);

	build_rect_vertex_buffer(&app->rect_buffer, &app->rect_vertex_buffer);
	glUseProgram(app->rect_shader.id);
	const mat4 ortho_mat = mat4_ortho(
		0.f, viewport_width, 0.f, viewport_height, RECT_MIN_SORT_ORDER,
		RECT_MAX_SORT_ORDER
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

	viewport_render_to_window(
		&app->viewport_game, &app->viewport_renderer,
		&app->viewport_shader
	);

	viewport_render_to_window(
		&app->viewport_ui, &app->viewport_renderer,
		&app->viewport_shader
	);

	SDL_GL_SwapWindow(app->window.sdl);
}

static void app_cleanup(App* app) {
#if defined(__DEBUG__)
	hot_reload_cleanup(&app->hot_reload);
#endif
	delete_shader_program(&app->test_shader);
	delete_shader_program(&app->test_shader);
	delete_shader_program(&app->rect_shader);
	delete_shader_program(&app->viewport_shader);
	font_delete(&app->font1);
	font_delete(&app->font2);
	texture_array_free(&app->texture_array);
	viewport_cleanup(&app->viewport_game);
	viewport_cleanup(&app->viewport_ui);
	renderer_cleanup(&app->test_renderer);
	renderer_cleanup(&app->rect_renderer);
	renderer_cleanup(&app->viewport_renderer);
	SDL_GL_DestroyContext(app->window.gl_context);
	if (app->window.sdl)
		SDL_DestroyWindow(app->window.sdl);
	SDL_free(app);
}

static void app_event_mouse_down(
	const App* app,
	const SDL_MouseButtonEvent event
) {
	switch (event.button) {
	case SDL_BUTTON_LEFT:
	case SDL_BUTTON_RIGHT:
	case SDL_BUTTON_MIDDLE:
	case SDL_BUTTON_X1:
	case SDL_BUTTON_X2:
	default: return;
	}
}

static void app_event_key_down(App* app, const SDL_KeyboardEvent event) {
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
			APP_IDENTIFIER
		)
	) {
		SDL_LogError(0,"Failed to set SDL AppMetadata: %s\n", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO)) {
		SDL_LogError(0,"Failed to initialize SDL: %s\n", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	App* app = SDL_malloc(sizeof(App));
	if (app == NULL) {
		SDL_LogError(0,"Failed to allocate APP memory: %s\n", SDL_GetError());
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
		SDL_LogError(0,"Failed to create Window: %s\n", SDL_GetError());
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
	SDL_GL_SetAttribute(
		SDL_GL_CONTEXT_PROFILE_MASK,
		SDL_GL_CONTEXT_PROFILE_CORE
	);
#endif
	//TODO: test if OPENGL_FORWARD_COMPAT is required for mac with sdl

	app->window.gl_context = SDL_GL_CreateContext(window);

	if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
		SDL_LogError(0,"Failed to load OpenGL via Glad: %s\n", SDL_GetError());
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
		app->window.width = event->window.data1;
		app->window.height = event->window.data2;
		viewport_generate(
			&app->viewport_game,
			(ivec2){app->window.width, app->window.height}
		);
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
		if(!load_game_lib(&app->api, &app->game, &app->hot_reload)){
			SDL_LogError(0, "Hot Reload failed!");
			return SDL_APP_FAILURE;
		}
#endif
		break;

	case SDL_EVENT_WINDOW_FOCUS_LOST:
		app->has_focus = false;
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