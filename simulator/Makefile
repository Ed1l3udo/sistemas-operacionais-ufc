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
CORE_SOURCES = $(filter-out $(SOURCE_DIR)/main.c,$(SOURCES))
CORE_OBJECTS = $(patsubst $(SOURCE_DIR)/%.c,$(BUILD_DIR)/%.o,$(CORE_SOURCES))
TEST_SOURCES = $(wildcard tests/*.c)
TEST_OBJECTS = $(patsubst tests/%.c,$(BUILD_DIR)/tests_%.o,$(TEST_SOURCES))
TEST_TARGET = scheduler_tests$(EXECUTABLE_EXTENSION)

.PHONY: all test sanitize clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(BASE_CFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/tests_%.o: tests/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(BASE_CFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

$(TEST_TARGET): $(CORE_OBJECTS) $(TEST_OBJECTS)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

test: $(TEST_TARGET)
	./$(TEST_TARGET)

sanitize:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CFLAGS) -fsanitize=address,undefined -g" \
		LDFLAGS="$(LDFLAGS) -fsanitize=address,undefined" test

clean:
	rm -rf $(BUILD_DIR) scheduler scheduler.exe scheduler_tests scheduler_tests.exe
