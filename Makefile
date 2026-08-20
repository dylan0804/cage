CC = gcc
CFLAGS = $(shell pkg-config --cflags freetype2)
LDFLAGS = $(shell pkg-config --libs freetype2)
LIBS = -L/opt/homebrew/lib -lglfw -framework OpenGL -framework Cocoa -framework IOKit
TARGET = engine

$(TARGET): main.c
	$(CC) $(CFLAGS) $(LDFLAGS) main.c $(LIBS) -o $(TARGET)

run:
	make && ./engine
