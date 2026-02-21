#ifndef PIPEWIRE_CAPTURE_H
#define PIPEWIRE_CAPTURE_H

#include <stdint.h>

// Initialize PipeWire connection for the specified remote node stream.
void init_pipewire_capture(uint32_t node_id);

#endif // PIPEWIRE_CAPTURE_H
