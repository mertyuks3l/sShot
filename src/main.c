// The runner

#include "app.h"

// Command line arguments can be added later for things like window size, dev mode, etc.
int main(void) {
    
    if (app_init() != APP_SUCCESS) {
        return 1;
    }
    uint8_t status = app_run();
    app_quit();
    return status;
}