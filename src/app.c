#include "app.h"
#include "buttons.h"
#include "colors.h"
#include "mouse.h"
#include "undo.h"

#include "wayland/take_ss_w.h"
#include "X11/take_ss_x.h"

#define ASSET_PATH "/usr/share/sshot/"

#define TITLE "sShot" // Window title(may be changed later)
#define DEFAULT_WINDOW_WIDTH 800
#define DEFAULT_WINDOW_HEIGHT 600
#define DEFAULT_BUTTON_SIZE 20
#define BUTTON_COUNT 3

int current_session;

SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
int window_width = 0;
int window_height = 0;
bool is_running = true;
SDL_Event event;

SDL_FRect selection_rect = {0, 0, 0, 0}; // To store the current selection rectangle

Button *save_button;
Button *copy_button;
Button *fullscreen_button;
int fscreen_button_spacing = 20; // Spacing between buttons

SDL_Surface* original_surface = NULL; 
SDL_Texture* display_texture = NULL;
SDL_FRect image_rect = {0, 0, 0, 0}; // To store the position and size of the loaded image
float display_texture_width, display_texture_height; 
float zoom_sens = 0.05; // Sensitivity for zooming in/out the image


bool is_drawing_selection_rect = false;
bool is_dragging_selection_rect = false;
bool is_mouse_over_buttons = false;
float start_x, start_y;

Button *all_buttons[BUTTON_COUNT];

char *icon_path = ASSET_PATH "icon.png"; // This is for the notification icon

// Local function prototypes
bool initialize_window();
void process_input(SDL_Event *event, Button *buttons[]);
void update();
void render();
bool load_assets();
void on_fullscreen_button_click();
void on_save_button_click();
void copy_image_to_clipboard();
void on_copy_button_click();
void handle_window_resize();
void zoomin_image();
void zoomout_image();
void crop_image();


int get_session() {
    return getenv("WAYLAND_DISPLAY") != NULL ? WAYLAND : X11;
}

void send_notification(const char *title, const char *message) {
    // Create the notification: (Title, Body, Icon)
    NotifyNotification *n = notify_notification_new(title, 
                                                    message, 
                                                    icon_path);

    // Show it
    notify_notification_show(n, NULL);

    // Clean up
    g_object_unref(G_OBJECT(n));
}


// Global functions
int app_init(void){
    // Get the current session type first
    current_session = get_session();

    // Take screenshot according to the session type and store the file path in a global variable
    if (current_session == WAYLAND) {
        original_surface = take_ss_wayland();
    } else {
        original_surface = take_ss_x11();
    }

    if (original_surface == NULL) {
        send_notification("sShot Error", "Failed to take screenshot. Please try again.");
        return APP_ERROR_INIT;
    }

    // Initialize SDL, create window and renderer
    if (!initialize_window()) {
        send_notification("sShot Error", "Failed to initialize window.");
        return APP_ERROR_INIT;
    }

    SDL_PumpEvents();
    SDL_SyncWindow(window);  // Wait for Wayland compositor to configure the window
    // get window size
    SDL_GetWindowSizeInPixels(window, &window_width, &window_height);

    display_texture = SDL_CreateTextureFromSurface(renderer, original_surface);

    if (display_texture == NULL) {
        fprintf(stderr, "Error creating texture from surface: %s\n", SDL_GetError());
        SDL_DestroySurface(original_surface);
        send_notification("sShot Error", "Failed to take screenshot. Please try again.");
        return APP_ERROR_INIT;
    }

    image_rect.w = (float)original_surface->w;
    image_rect.h = (float)original_surface->h;

    SDL_GetTextureSize(display_texture, &display_texture_width, &display_texture_height);
    
    SDL_SetTextureScaleMode(display_texture, SDL_SCALEMODE_LINEAR);

    // Create buttons
    save_button = create_button(renderer, SAVE, ASSET_PATH "save_icon.svg", 0, 0, DEFAULT_BUTTON_SIZE);
    copy_button = create_button(renderer, COPY, ASSET_PATH "copy_icon.svg", 0, 0, DEFAULT_BUTTON_SIZE);
    fullscreen_button = create_button(renderer, FULLSCREEN, ASSET_PATH "fullscreen_icon.svg", 0, 0, DEFAULT_BUTTON_SIZE);

    // Bind buttons to functions
    bind_button_to_function(save_button, on_save_button_click);
    bind_button_to_function(copy_button, on_copy_button_click);
    bind_button_to_function(fullscreen_button, on_fullscreen_button_click);

    // Top right corner for fullscreen button
    fullscreen_button->rect.x = window_width - DEFAULT_BUTTON_SIZE - fscreen_button_spacing;
    fullscreen_button->rect.y = fscreen_button_spacing;

    for (int i = 0; i < BUTTON_COUNT; i++) {
        all_buttons[i] = (i == 0) ? save_button : (i == 1) ? copy_button : fullscreen_button;
    }

    // init notification system
    notify_init(TITLE);

    // Everything is initialized successfully
    is_running = true;

    return APP_SUCCESS;

    
}

int app_run(void) {
    // Main loop
    while (is_running) {
        process_input(&event, all_buttons);
        update();
        render();
    }
    return APP_SUCCESS;
}

void app_quit() { 
    // Cleanup notification system
    notify_uninit();

    // Destroy buttons
    destroy_button(save_button);
    destroy_button(copy_button);
    destroy_button(fullscreen_button);

    // Free loaded image
    if (display_texture) {
        SDL_DestroyTexture(display_texture);
        display_texture = NULL;
    }
    if (original_surface) {
        SDL_DestroySurface(original_surface);
        original_surface = NULL;
    }
    // Free undo stack
    free_undo_stack();
    
    // Destroy renderer, window and quit
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void on_fullscreen_button_click() {
    // Make the current rect as the same as the image rect
    selection_rect = image_rect; 
}

typedef struct {
    char *result;   // heap-allocated copy of chosen path, or NULL on cancel/error
    bool done;
} SaveDialogResult;

static void save_dialog_callback(void *userdata, const char * const *filelist, int filter) {
    SaveDialogResult *res = (SaveDialogResult *)userdata;
    if (filelist && filelist[0]) {
        res->result = SDL_strdup(filelist[0]);
    } else {
        res->result = NULL;
    }
    res->done = true;
}

char *get_save_path_from_user(void) {
    // Build default filename
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char defaultFilename[128];
    strftime(defaultFilename, sizeof(defaultFilename),
             "screenshot_%Y-%m-%d_%H-%M-%S.png", t);

    SaveDialogResult res = { NULL, false };

    SDL_ShowSaveFileDialog(
        save_dialog_callback,       // callback
        &res,                       // userdata
        NULL,                       // parent window (NULL = no parent)
        NULL,                       // filter array
        0,                          // number of filters
        defaultFilename             // default filename
    );

    // Pump events until the dialog signals completion
    SDL_Event event;
    while (!res.done) {
        SDL_WaitEvent(&event);      // blocks until any event arrives
        SDL_PumpEvents();
    }

    return res.result;
}

bool save_image(SDL_Surface *surface, const char *path) {
    if (IMG_SavePNG(surface, path) != true) {
        fprintf(stderr, "Error saving image: %s\n", SDL_GetError());
        return false;
    } else {
        return true;
    }
}

void on_save_button_click() {
    const char* save_path = get_save_path_from_user();
    if (save_path == NULL) {
        fprintf(stderr, "Save cancelled by user.\n");
        return;
    }

    crop_image(); // This will cut both the texture and the surface to the selection_rect

    if (save_image(original_surface, save_path) != true) {
        fprintf(stderr, "Error saving image: %s\n", SDL_GetError());
        send_notification("sShot Error", "Failed to save screenshot.");
    } else {
        fprintf(stdout, "Image saved successfully to %s\n", save_path);
        send_notification("sShot", "Screenshot saved successfully!");
        is_running = false;
    }
}

void copy_image_to_clipboard() {
    // Crop the image
    // save the image to a temporary location 
    // copy the image to clipboard using xclip or wl-copy depending on the session type
    
    crop_image(); 
    
    char* ss_clipboard_filepath = "/tmp/sshot_clipboard.png"; // Temporary file path for the cropped image
    
    if (save_image(original_surface, ss_clipboard_filepath) != true) {
        fprintf(stderr, "Error saving image for clipboard: %s\n", SDL_GetError());
        send_notification("sShot Error", "Failed to copy screenshot to clipboard.");
        return;
    }
    
    if (current_session == WAYLAND) {
        // Use wl-copy to copy the image to clipboard
        if (system("cat /tmp/sshot_clipboard.png | wl-copy --type image/png") != 0) {
            fprintf(stderr, "Error copying image to clipboard with wl-copy\n");
            send_notification("sShot Error", "Failed to copy screenshot to clipboard.");
        } else {
            fprintf(stdout, "Image copied to clipboard successfully using wl-copy\n");
        }
    } else {
        // Use xclip to copy the image to clipboard
        if (system("xclip -selection clipboard -t image/png -i /tmp/sshot_clipboard.png") != 0) {
            fprintf(stderr, "Error copying image to clipboard with xclip\n");
            send_notification("sShot Error", "Failed to copy screenshot to clipboard.");
        } else {
            fprintf(stdout, "Image copied to clipboard successfully using xclip\n");
        }
    }
    
    send_notification("sShot", "Screenshot copied to clipboard successfully!");
    is_running = false; // Exit the application after copying to clipboard
}

void on_copy_button_click() {
    copy_image_to_clipboard();
}

bool initialize_window() {
    if (SDL_Init(SDL_INIT_VIDEO) != true) {
           fprintf(stderr, "Error initializing SDL: %s\n", SDL_GetError());
           return false;
    }
    
    if (SDL_CreateWindowAndRenderer(TITLE, 1920, 1080, SDL_WINDOW_FULLSCREEN, &window, &renderer) != true) {
        return false;
    }

    if (window == NULL || renderer == NULL) {
        fprintf(stderr, "Error creating window or renderer: %s\n", SDL_GetError());
        return false;
    }

    if (SDL_SetRenderVSync(renderer, 1) != true) {
        fprintf(stderr, "Error enabling VSync: %s\n", SDL_GetError());
        return false;
    }

    return true;
}


void process_input(SDL_Event *event, Button *buttons[]) {
    while (SDL_PollEvent(event))
        {
            switch (event->type)
            {
              case SDL_EVENT_QUIT:
                    is_running = false;
                    return;
                case SDL_EVENT_KEY_UP:
                    break;
                case SDL_EVENT_KEY_DOWN:
                    SDL_Keymod mod = event->key.mod;
                    SDL_Keycode key = event->key.key;

                    bool ctrl = (mod & SDL_KMOD_CTRL) != 0;

                    if (key == SDLK_ESCAPE || key == SDLK_Q) {
                        is_running = false;
                        return;
                    }
                    if (ctrl && key == SDLK_Z) {
                        undo(renderer, &original_surface, &display_texture, &image_rect, &display_texture_width, &display_texture_height);
                        selection_rect = image_rect; // Set selection_rect to the new image_rect after undoing
                    }
                    if (ctrl && key == SDLK_C){
                        copy_image_to_clipboard();
                    }
                    if (ctrl && key == SDLK_S){
                        on_save_button_click();
                    }
                    break;

                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (event->button.button == SDL_BUTTON_LEFT) {
                    mouse_left_button_down(event, &is_drawing_selection_rect, &is_dragging_selection_rect, &start_x, &start_y, &selection_rect, buttons);
                }
                break;

            case SDL_EVENT_MOUSE_MOTION:
                if (is_drawing_selection_rect || is_dragging_selection_rect) {
                    // Calculate width/height based on current mouse pos
                    mouse_motion(event, &is_drawing_selection_rect, &is_dragging_selection_rect, &start_x, &start_y, &selection_rect);
                }
                break;

            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (event->button.button == SDL_BUTTON_LEFT) {
                    mouse_left_button_up(event, &is_drawing_selection_rect, &is_dragging_selection_rect, &start_x, &start_y, &selection_rect, &save_button->rect, &copy_button->rect);
                }
                if (event->button.button == SDL_BUTTON_RIGHT) {
                    crop_image(); // Cut the image to the selection_rect when right-clicking (for testing purposes, can change this later)
                }
                break;
            
            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                window_width = event->window.data1;
                window_height = event->window.data2;

                // Update fullscreen button position
                fullscreen_button->rect.x = window_width - DEFAULT_BUTTON_SIZE - fscreen_button_spacing;
                fullscreen_button->rect.y = fscreen_button_spacing;

                // Re-center the image
                image_rect.x = (window_width - image_rect.w) / 2; // Center the image
                image_rect.y = (window_height - image_rect.h) / 2;

                break;
                
            case SDL_EVENT_MOUSE_WHEEL:
                if (event->wheel.y > 0) {
                    // Zoom in
                    image_rect.w = image_rect.w * zoom_sens + image_rect.w; // Increase width by a percentage
                    image_rect.h = image_rect.h * zoom_sens + image_rect.h;

                    zoom_sens = (zoom_sens < 0.30) ? zoom_sens + 0.02 : 0.30;
                } else if (event->wheel.y < 0) {
                    // Zoom out, but prevent the image from becoming too small
                    if (image_rect.w > zoom_sens && image_rect.h > zoom_sens) {
                        image_rect.w = image_rect.w - image_rect.w * zoom_sens; // Decrease width by a percentage
                        image_rect.h = image_rect.h - image_rect.h * zoom_sens; 

                        zoom_sens = (zoom_sens > 0.10) ? zoom_sens - 0.02 : 0.10;
                    }
                }
                // Re-center the image after zooming
                image_rect.x = (window_width - image_rect.w) / 2; // Center the image
                image_rect.y = (window_height - image_rect.h) / 2;
                break;
            
            }
        }

    return;
}

void update() {
    float mouse_x, mouse_y;
    SDL_GetMouseState(&mouse_x, &mouse_y);
    handle_button_hover(all_buttons, BUTTON_COUNT, mouse_x, mouse_y);
    return;
}

void render() {
    SDL_SetRenderDrawColorStruct(renderer, BACKGROUND_COLOR);
    SDL_RenderClear(renderer);
    
    // Render the loaded image
    if (display_texture) {
        SDL_RenderTexture(renderer, display_texture, NULL, &image_rect);
    }

    // Draw selection rectangle
    if (is_drawing_selection_rect || is_dragging_selection_rect || (selection_rect.w != 0 && selection_rect.h != 0)) {
        // Draw the outline
        SDL_SetRenderDrawColorStruct(renderer, COLOR_SEMI_TRANSPARENT_BLUE);
        SDL_RenderRect(renderer, &selection_rect);
    }

    // Calculate button positions based on selection_rect
    if (selection_rect.w != 0 && selection_rect.h != 0 && !(selection_rect.w >= window_width || selection_rect.h >= window_height)) {
        // Position the buttons at the center and below of selection rectangle
        copy_button->rect.x = selection_rect.x + selection_rect.w / 2 - copy_button->rect.w; 
        copy_button->rect.y = selection_rect.y + selection_rect.h + 5; // 5 pixels below the rectangle
        save_button->rect.x = selection_rect.x + selection_rect.w / 2 - save_button->rect.w - copy_button->rect.w - 5; // 5 pixels to the left of the copy button
        save_button->rect.y = selection_rect.y + selection_rect.h + 5; // 5 pixels below the rectangle
    } else {
        // If there is no selection rectangle yet, position the buttons at the bottom right corner
        copy_button->rect.x = window_width - copy_button->rect.w;
        copy_button->rect.y = window_height - copy_button->rect.h - 5;
        save_button->rect.x = window_width - save_button->rect.w - copy_button->rect.w - 5;
        save_button->rect.y = window_height - save_button->rect.h - 5;

    }
    // Draw buttons
    for (int i = 0; i < BUTTON_COUNT; i++) {
        render_button(renderer, all_buttons[i], COLOR_BUTTON_HOVER);
    }
    // Present the rendered frame to the screen
    SDL_RenderPresent(renderer);
    return;
}

void crop_image() {
    if (compare_frects(selection_rect, image_rect)) return;
    if (selection_rect.w <= 0 || selection_rect.h <= 0) return;

     // Compute how much of the surface each logical pixel represents
    float scale_x = (float)original_surface->w / image_rect.w;
    float scale_y = (float)original_surface->h / image_rect.h;

    // selection_rect is relative to the window, make it relative to image_rect
    float rel_x = selection_rect.x - image_rect.x;
    float rel_y = selection_rect.y - image_rect.y;

    // Map selection into surface space
    SDL_Rect surface_crop = {
        .x = (int)(rel_x * scale_x),
        .y = (int)(rel_y * scale_y),
        .w = (int)(selection_rect.w * scale_x),
        .h = (int)(selection_rect.h * scale_y),
    };

    // Clamp to surface bounds
    if (surface_crop.x < 0) surface_crop.x = 0;
    if (surface_crop.y < 0) surface_crop.y = 0;
    if (surface_crop.x + surface_crop.w > original_surface->w)
        surface_crop.w = original_surface->w - surface_crop.x;
    if (surface_crop.y + surface_crop.h > original_surface->h)
        surface_crop.h = original_surface->h - surface_crop.y;

    // Cut the surface
    SDL_Surface *cropped = SDL_CreateSurface(
        surface_crop.w, surface_crop.h,
        original_surface->format
    );

    SDL_BlitSurface(original_surface, &surface_crop, cropped, NULL);

    push_undo_state(renderer, original_surface, display_texture, image_rect, display_texture_width, display_texture_height);

    // Replace original surface and rebuild texture
    SDL_DestroySurface(original_surface);
    original_surface = cropped;

    SDL_DestroyTexture(display_texture);
    display_texture = SDL_CreateTextureFromSurface(renderer, original_surface);

    // Reset image_rect to match new texture dimensions
    image_rect.w = (float)original_surface->w / scale_x;
    image_rect.h = (float)original_surface->h / scale_y;
    image_rect.x = (window_width - image_rect.w) / 2;
    image_rect.y = (window_height - image_rect.h) / 2;

    selection_rect = image_rect; // Set selection_rect to the new image_rect after cutting
}

