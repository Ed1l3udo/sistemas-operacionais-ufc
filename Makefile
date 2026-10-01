CC ?= gcc
CFLAGS ?= -std=c17 -Wall -Wextra -Wpedantic -Werror -O2
CPPFLAGS ?= -Iinclude
EXEEXT := $(if $(filter Windows_NT,$(OS)),.exe,)
BUILD_DIR := build
SRC := src/main.c src/input.c src/scheduler.c src/output.c
OBJ := $(SRC:src/%.c=$(BUILD_DIR)/%.o)
BIN := $(BUILD_DIR)/scheduler$(EXEEXT)

.PHONY: all clean web

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/%.o: src/%.c include/scheduler.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

web: $(BIN)
	node web/server.mjs

clean:
	rm -rf $(BUILD_DIR)
