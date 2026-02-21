CC = gcc
PKGS = libpipewire-0.3 gio-2.0 glib-2.0
CFLAGS = -Wall -Wextra -O2 $(shell pkg-config --cflags $(PKGS))
LDFLAGS = $(shell pkg-config --libs $(PKGS))
TARGET = second_screen
SRC = src/main.c src/wayland_capture.c src/pipewire_capture.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)
