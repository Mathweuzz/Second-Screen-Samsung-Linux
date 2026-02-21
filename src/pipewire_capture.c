#include "pipewire_capture.h"
#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>

struct stream_data {
    struct pw_main_loop *loop;
    struct pw_stream *stream;
};

static void on_process(void *userdata) {
    struct stream_data *data = userdata;
    struct pw_buffer *b;
    struct spa_buffer *buf;

    if ((b = pw_stream_dequeue_buffer(data->stream)) == NULL) {
        return;
    }

    buf = b->buffer;
    if (buf->datas[0].data != NULL) {
        printf("Frame grabbed! Size: %d bytes\n", buf->datas[0].chunk->size);
    }

    pw_stream_queue_buffer(data->stream, b);
}

static const struct pw_stream_events stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .process = on_process,
};

static void *pw_loop_thread(void *arg) {
    uint32_t node_id = (uint32_t)(uintptr_t)arg;
    struct stream_data data = {0};

    pw_init(NULL, NULL);
    data.loop = pw_main_loop_new(NULL);
    
    struct pw_properties *props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Video",
        PW_KEY_MEDIA_CATEGORY, "Capture",
        PW_KEY_MEDIA_ROLE, "Screen",
        NULL);

    data.stream = pw_stream_new_simple(pw_main_loop_get_loop(data.loop), "second_screen_video", props, &stream_events, &data);

    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    const struct spa_pod *params[1];
    params[0] = spa_pod_builder_add_object(&b,
        SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
        SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
        SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
        SPA_FORMAT_VIDEO_format, SPA_POD_CHOICE_ENUM_Id(4, SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_RGBx, SPA_VIDEO_FORMAT_RGBA, SPA_VIDEO_FORMAT_BGRA)
    );

    pw_stream_connect(data.stream,
                      PW_DIRECTION_INPUT,
                      node_id,
                      PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS,
                      params, 1);

    printf("Connecting to PipeWire Node %u...\n", node_id);
    pw_main_loop_run(data.loop);

    pw_stream_destroy(data.stream);
    pw_main_loop_destroy(data.loop);
    pw_deinit();
    
    return NULL;
}

void init_pipewire_capture(uint32_t node_id) {
    printf("Starting PipeWire thread...\n");
    GThread *thread = g_thread_new("pw_capture", pw_loop_thread, (void *)(uintptr_t)node_id);
    if (!thread) return;
    g_thread_unref(thread);
}
