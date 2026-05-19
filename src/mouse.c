#include "mouse.h"
#include <stdio.h>

static float dx, dy;

void mouse_left_button_down(SDL_Event *event, bool *is_drawing, bool *is_dragging, float *start_x, float *start_y, SDL_FRect *current_rect, Button *buttons[]) {
    // Handle left button down event
    *is_drawing = true;
    *is_dragging = false;

    // Check if the click is within the one of the buttons
    for (int i = 0; i < BUTTON_TYPE_COUNT; i++) {
        if (is_button_hovered(buttons[i], event->button.x, event->button.y)) {
            // Clicked on a button, so don't start drawing a new rect
            *is_drawing = false;
            // Call the button's function
            if (buttons[i]->on_click != NULL) {
                buttons[i]->on_click(buttons[i]->type); // Pass the button's type as an argument
            }
            return;
        }
    }
    *start_x = event->button.x;
    *start_y = event->button.y;

    // if the mouse start point is inside the current rect, we should not start a new rect, but instead allow dragging the existing one
    if (current_rect->w != 0 && current_rect->h != 0) {
        if (event->button.x >= current_rect->x && event->button.x <= current_rect->x + current_rect->w &&
            event->button.y >= current_rect->y && event->button.y <= current_rect->y + current_rect->h) {
            // Clicked inside the existing rect, so we should not start a new one
            *is_drawing = false;
            *is_dragging = true;
            // Calculate the offset for dragging
            dx = current_rect->x - event->button.x;
            dy = current_rect->y - event->button.y;
            return;
        }
    }


    // Initialize rect at click point
    current_rect->x = *start_x;
    current_rect->y = *start_y;
    current_rect->w = 0;
    current_rect->h = 0;
}

void mouse_motion(SDL_Event *event, bool *is_drawing, bool *is_dragging, float *start_x, float *start_y, SDL_FRect *current_rect) {
    if (*is_drawing) {
        // Calculate width/height based on current mouse pos
        current_rect->w = event->motion.x - *start_x;
        current_rect->h = event->motion.y - *start_y;
        
        return;
    }
    if (*is_dragging) {
        // Update rect position when dragging
        current_rect->x = event->motion.x  + dx;
        current_rect->y = event->motion.y + dy;
        
        return;
    }
}


void mouse_left_button_up(SDL_Event *event, bool *is_drawing, bool *is_dragging, float *start_x, float *start_y, SDL_FRect *current_rect, SDL_FRect *save_button_rect, SDL_FRect *copy_button_rect) {
    *is_drawing = false;
    *is_dragging = false;
    // Handle "negative" dragging (dragging up/left)
    if (current_rect->w < 0) {
        current_rect->x += current_rect->w;
        current_rect->w = -current_rect->w;
    }
    if (current_rect->h < 0) {
        current_rect->y += current_rect->h;
        current_rect->h = -current_rect->h;
    }

    // Update button positions based on final rect
    save_button_rect->x = current_rect->x + current_rect->w - save_button_rect->w;
    save_button_rect->y = current_rect->y + current_rect->h + 10; // 10 pixels below the rect
    copy_button_rect->x = save_button_rect->x - copy_button_rect->w - 10; // 10 pixels to the left of save button
    copy_button_rect->y = save_button_rect->y;
}
