#ifndef MJPEG_STREAM_H
#define MJPEG_STREAM_H

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>

// Global state holding the latest compressed JPEG frame
typedef struct {
    uint8_t *jpeg_buffer;
    unsigned long jpeg_size;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} FrameState;

extern FrameState global_frame;

// Initialize the MJPEG state (mutues/cond vars)
void init_mjpeg_stream();

// Compress a raw BGRA frame from PipeWire and make it available for the HTTP clients
void update_latest_frame(const uint8_t *bgra_pixels, int width, int height, int stride);

// Logic to keep a TCP connection alive sending the boundary multipart continuously
void handle_mjpeg_client(int client_socket);

#endif // MJPEG_STREAM_H
