/*
The header file for all the global variables and functions
*/

#ifndef APP_H
#define APP_H

#include <stdio.h>
#include <stdlib.h>

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include <libnotify/notify.h>

// helper macro to set draw color using SDL_Color struct
#define SDL_SetRenderDrawColorStruct(renderer, color) SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
#define compare_frects(a, b) \
    (((a).x == (b).x) && ((a).y == (b).y) && ((a).w == (b).w) && ((a).h == (b).h))

// Program succes enum 
enum {
    APP_SUCCESS = 0,
    APP_FAILURE = 1, // General failure, can be used for any error
    APP_ERROR_INIT = 2,
    APP_ERROR_ASSET_LOAD = 3,
    APP_ERROR_RENDER = 4,
    // TODO: Add more specific error codes later (e.g., APP_ERROR_WINDOW_INIT, APP_ERROR_ASSET_LOAD, etc.)
};


enum Session{
    WAYLAND,
    X11,
};

// Function prototypes
int app_init(void);
int app_run(void);
void app_quit(void);

#endif // APP_H