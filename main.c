#include <glad/glad.h>

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_render.h>

/*
 * C ROGUELIKE FRAMEWORK *******************************************************
 * This is a lightweight C framework in preparation for 7drl 2025.
 * It also serves as a learning project to get started learning SDL3.
 *
 * For simplicity, we'll use GL 3.3 core for all desktop platform and GL ES 3.0
 * for web as the feature set of these two gl versions is quite close to each
 * other.
 *
 * Ttarget platforms:
 *   -Windows
 *   -Linux
 *   -macOS
 *   -Emscripten
 *
 * Third Party Libs:
 *  -sdl
 *  -miniaudio
 *  -stb_image
 *  -stb_truetype
 *  -FastNoise Lite
 *
 *******************************************************************************
 */

/* GLOBALS ********************************************************************/
const char* APP_TITLE       = "ROGUELIKE GAME";
const char* APP_VERSION     = "0.1.0";
const char* APP_IDENTIFIER  = "com.otone.roguelike";

#define SDL_WINDOW_WIDTH  800
#define SDL_WINDOW_HEIGHT 600

#define TARGET_FPS 30
const Uint64 TICK_RATE_IN_MS = (Uint64)(1.0f / (float)TARGET_FPS * 1000.0f);
const float DELTA_TIME = (float)TICK_RATE_IN_MS / 1000.0f;

const char* GLSL_SOURCE_HEADER =
    #if defined(SDL_PLATFORM_EMSCRIPTEN)
    "#version 300 es\n"
    #else
    "#version 330 core\n"
    #endif
    "precision mediump float;\n";
const char* test_shader_vert  =
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

typedef struct
{
    float pos_x;
    float pos_y;
} Player;

typedef struct
{
    Player player;
} Game;

typedef struct
{
    float pos_x, pos_y;
} Mouse;

typedef struct
{
    SDL_Window* sdl;
    SDL_GLContext gl_context;
    int width, height;
    bool fullscreen;
} Window;

/* RENDERER *******************************************************************/
typedef struct
{
    GLuint vao, vbo;
} Renderer;

void renderer_init(Renderer* renderer)
{
    glGenBuffers(1, &renderer->vbo);
    SDL_assert(renderer->vbo != 0);

    glGenVertexArrays(1, &renderer->vao);
    SDL_assert(renderer->vao != 0);
}

void renderer_cleanup(const Renderer* renderer)
{
    SDL_assert(renderer->vbo != 0);
    glDeleteBuffers(1, &renderer->vbo);

    SDL_assert(renderer->vao != 0);
    glDeleteVertexArrays(1, &renderer->vao);
}

/* SHADER *********************************************************************/
typedef enum
{
    SHADER_TYPE_VERTEX,
    SHADER_TYPE_FRAGMENT,
} Shader_Type;

typedef struct
{
    GLuint id;
    Shader_Type type;
} Shader;

typedef struct
{
    GLuint id;
} Shader_Program;

bool check_shader_compilation(const GLuint shader)
{
    int success;
    int shaderType;

    glGetShaderiv(shader, GL_SHADER_TYPE, &shaderType);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success)
    {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        SDL_Log("shader compilation failed: %s", infoLog);
        return false;
    }

    return true;
}

Shader compile_shader(const char* source, const Shader_Type type)
{
    Shader shader = {0};
    GLenum gl_shader_type = 0;
    switch (type)
    {
    case SHADER_TYPE_VERTEX:
        gl_shader_type = GL_VERTEX_SHADER;
        break;
    case SHADER_TYPE_FRAGMENT:
        gl_shader_type = GL_FRAGMENT_SHADER;
        break;
    }
    shader.id = glCreateShader(gl_shader_type);
    const char* strs[] = { GLSL_SOURCE_HEADER, source };
    glShaderSource(shader.id, 2, strs, NULL);
    glCompileShader(shader.id);
    SDL_assert(check_shader_compilation(shader.id));
    return shader;
}

void delete_shader(Shader* shader)
{
    glDeleteShader(shader->id);
    shader->id = 0;
}

Shader_Program link_shaders(const Shader* vertex, const Shader* fragment)
{
    Shader_Program program = {0};
    program.id = glCreateProgram();
    glAttachShader(program.id, vertex->id);
    glAttachShader(program.id, fragment->id);
    glLinkProgram(program.id);
    return program;
}

/* APP ************************************************************************/
typedef struct
{
    Window window;
    Game game;
    Mouse mouse;
    Uint64 last_tick;
    Renderer test_renderer;
    Shader_Program test_shader;
} App;

static App default_app()
{
    return (App){
        .window = (Window){
            .width = SDL_WINDOW_WIDTH,
            .height = SDL_WINDOW_HEIGHT,
        },
    };
}

static void app_init(App* app)
{
    Shader vert = compile_shader(test_shader_vert, SHADER_TYPE_VERTEX);
    Shader frag = compile_shader(test_shader_frag, SHADER_TYPE_FRAGMENT);
    app->test_shader = link_shaders(&vert, &frag);
    delete_shader(&vert);
    delete_shader(&frag);

    renderer_init(&app->test_renderer);
    glBindBuffer(GL_ARRAY_BUFFER, app->test_renderer.vbo);
    glBindVertexArray(app->test_renderer.vao);

    const float vertices[] = {
        // positions            // colors
        0.0f,  0.5f, 0.5f,     1.0f, 0.0f, 0.0f,
        0.5f, -0.5f, 0.5f,     0.0f, 1.0f, 0.0f,
       -0.5f, -0.5f, 0.5f,     0.0f, 0.0f, 1.0f,
    };

    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 18, &vertices[0], GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3,  GL_FLOAT, GL_FALSE, 6 * sizeof(float), NULL);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3,  GL_FLOAT, GL_FALSE, 6 * sizeof(float),
        (void*)(3 * sizeof(float)));
}

static void app_tick(App* app)
{
    app->game.player.pos_x = app->mouse.pos_x;
    app->game.player.pos_y = app->mouse.pos_y;
}

static void app_draw(const App* app)
{
    glClearColor(0.25f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glViewport(0, 0, app->window.width, app->window.height);

    glUseProgram(app->test_shader.id);
    glBindBuffer(GL_ARRAY_BUFFER, app->test_renderer.vbo);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    SDL_GL_SwapWindow(app->window.sdl);
}

static void app_cleanup(App* app)
{
    renderer_cleanup(&app->test_renderer);
    SDL_GL_DestroyContext(app->window.gl_context);

    if (app->window.sdl)
        SDL_DestroyWindow(app->window.sdl);

    SDL_free(app);
}

static void app_event_key_down(App* app, const SDL_KeyboardEvent event)
{
    switch (event.key)
    {
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
SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv)
{
    if (!SDL_SetAppMetadata(
            APP_TITLE,
            APP_VERSION,
            APP_IDENTIFIER)
    )
    {
        SDL_Log("Failed to set SDL AppMetadata: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO))
    {
        SDL_Log("Failed to initialize SDL: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    App* app = SDL_malloc(sizeof(App));
    if (app == NULL)
    {
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

    if (!window)
    {
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
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif

    app->window.gl_context = SDL_GL_CreateContext(window);

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
    {
        SDL_Log("Failed to load OpenGL via Glad: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    app_init(app);

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
SDL_AppResult SDL_AppIterate(void* appstate)
{
    App* app = (App*)appstate;
    const Uint64 now = SDL_GetTicks();

    SDL_GetMouseState(&app->mouse.pos_x, &app->mouse.pos_y);

    while ((now - app->last_tick) >= TICK_RATE_IN_MS)
    {
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
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event)
{
    App* app = (App*)appstate;
    switch (event->type)
    {
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
void SDL_AppQuit(void* appstate, SDL_AppResult result)
{
    if (appstate == NULL) return;

    App* app = (App*)appstate;

    app_cleanup(app);
}