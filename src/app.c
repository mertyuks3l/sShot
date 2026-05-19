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


typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    int width, height;
    bool is_running;
} AppWindow;

typedef struct {
    SDL_Surface* surface;
    SDL_Texture* texture;
    SDL_FRect image_rect;
    float texture_width, texture_height;
} Screenshot;

typedef struct {
    SDL_FRect rect;
    bool is_drawing;
    bool is_dragging;
    float start_x, start_y;
} Selection;

AppWindow app_window;
Screenshot screenshot;
Selection selection;

SDL_Event event;

int current_session;
float zoom_speed = 0.05; // Sensitivity for zooming in/out the image
bool is_mouse_over_buttons = false;

Button *all_buttons[BUTTON_COUNT];
Button *save_button;
Button *copy_button;
Button *fullscreen_button;
int fscreen_button_spacing = 20; // Spacing between buttons

char *icon_path = ASSET_PATH "icon.png"; // This is for the notification icon

// Local function prototypes
bool initialize_window();
void process_input(SDL_Event *event, Button *buttons[]);
void update();
void render();
bool load_assets();
void on_fullscreen_button_click(ButtonType type);
void on_save_button_click(ButtonType type);
void copy_image_to_clipboard();
void on_copy_button_click(ButtonType type);
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
        screenshot.surface = take_ss_wayland();
    } else {
        screenshot.surface = take_ss_x11();
    }

    if (screenshot.surface == NULL) {
        send_notification("sShot Error", "Failed to take screenshot. Please try again.");
        return APP_ERROR_INIT;
    }

    // Initialize SDL, create window and renderer
    if (!initialize_window()) {
        send_notification("sShot Error", "Failed to initialize window.");
        return APP_ERROR_INIT;
    }

    SDL_PumpEvents();
    SDL_SyncWindow(app_window.window);  // Wait for Wayland compositor to configure the window
    // get window size
    SDL_GetWindowSizeInPixels(app_window.window, &app_window.width, &app_window.height);

    screenshot.texture = SDL_CreateTextureFromSurface(app_window.renderer, screenshot.surface);

    if (screenshot.texture == NULL) {
        fprintf(stderr, "Error creating texture from surface: %s\n", SDL_GetError());
        SDL_DestroySurface(screenshot.surface);
        send_notification("sShot Error", "Failed to take screenshot. Please try again.");
        return APP_ERROR_INIT;
    }

    screenshot.image_rect.w = (float)screenshot.surface->w;
    screenshot.image_rect.h = (float)screenshot.surface->h;

    SDL_GetTextureSize(screenshot.texture, &screenshot.texture_width, &screenshot.texture_height);
    
    SDL_SetTextureScaleMode(screenshot.texture, SDL_SCALEMODE_LINEAR);

    // Create buttons
    save_button = create_button(app_window.renderer, SAVE, ASSET_PATH "save_icon.svg", 0, 0, DEFAULT_BUTTON_SIZE);
    copy_button = create_button(app_window.renderer, COPY, ASSET_PATH "copy_icon.svg", 0, 0, DEFAULT_BUTTON_SIZE);
    fullscreen_button = create_button(app_window.renderer, FULLSCREEN, ASSET_PATH "fullscreen_icon.svg", 0, 0, DEFAULT_BUTTON_SIZE);

    // Bind buttons to functions
    bind_button_to_function(save_button, on_save_button_click);
    bind_button_to_function(copy_button, on_copy_button_click);
    bind_button_to_function(fullscreen_button, on_fullscreen_button_click);

    // Top right corner for fullscreen button
    fullscreen_button->rect.x = app_window.width - DEFAULT_BUTTON_SIZE - fscreen_button_spacing;
    fullscreen_button->rect.y = fscreen_button_spacing;

    all_buttons[0] = save_button;
    all_buttons[1] = copy_button;
    all_buttons[2] = fullscreen_button;

    // init notification system
    notify_init(TITLE);

    // Everything is initialized successfully
    app_window.is_running = true;

    return APP_SUCCESS;

    
}

int app_run(void) {
    // Main loop
    while (app_window.is_running) {
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
    if (screenshot.texture) {
        SDL_DestroyTexture(screenshot.texture);
        screenshot.texture = NULL;
    }
    if (screenshot.surface) {
        SDL_DestroySurface(screenshot.surface);
        screenshot.surface = NULL;
    }
    // Free undo stack
    free_undo_stack();
    
    // Destroy renderer, window and quit
    SDL_DestroyRenderer(app_window.renderer);
    SDL_DestroyWindow(app_window.window);
    SDL_Quit();
}

void on_fullscreen_button_click(ButtonType type) {
    (void)type; // unused
    // Make the current rect as the same as the image rect
    selection.rect = screenshot.image_rect; 
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

void on_save_button_click(ButtonType type) {
    (void)type; // unused
    const char* save_path = get_save_path_from_user();
    if (save_path == NULL) {
        fprintf(stderr, "Save cancelled by user.\n");
        return;
    }

    crop_image(); // This will cut both the texture and the surface to the selection_rect

    if (save_image(screenshot.surface, save_path) != true) {
        send_notification("sShot Error", "Failed to save screenshot.");
    } else {
        fprintf(stdout, "Image saved successfully to %s\n", save_path);
        send_notification("sShot", "Screenshot saved successfully!");
        app_window.is_running = false;
    }
}

void copy_image_to_clipboard() {
    // Crop the image
    // save the image to a temporary location 
    // copy the image to clipboard using xclip or wl-copy depending on the session type
    
    crop_image(); 
    
    char* ss_clipboard_filepath = "/tmp/sshot_clipboard.png"; // Temporary file path for the cropped image
    
    if (save_image(screenshot.surface, ss_clipboard_filepath) != true) {
        fprintf(stderr, "Error saving image for clipboard: %s\n", SDL_GetError());
        send_notification("sShot Error", "Failed to copy screenshot to clipboard.");
        return;
    }

    int result = -1;
    if (current_session == WAYLAND) {
        result = system("wl-copy --type image/png < /tmp/sshot_clipboard.png");
    } else {
        result = system("xclip -selection clipboard -t image/png -i /tmp/sshot_clipboard.png");
    }

    if (result != 0) {
        fprintf(stderr, "Error copying image to clipboard\n");
        send_notification("sShot Error", "Failed to copy screenshot to clipboard.");
        return;
    }

    fprintf(stdout, "Image copied to clipboard successfully\n");
    send_notification("sShot", "Screenshot copied to clipboard successfully!");
    app_window.is_running = false; // Exit the application after copying to clipboard
}

void on_copy_button_click(ButtonType type) {
    (void)type; // unused
    copy_image_to_clipboard();
}

bool initialize_window() {
    if (SDL_Init(SDL_INIT_VIDEO) != true) {
           fprintf(stderr, "Error initializing SDL: %s\n", SDL_GetError());
           return false;
    }
    
    if (SDL_CreateWindowAndRenderer(TITLE, 1920, 1080, SDL_WINDOW_FULLSCREEN, &app_window.window, &app_window.renderer) != true) {
        return false;
    }

    if (app_window.window == NULL || app_window.renderer == NULL) {
        fprintf(stderr, "Error creating window or renderer: %s\n", SDL_GetError());
        return false;
    }

    if (SDL_SetRenderVSync(app_window.renderer, 1) != true) {
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
                    app_window.is_running = false;
                    return;
                case SDL_EVENT_KEY_UP:
                    break;
                case SDL_EVENT_KEY_DOWN:
                    SDL_Keymod mod = event->key.mod;
                    SDL_Keycode key = event->key.key;

                    bool ctrl = (mod & SDL_KMOD_CTRL) != 0;

                    if (key == SDLK_ESCAPE || key == SDLK_Q) {
                        app_window.is_running = false;
                        return;
                    }
                    if (ctrl && key == SDLK_Z) {
                        undo(app_window.renderer, &screenshot.surface, &screenshot.texture, &screenshot.image_rect, &screenshot.texture_width, &screenshot.texture_height);
                        selection.rect = screenshot.image_rect; // Set selection_rect to the new image_rect after undoing
                    }
                    if (ctrl && key == SDLK_C){
                        copy_image_to_clipboard();
                    }
                    if (ctrl && key == SDLK_S){
                        on_save_button_click(SAVE);
                    }
                    break;

                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (event->button.button == SDL_BUTTON_LEFT) {
                    mouse_left_button_down(event, &selection.is_drawing, &selection.is_dragging, &selection.start_x, &selection.start_y, &selection.rect, buttons);
                }
                break;

            case SDL_EVENT_MOUSE_MOTION:
                if (selection.is_drawing || selection.is_dragging) {
                    // Calculate width/height based on current mouse pos
                    mouse_motion(event, &selection.is_drawing, &selection.is_dragging, &selection.start_x, &selection.start_y, &selection.rect);
                }
                break;

            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (event->button.button == SDL_BUTTON_LEFT) {
                    mouse_left_button_up(event, &selection.is_drawing, &selection.is_dragging, &selection.start_x, &selection.start_y, &selection.rect, &save_button->rect, &copy_button->rect);
                }
                if (event->button.button == SDL_BUTTON_RIGHT) {
                    crop_image(); // Cut the image to the selection_rect when right-clicking (for testing purposes, can change this later)
                }
                break;
            
            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                app_window.width = event->window.data1;
                app_window.height = event->window.data2;

                // Update fullscreen button position
                fullscreen_button->rect.x = app_window.width - DEFAULT_BUTTON_SIZE - fscreen_button_spacing;
                fullscreen_button->rect.y = fscreen_button_spacing;

                // Re-center the image
                screenshot.image_rect.x = (app_window.width - screenshot.image_rect.w) / 2;
                screenshot.image_rect.y = (app_window.height - screenshot.image_rect.h) / 2;

                break;
                
            case SDL_EVENT_MOUSE_WHEEL:
                if (event->wheel.y > 0) {
                    // Zoom in
                    screenshot.image_rect.w = screenshot.image_rect.w * zoom_speed + screenshot.image_rect.w;
                    screenshot.image_rect.h = screenshot.image_rect.h * zoom_speed + screenshot.image_rect.h;

                    zoom_speed = (zoom_speed < 0.30) ? zoom_speed + 0.02 : 0.30;
                } else if (event->wheel.y < 0) {
                    // Zoom out, but prevent the image from becoming too small
                    if (screenshot.image_rect.w > zoom_speed && screenshot.image_rect.h > zoom_speed) {
                        screenshot.image_rect.w = screenshot.image_rect.w - screenshot.image_rect.w * zoom_speed;
                        screenshot.image_rect.h = screenshot.image_rect.h - screenshot.image_rect.h * zoom_speed; 

                        zoom_speed = (zoom_speed > 0.10) ? zoom_speed - 0.02 : 0.10;
                    }
                }
                // Re-center the image after zooming
                screenshot.image_rect.x = (app_window.width - screenshot.image_rect.w) / 2;
                screenshot.image_rect.y = (app_window.height - screenshot.image_rect.h) / 2;
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
    SDL_SetRenderDrawColorStruct(app_window.renderer, BACKGROUND_COLOR);
    SDL_RenderClear(app_window.renderer);
    
    // Render the loaded image
    if (screenshot.texture) {
        SDL_RenderTexture(app_window.renderer, screenshot.texture, NULL, &screenshot.image_rect);
    }

    // Draw selection rectangle
    if (selection.is_drawing || selection.is_dragging || (selection.rect.w != 0 && selection.rect.h != 0)) {
        // Draw the outline
        SDL_SetRenderDrawColorStruct(app_window.renderer, COLOR_SEMI_TRANSPARENT_BLUE);
        SDL_RenderRect(app_window.renderer, &selection.rect);
    }

    // Calculate button positions based on selection.rect
    if (selection.rect.w != 0 && selection.rect.h != 0 && !(selection.rect.w >= app_window.width || selection.rect.h >= app_window.height)) {
        // Position the buttons at the center and below of selection rectangle
        copy_button->rect.x = selection.rect.x + selection.rect.w / 2 - copy_button->rect.w; 
        copy_button->rect.y = selection.rect.y + selection.rect.h + 5; // 5 pixels below the rectangle
        save_button->rect.x = selection.rect.x + selection.rect.w / 2 - save_button->rect.w - copy_button->rect.w - 5; // 5 pixels to the left of the copy button
        save_button->rect.y = selection.rect.y + selection.rect.h + 5; // 5 pixels below the rectangle
    } else {
        // If there is no selection rectangle yet, position the buttons at the bottom right corner
        copy_button->rect.x = app_window.width - copy_button->rect.w;
        copy_button->rect.y = app_window.height - copy_button->rect.h - 5;
        save_button->rect.x = app_window.width - save_button->rect.w - copy_button->rect.w - 5;
        save_button->rect.y = app_window.height - save_button->rect.h - 5;

    }
    // Draw buttons
    for (int i = 0; i < BUTTON_COUNT; i++) {
        render_button(app_window.renderer, all_buttons[i], COLOR_BUTTON_HOVER);
    }
    // Present the rendered frame to the screen
    SDL_RenderPresent(app_window.renderer);
    return;
}

void crop_image() {
    if (compare_frects(selection.rect, screenshot.image_rect)) return;
    if (selection.rect.w <= 0 || selection.rect.h <= 0) return;

     // Compute how much of the surface each logical pixel represents
    float scale_x = (float)screenshot.surface->w / screenshot.image_rect.w;
    float scale_y = (float)screenshot.surface->h / screenshot.image_rect.h;

    // selection.rect is relative to the window, make it relative to image_rect
    float rel_x = selection.rect.x - screenshot.image_rect.x;
    float rel_y = selection.rect.y - screenshot.image_rect.y;

    // Map selection into surface space
    SDL_Rect surface_crop = {
        .x = (int)(rel_x * scale_x),
        .y = (int)(rel_y * scale_y),
        .w = (int)(selection.rect.w * scale_x),
        .h = (int)(selection.rect.h * scale_y),
    };

    // Clamp to surface bounds
    if (surface_crop.x < 0) surface_crop.x = 0;
    if (surface_crop.y < 0) surface_crop.y = 0;
    if (surface_crop.x + surface_crop.w > screenshot.surface->w)
        surface_crop.w = screenshot.surface->w - surface_crop.x;
    if (surface_crop.y + surface_crop.h > screenshot.surface->h)
        surface_crop.h = screenshot.surface->h - surface_crop.y;

    // Cut the surface
    SDL_Surface *cropped = SDL_CreateSurface(
        surface_crop.w, surface_crop.h,
        screenshot.surface->format
    );

    SDL_BlitSurface(screenshot.surface, &surface_crop, cropped, NULL);

    push_undo_state(app_window.renderer, screenshot.surface, screenshot.texture, screenshot.image_rect, screenshot.texture_width, screenshot.texture_height);

    // Replace original surface and rebuild texture
    SDL_DestroySurface(screenshot.surface);
    screenshot.surface = cropped;

    SDL_DestroyTexture(screenshot.texture);
    screenshot.texture = SDL_CreateTextureFromSurface(app_window.renderer, screenshot.surface);

    // Reset image_rect to match new texture dimensions
    screenshot.image_rect.w = (float)screenshot.surface->w / scale_x;
    screenshot.image_rect.h = (float)screenshot.surface->h / scale_y;
    screenshot.image_rect.x = (app_window.width - screenshot.image_rect.w) / 2;
    screenshot.image_rect.y = (app_window.height - screenshot.image_rect.h) / 2;

    selection.rect = screenshot.image_rect; // Set selection.rect to the new image_rect after cutting
}

