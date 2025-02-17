#include <stdio.h>
#include <stdint.h>
#include <malloc.h>

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/*
 * C ROGUELIKE FRAMEWORK *******************************************************
 * This is a lightweight C framework in preparation for 7drl 2025.
 * It also serves as a learning project to get started learning SDL3.
 *
 * Ideally the only dependency is SDL.
 * So instead of the typical route via stb, miniaudio and so on we want to do
 * everything with SDL. (we'll probably need stb_image and stb_truetype)
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

const int SDL_WINDOW_WIDTH  = 1920;
const int SDL_WINDOW_HEIGHT = 1080;

typedef struct
{
    uint8_t pos_x;
    uint8_t pos_y;
} Player;

typedef struct
{
    Player player;
} Game;

typedef struct
{
    Game game;
    SDL_Window* window;
    SDL_Renderer* renderer;
} App;

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
        return SDL_APP_FAILURE;

    if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO))
        return SDL_APP_FAILURE;

    App* app = SDL_malloc(sizeof(App));
    if (app == NULL)
        return SDL_APP_FAILURE;

    *appstate = app;

    if (!SDL_CreateWindowAndRenderer(
        APP_TITLE,
        SDL_WINDOW_WIDTH, SDL_WINDOW_HEIGHT,
        SDL_WINDOW_BORDERLESS,
        &app->window, &app->renderer)
    ) return SDL_APP_FAILURE;

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
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    switch (event->type)
    {
    case SDL_EVENT_QUIT: return SDL_APP_SUCCESS;
    default: return SDL_APP_CONTINUE;
    }
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
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    if (appstate != NULL)
    {
        App* app = (App*)appstate;
        SDL_DestroyRenderer(app->renderer);
        SDL_DestroyWindow(app->window);
        SDL_free(app);
    }
}