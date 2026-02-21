#include "wayland_capture.h"
#include "pipewire_capture.h"
#include <stdio.h>
#include <stdlib.h>
#include <gio/gio.h>
#include <glib.h>

#define PORTAL_BUS_NAME "org.freedesktop.portal.Desktop"
#define SCREENCAST_OBJECT_PATH "/org/freedesktop/portal/desktop"
#define SCREENCAST_INTERFACE "org.freedesktop.portal.ScreenCast"
#define REQUEST_INTERFACE "org.freedesktop.portal.Request"

typedef struct {
    GDBusConnection *connection;
    GMainLoop *loop;
    char *session_path;
} PortalContext;

PortalContext ctx = {0};

static void start_screencast();
static void select_sources();

static void on_start_response(GDBusConnection *connection,
                              const gchar *sender_name,
                              const gchar *object_path,
                              const gchar *interface_name,
                              const gchar *signal_name,
                              GVariant *parameters,
                              gpointer user_data) {
    (void)connection; (void)sender_name; (void)object_path; (void)interface_name; (void)signal_name; (void)user_data;
    
    guint32 response_code;
    GVariant *results;
    g_variant_get(parameters, "(u@a{sv})", &response_code, &results);

    if (response_code != 0) {
        fprintf(stderr, "User denied screen sharing!\n");
        g_main_loop_quit(ctx.loop);
        return;
    }

    printf("\n[Wayland] SUCCESS! User authorized capture.\n");
    
    GVariant *streams;
    if (g_variant_lookup(results, "streams", "@a(ua{sv})", &streams)) {
        GVariantIter iter;
        guint32 node_id;
        GVariant *stream_props;
        
        g_variant_iter_init(&iter, streams);
        if (g_variant_iter_next(&iter, "(u@a{sv})", &node_id, &stream_props)) {
            printf("PipeWire Node ID received: %u\n", node_id);
            init_pipewire_capture(node_id);
        }
        g_variant_unref(stream_props);
        g_variant_unref(streams);
    }
}

static void start_screencast() {
    GError *error = NULL;
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);

    g_variant_builder_add(&builder, "{sv}", "handle_token", g_variant_new_string("start_token_123"));

    GVariant *result = g_dbus_connection_call_sync(
        ctx.connection,
        PORTAL_BUS_NAME,
        SCREENCAST_OBJECT_PATH,
        SCREENCAST_INTERFACE,
        "Start",
        g_variant_new("(os@a{sv})", ctx.session_path, "", g_variant_builder_end(&builder)),
        NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error
    );

    if (!result) {
        fprintf(stderr, "Start failed: %s\n", error->message);
        return;
    }
    
    const gchar *req_path;
    g_variant_get(result, "(&o)", &req_path);
    printf("Waiting for user permission...\n");
    
    g_dbus_connection_signal_subscribe(ctx.connection, PORTAL_BUS_NAME, REQUEST_INTERFACE, "Response", req_path, NULL, G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE, on_start_response, NULL, NULL);
    g_variant_unref(result);
}

static void on_select_sources_response(GDBusConnection *connection,
                                       const gchar *sender_name,
                                       const gchar *object_path,
                                       const gchar *interface_name,
                                       const gchar *signal_name,
                                       GVariant *parameters,
                                       gpointer user_data) {
    (void)connection; (void)sender_name; (void)object_path; (void)interface_name; (void)signal_name; (void)user_data;
    
    guint32 response_code;
    g_variant_get(parameters, "(u@a{sv})", &response_code, NULL);

    if (response_code != 0) {
        fprintf(stderr, "SelectSources failed or cancelled (Code: %d)\n", response_code);
        g_main_loop_quit(ctx.loop);
        return;
    }

    printf("Starting screencast...\n");
    start_screencast();
}

static void select_sources() {
    GError *error = NULL;
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
    
    g_variant_builder_add(&builder, "{sv}", "types", g_variant_new_uint32(1));
    g_variant_builder_add(&builder, "{sv}", "multiple", g_variant_new_boolean(FALSE));

    GVariant *result = g_dbus_connection_call_sync(
        ctx.connection,
        PORTAL_BUS_NAME,
        SCREENCAST_OBJECT_PATH,
        SCREENCAST_INTERFACE,
        "SelectSources",
        g_variant_new("(o@a{sv})", ctx.session_path, g_variant_builder_end(&builder)),
        NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error
    );

    if (!result) {
        fprintf(stderr, "SelectSources failed: %s\n", error->message);
        return;
    }

    const gchar *req_path;
    g_variant_get(result, "(&o)", &req_path);
    g_dbus_connection_signal_subscribe(ctx.connection, PORTAL_BUS_NAME, REQUEST_INTERFACE, "Response", req_path, NULL, G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE, on_select_sources_response, NULL, NULL);
    g_variant_unref(result);
}

static void on_create_session_response(GDBusConnection *connection,
                                       const gchar *sender_name,
                                       const gchar *object_path,
                                       const gchar *interface_name,
                                       const gchar *signal_name,
                                       GVariant *parameters,
                                       gpointer user_data) {
    (void)connection; (void)sender_name; (void)object_path; (void)interface_name; (void)signal_name; (void)user_data;

    guint32 response_code;
    GVariant *results;
    g_variant_get(parameters, "(u@a{sv})", &response_code, &results);

    if (response_code != 0) {
        fprintf(stderr, "Session creation failed (Code: %d)\n", response_code);
        g_main_loop_quit(ctx.loop);
        return;
    }

    const gchar *session_handle = NULL;
    g_variant_lookup(results, "session_handle", "s", &session_handle);
    
    if (session_handle) {
        ctx.session_path = g_strdup(session_handle);
        select_sources();
    }
}

static void create_session() {
    GError *error = NULL;
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&builder, "{sv}", "session_handle_token", g_variant_new_string("second_screen_session"));
    
    GVariant *options = g_variant_builder_end(&builder);
    
    printf("Requesting ScreenCast Session...\n");
    
    GVariant *result = g_dbus_connection_call_sync(
        ctx.connection,
        PORTAL_BUS_NAME,
        SCREENCAST_OBJECT_PATH,
        SCREENCAST_INTERFACE,
        "CreateSession",
        g_variant_new("(@a{sv})", options),
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error
    );

    if (!result) {
        fprintf(stderr, "CreateSession failed: %s\n", error->message);
        g_error_free(error);
        return;
    }

    const gchar *request_path = NULL;
    g_variant_get(result, "(&o)", &request_path);
    
    g_dbus_connection_signal_subscribe(
        ctx.connection,
        PORTAL_BUS_NAME,
        REQUEST_INTERFACE,
        "Response",
        request_path,
        NULL,
        G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
        on_create_session_response,
        NULL,
        NULL
    );

    g_variant_unref(result);
}

static void *wayland_loop_thread(void *arg) {
    (void)arg;
    
    GError *error = NULL;
    ctx.connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    
    if (!ctx.connection) {
        fprintf(stderr, "Failed to connect to D-Bus: %s\n", error->message);
        return NULL;
    }

    ctx.loop = g_main_loop_new(NULL, FALSE);
    create_session();
    g_main_loop_run(ctx.loop);
    
    return NULL;
}

int init_wayland_capture() {
    printf("Initializing Wayland capture thread...\n");
    GThread *thread = g_thread_new("wayland_dbus", wayland_loop_thread, NULL);
    if (!thread) return -1;
    g_thread_unref(thread);
    return 0;
}
