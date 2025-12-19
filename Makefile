# Solar Array Designer - Makefile

# Compiler
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2

# Detect OS
UNAME_S := $(shell uname -s)

# Platform-specific settings
ifeq ($(UNAME_S),Darwin)
    # macOS
    INCLUDES = -I/opt/homebrew/include -I./src
    LDFLAGS = -L/opt/homebrew/lib
    LIBS = -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
else ifeq ($(UNAME_S),Linux)
    # Linux
    INCLUDES = -I./src
    LDFLAGS =
    LIBS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
else
    # Windows (MinGW)
    INCLUDES = -I./src
    LDFLAGS =
    LIBS = -lraylib -lopengl32 -lgdi32 -lwinmm
endif

# Source files
SRCDIR = src
SOURCES = $(SRCDIR)/main.c $(SRCDIR)/app.c $(SRCDIR)/gui.c
OBJECTS = $(SOURCES:.c=.o)

# Output
TARGET = shellpower

# Default target
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS) $(LIBS)
	@echo "Build complete: $(TARGET)"

# Compile source files
$(SRCDIR)/%.o: $(SRCDIR)/%.c $(SRCDIR)/app.h $(SRCDIR)/raygui.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TARGET)
	@echo "Cleaned build artifacts"

# Run the application
run: $(TARGET)
	./$(TARGET)

# Install dependencies (macOS)
deps-mac:
	brew install raylib

# Install dependencies (Linux/Debian)
deps-linux:
	sudo apt-get update
	sudo apt-get install -y libraylib-dev

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: clean all

# Release build
release: CFLAGS += -O3 -DNDEBUG
release: clean all

.PHONY: all clean run deps-mac deps-linux debug release
