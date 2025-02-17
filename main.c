#include "tiny_ttf.h"
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>

/*
 * C ROGUELIKE FRAMEWORK *******************************************************
 * This is a lightweight C framework in preparation for 7drl 2025.
 * It also serves as a learning project to get started learning SDL3.
 *
 * Ideally the only dependency is SDL.
 * So instead of the typical route via stb, miniaudio and so on we want to do
 * everything with the SDL libs (SDL, SDL_ttf, SDL_mixer and SDL_image).
 *
 * Planned target platforms:
 *   -Windows
 *   -Linux
 *   -macOS
 *   -Emscripten
 *
 *******************************************************************************
 */

const char* APP_TITLE       = "ROGUELIKE GAME";
const char* APP_VERSION     = "0.1.0";
const char* APP_IDENTIFIER  = "com.otone.roguelike";

#define SDL_WINDOW_WIDTH  800
#define SDL_WINDOW_HEIGHT 600

#define TARGET_FPS 30
const Uint64 TICK_RATE_IN_MS = (Uint64)(1.0f / (float)TARGET_FPS * 1000.0f);
const float DELTA_TIME = (float)TICK_RATE_IN_MS / 1000.0f;

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
    int width, height;
    bool fullscreen;
} Window;

typedef struct
{
    Window window;
    Game game;
    Mouse mouse;
    SDL_Renderer* renderer;
    TTF_Font* font;
    TTF_TextEngine* text_engine;

    Uint64 last_tick;
} App;

static App default_app()
{
    return (App){
        .window = (Window){
            .width = SDL_WINDOW_WIDTH,
            .height = SDL_WINDOW_HEIGHT,
        },
        .font = NULL,
    };
}

static void app_tick(App* app)
{
    app->game.player.pos_x = app->mouse.pos_x;
    app->game.player.pos_y = app->mouse.pos_y;
}

static void app_draw(const App* app)
{
    SDL_SetRenderDrawColor(app->renderer, 0, 255, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(app->renderer);


    SDL_SetRenderDrawColor(app->renderer, 128, 0, 0,SDL_ALPHA_OPAQUE);
    SDL_RenderLine(
        app->renderer,
        0, 0,
        (float)app->window.width, (float)app->window.height
    );

    SDL_RenderDebugText(
        app->renderer,
        app->game.player.pos_x,
        app->game.player.pos_y,
        "HELLO ROGUE!"
    );

    SDL_FRect rect = (SDL_FRect){
        .w = (float)app->window.width * 0.25f,
        .h = (float)app->window.height * 0.25f,
    };
    rect.x = (float)app->window.width * 0.5f - rect.w * 0.5f;
    rect.y = (float)app->window.height * 0.5f - rect.h * 0.5f;

    SDL_RenderFillRect(app->renderer, &rect);


    //TEXT Test
    TTF_Text* txt = TTF_CreateText(app->text_engine, app->font,
                                   "Hello Rogue!", 0);
    TTF_SetTextColor(txt, 128, 128, 0, SDL_ALPHA_OPAQUE);
    TTF_DrawRendererText(txt, app->game.player.pos_x, rect.y);
    TTF_DestroyText(txt);

    SDL_RenderPresent(app->renderer);
}

static void process_event_key_down(App* app, const SDL_KeyboardEvent event)
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

/*******************************************************************************
 * SDL callbacks
 ******************************************************************************/

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

    if (!SDL_CreateWindowAndRenderer(
            APP_TITLE,
            SDL_WINDOW_WIDTH, SDL_WINDOW_HEIGHT,
            SDL_WINDOW_BORDERLESS,
            &app->window.sdl, &app->renderer)
    )
    {
        SDL_Log("Failed to CreateWindowAndRenderer: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_SetRenderVSync(app->renderer, 1))
    {
        SDL_Log("Failed to SetRenderVSync = 1: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    //FONT
    if (!TTF_Init())
    {
        SDL_Log("Failed to initialize SDL_ttf: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    app->font = TTF_OpenFontIO(
        SDL_IOFromConstMem(tiny_ttf, tiny_ttf_len),
        true, 18.0f);
    if (!app->font)
    {
        SDL_Log("Failed to open Font: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    //TEXT RENDER ENGINE
    app->text_engine = TTF_CreateRendererTextEngine(app->renderer);
    if (!app->text_engine)
    {
        SDL_Log("Failed to create TextEngine: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

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
 * background, etc), or it might just call this function in a loop as fast
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
 * Your app should not call SDL_PollEvent, SDL_PumpEvent, etc, as SDL
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
        process_event_key_down(app, event->key);
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

    TTF_CloseFont(app->font);
    TTF_DestroyRendererTextEngine(app->text_engine);
    SDL_DestroyRenderer(app->renderer);
    SDL_DestroyWindow(app->window.sdl);
    SDL_free(app);
}
