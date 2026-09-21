CC = gcc

CPPFLAGS = -Iinclude
BASE_CFLAGS = -std=c17 -Wall -Wextra -Wpedantic
CFLAGS =
LDFLAGS =
LDLIBS =

BUILD_DIR = build
SOURCE_DIR = src

ifeq ($(OS),Windows_NT)
EXECUTABLE_EXTENSION = .exe
endif

TARGET = scheduler$(EXECUTABLE_EXTENSION)
SOURCES = $(wildcard $(SOURCE_DIR)/*.c)
OBJECTS = $(patsubst $(SOURCE_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(BASE_CFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR) scheduler scheduler.exe

