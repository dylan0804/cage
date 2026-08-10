CC = gcc
CFLAGS =
LIBS = -L/opt/homebrew/lib -lglfw -framework OpenGL -framework Cocoa -framework IOKit
TARGET = engine

$(TARGET): main.c
	$(CC) $(CFLAGS) main.c $(LIBS) -o $(TARGET)

run:
	make && ./engine
