#include "take_ss_w.h"

#include <string.h>

GMainLoop *loop;
char ss_filepath[256];

void filepath_copy(char* dest, const char* src); // Forward declaration of the helper function

// This callback runs when the user finishes interacting with the screenshot UI
static void on_portal_response(GDBusConnection *conn, const gchar *sender, const gchar *path,
                               const gchar *interface, const gchar *signal, GVariant *parameters, gpointer user_data) {
    guint32 response;
    GVariant *results;
    
    // Portal response signal signature is (ua{sv})
    // u: response code (0 = success)
    // a{sv}: dictionary of results
    g_variant_get(parameters, "(u@a{sv})", &response, &results);

    if (response == 0) {
        gchar *uri;
        if (g_variant_lookup(results, "uri", "&s", &uri)) {
            filepath_copy(ss_filepath, uri); // Store the URI in the global variable
        }
    }

    g_variant_unref(results);
    g_main_loop_quit(loop); // Exit the program once we have the result
}

void filepath_copy(char* dest, const char* src) {
    if (src == NULL) {
        dest[0] = '\0';
        return;
    }

    const char *prefix = "file://";
    size_t prefix_len = strlen(prefix);
    const char *path_start = src;

    // Strip file:// prefix if present
    if (strncmp(src, prefix, prefix_len) == 0) {
        path_start = src + prefix_len;
    }

    size_t path_len = strlen(path_start);
    size_t max_len = 255; // ss_filepath is 256 bytes

    if (path_len > max_len) {
        path_len = max_len;
    }

    strncpy(dest, path_start, path_len);
    dest[path_len] = '\0';
}

void take_ss() {
    GDBusConnection *conn;
    GError *error = NULL;
    GVariant *result;

    conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    loop = g_main_loop_new(NULL, FALSE);

    // Subscribe with NULL path — catches Response on ANY request object
    g_dbus_connection_signal_subscribe(
        conn,
        NULL,                                   // Any sender
        "org.freedesktop.portal.Request",
        "Response",
        NULL,                                   // Any object path
        NULL,
        G_DBUS_SIGNAL_FLAGS_NONE,
        on_portal_response,
        NULL, NULL);

    result = g_dbus_connection_call_sync(
        conn, "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.Screenshot", "Screenshot",
        g_variant_new("(s@a{sv})", "", g_variant_new_array(G_VARIANT_TYPE("{sv}"), NULL, 0)),
        G_VARIANT_TYPE("(o)"), G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);

    if (error) {
        fprintf(stderr, "Call Failed: %s\n", error->message);
        g_main_loop_quit(loop);
        return;
    }

    g_variant_unref(result);

    g_main_loop_run(loop);
    g_object_unref(conn);

}

SDL_Surface* take_ss_wayland() {
    take_ss();
    // check if ss_filepath is empty
    if (ss_filepath[0] == '\0') {
        return NULL; // No screenshot taken or an error occurred
    }

    SDL_Surface* surface = IMG_Load(ss_filepath);
    if (surface == NULL) {
        fprintf(stderr, "Error loading screenshot image: %s\n", SDL_GetError());
        return NULL;
    }
    return surface;
}