#ifndef WAYLAND_CAPTURE_H
#define WAYLAND_CAPTURE_H

// Inicializa o processo de captura do Wayland (pedindo permissão via Portal DBus).
// Retorna 0 em caso de sucesso no disparo do DBus e -1 para erro.
int init_wayland_capture();

#endif // WAYLAND_CAPTURE_H
