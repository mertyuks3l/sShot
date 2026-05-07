#pragma once
#ifndef BUTTONS_H
#define BUTTONS_H

#include "SDL3/SDL.h"
#include "SDL3_image/SDL_image.h"

typedef enum ButtonType {
    SAVE,
    COPY,
    FULLSCREEN,
    BUTTON_TYPE_COUNT // This can be used to keep track of the number of button types
} ButtonType;

typedef struct {
    ButtonType type;
    SDL_FRect rect;
    SDL_Texture *texture;
    bool is_hovered;
    void (*on_click)(); // Function pointer for click action
    
} Button;


// Function prototypes
Button* create_button(SDL_Renderer* renderer, ButtonType type, const char* svg_path, int x, int y, int size);
SDL_Texture* LoadSVG(SDL_Renderer* renderer, const char* file, int width, int height);
void render_button(SDL_Renderer* renderer, Button* button, SDL_Color color);
bool is_button_hovered(Button* button, int mouse_x, int mouse_y);
void handle_button_hover(Button* buttons[], int button_count, int mouse_x, int mouse_y);
void destroy_button(Button* button);
void bind_button_to_function(Button* button, void (*on_click)());

#endif // BUTTONS_H