#include "buttons.h"
#include <stdio.h>
#include <stdlib.h>

// Helper to load SVG at a specific size to ensure sharpness
SDL_Texture* LoadSVG(SDL_Renderer* renderer, const char* file, int width, int height) {
    SDL_IOStream* io = SDL_IOFromFile(file, "rb");
    if (!io){
        printf("Failed to open SVG file: %s\n", SDL_GetError());
        return NULL;
    } 
    
    SDL_Surface* surface = IMG_LoadSizedSVG_IO(io, width, height);
    if (!surface) {
        SDL_CloseIO(io);
        printf("Failed to load SVG: %s\n", SDL_GetError());
        return NULL;
    }

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
    if (!tex) {
        printf("Failed to create texture from SVG surface: %s\n", SDL_GetError());
        SDL_DestroySurface(surface);
        SDL_CloseIO(io);
        return NULL;
    }
    SDL_DestroySurface(surface); 
    SDL_CloseIO(io);
    return tex;
}

Button* create_button(SDL_Renderer* renderer, ButtonType type, const char* svg_path, int x, int y, int size) {
    Button *btn = malloc(sizeof(Button));
    if (!btn) {
        return NULL;
    }
    btn->type = type;
    btn->rect = (SDL_FRect){x, y, size, size};
    btn->texture = LoadSVG(renderer, svg_path, size, size);
    btn->is_hovered = false;
    btn->on_click = NULL;
    return btn;
}


void render_button(SDL_Renderer* renderer, Button* button, SDL_Color color) {
    if (!button->texture) {
        return;
    }

    SDL_SetTextureBlendMode(button->texture, SDL_BLENDMODE_BLEND);
    if (button->is_hovered) {
        // Apply a simple hover effect by modulating the texture color
        SDL_SetTextureColorMod(button->texture, color.r, color.g, color.b);
    } else {
        SDL_SetTextureColorMod(button->texture, 255, 255, 255); // Original color
    }
    SDL_RenderTexture(renderer, button->texture, NULL, &button->rect);
}

bool is_button_hovered(Button* button, int mouse_x, int mouse_y) {
    return (mouse_x >= button->rect.x && mouse_x <= button->rect.x + button->rect.w &&
            mouse_y >= button->rect.y && mouse_y <= button->rect.y + button->rect.h);
}

void handle_button_hover(Button* buttons[], int button_count, int mouse_x, int mouse_y) {
    for (int i = 0; i < button_count; i++) {
        buttons[i]->is_hovered = is_button_hovered(buttons[i], mouse_x, mouse_y);
    }
}

void destroy_button(Button* button) {
    if (button->texture) {
        SDL_DestroyTexture(button->texture);
        button->texture = NULL;
    }
    free(button);
}

void bind_button_to_function(Button* button, void (*on_click)(ButtonType type)) {
    button->on_click = on_click;
}