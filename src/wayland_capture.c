#include "wayland_capture.h"
#include <stdio.h>
#include <stdlib.h>
#include <gio/gio.h>

#define PORTAL_BUS_NAME "org.freedesktop.portal.Desktop"
#define SCREENCAST_OBJECT_PATH "/org/freedesktop/portal/desktop"
#define SCREENCAST_INTERFACE "org.freedesktop.portal.ScreenCast"

int init_wayland_capture() {
    printf("[Wayland] Iniciando requisição para captura de tela via XDG Desktop Portal...\n");

    GError *error = NULL;
    GDBusConnection *connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    
    if (!connection) {
        fprintf(stderr, "[Wayland] Erro ao conectar no D-Bus: %s\n", error->message);
        g_error_free(error);
        return -1;
    }

    printf("[Wayland] Conexão D-Bus estabelecida com sucesso.\n");
    
    // Preparar as opções para CreateSession (um dicionário de string -> variant)
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&builder, "{sv}", "session_handle_token", g_variant_new_string("second_screen_session"));
    
    GVariant *options = g_variant_builder_end(&builder);
    
    printf("[Wayland] Solicitando nova Sessão de ScreenCast ao XDG Portal...\n");
    
    // Fazer a chamada síncrona para CreateSession. O dbus sempre espera os args numa tupla.
    GVariant *result = g_dbus_connection_call_sync(
        connection,
        PORTAL_BUS_NAME,
        SCREENCAST_OBJECT_PATH,
        SCREENCAST_INTERFACE,
        "CreateSession",
        g_variant_new("(@a{sv})", options),
        NULL, // Return type
        G_DBUS_CALL_FLAGS_NONE,
        -1, // Timeout
        NULL,
        &error
    );

    if (!result) {
        fprintf(stderr, "[Wayland] Falha ao criar sessão de ScreenCast: %s\n", error->message);
        g_error_free(error);
        g_object_unref(connection);
        return -1;
    }

    // O retorno de CreateSession é o path do objeto da requisição (tipo 'o')
    const char *request_handle = NULL;
    g_variant_get(result, "(&o)", &request_handle);
    
    printf("\n>>> [SUCESSO] Sessão criada no Portal! Handle da Requisição: %s\n", request_handle);
    
    g_variant_unref(result);
    g_object_unref(connection);

    return 0;
}
