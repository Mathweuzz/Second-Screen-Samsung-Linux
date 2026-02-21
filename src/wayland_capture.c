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

    // Pega o caminho ONDE a sessão foi criada lá dentro do variant da tupla '(o)' retornado.
    // O retorno de CreateSession (e das outras) na verdade vem pelo Sinal de Resposta, não direto no objeto.
    // Mas para fins de manter a didática, vamos extrair a string da Request Handle gerada.
    const gchar *request_handle = NULL;
    g_variant_get(result, "(&o)", &request_handle);
    
    printf("\n>>> [SUCESSO] Sessão requerida! Handle da Requisição: %s\n", request_handle);
    
    // NOTA: No padrão XDG, o CreateSession retorna o Request Object Path.
    // Precisamos escutar o sinal 'Response' desse request para pegar o 'Session Object Path' real,
    // para então podermos chamar SelectSources nele.
    // Vou colocar a estrutura base para a subscrição de Sinais do GDBus:

    printf("\n[Wayland] (Ação Requerida) Precisamos escutar os sinais Assíncronos do D-Bus para continuar o Handshake do ScreenCast...\n");
    printf("[Wayland] O código atual fará o setup do loop principal do GLib depois.\n");
    
    g_variant_unref(result);
    g_object_unref(connection);

    return 0;
}
