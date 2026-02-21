#include "mjpeg_stream.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <turbojpeg.h>

FrameState global_frame = {0};

void init_mjpeg_stream() {
    pthread_mutex_init(&global_frame.mutex, NULL);
    pthread_cond_init(&global_frame.cond, NULL);
    global_frame.jpeg_buffer = NULL;
    global_frame.jpeg_size = 0;
}

void update_latest_frame(const uint8_t *bgra_pixels, int width, int height, int stride) {
    tjhandle _jpegCompressor = tjInitCompress();
    if (!_jpegCompressor) {
        fprintf(stderr, "TurboJPEG Init Error: %s\n", tjGetErrorStr());
        return;
    }

    uint8_t *compressed_image = NULL;
    unsigned long compressed_size = 0;

    int jpeg_quality = 75;

    // Convert BGRA (PipeWire format) to JPEG
    // Pipewire typically uses BGRx or BGRA, which maps to TJPF_BGRA in turbojpeg
    if (tjCompress2(_jpegCompressor, bgra_pixels, width, stride, height, TJPF_BGRA,
                    &compressed_image, &compressed_size, TJSAMP_420, jpeg_quality,
                    TJFLAG_FASTDCT) < 0) {
        fprintf(stderr, "TurboJPEG Compress Error: %s\n", tjGetErrorStr());
        tjDestroy(_jpegCompressor);
        return;
    }

    // Lock the mutex to safely update the shared global frame
    pthread_mutex_lock(&global_frame.mutex);

    // Free previously allocated jpeg buffer if it exists
    if (global_frame.jpeg_buffer) {
        tjFree(global_frame.jpeg_buffer);
    }

    global_frame.jpeg_buffer = compressed_image;
    global_frame.jpeg_size = compressed_size;

    // Wake up all HTTP clients waiting for a new frame
    pthread_cond_broadcast(&global_frame.cond);
    
    pthread_mutex_unlock(&global_frame.mutex);

    tjDestroy(_jpegCompressor);
}

void handle_mjpeg_client(int client_socket) {
    const char *header = 
        "HTTP/1.1 200 OK\r\n"
        "Cache-Control: no-cache, private\r\n"
        "Pragma: no-cache\r\n"
        "Content-Type: multipart/x-mixed-replace; boundary=--myboundary\r\n"
        "Connection: close\r\n\r\n";
    
    if (send(client_socket, header, strlen(header), 0) < 0) {
        return;
    }

    while (1) {
        pthread_mutex_lock(&global_frame.mutex);
        
        // Wait until the PipeWire thread wakes us up with a new frame
        pthread_cond_wait(&global_frame.cond, &global_frame.mutex);

        if (!global_frame.jpeg_buffer || global_frame.jpeg_size == 0) {
            pthread_mutex_unlock(&global_frame.mutex);
            continue;
        }

        char frame_header[256];
        int header_len = snprintf(frame_header, sizeof(frame_header),
                 "--myboundary\r\n"
                 "Content-Type: image/jpeg\r\n"
                 "Content-Length: %lu\r\n\r\n", 
                 global_frame.jpeg_size);

        // Send Boundary Header
        if (send(client_socket, frame_header, header_len, 0) < 0) {
            pthread_mutex_unlock(&global_frame.mutex);
            break; // Client disconnected
        }

        // Send JPEG Bytes
        if (send(client_socket, global_frame.jpeg_buffer, global_frame.jpeg_size, 0) < 0) {
            pthread_mutex_unlock(&global_frame.mutex);
            break; // Client disconnected
        }
        
        // Send trailing CRLF for the multipart spec
        if (send(client_socket, "\r\n", 2, 0) < 0) {
            pthread_mutex_unlock(&global_frame.mutex);
            break;
        }

        pthread_mutex_unlock(&global_frame.mutex);
    }
}
