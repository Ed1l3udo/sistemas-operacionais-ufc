CC ?= gcc
CFLAGS ?= -std=c17 -Wall -Wextra -Wpedantic -Werror -O2
CPPFLAGS ?= -Iinclude
EXEEXT := $(if $(filter Windows_NT,$(OS)),.exe,)
BUILD_DIR := build
SRC := src/main.c src/input.c src/scheduler.c src/output.c
OBJ := $(SRC:src/%.c=$(BUILD_DIR)/%.o)
BIN := $(BUILD_DIR)/scheduler$(EXEEXT)
TEST_BIN := $(BUILD_DIR)/test_scheduler$(EXEEXT)

.PHONY: all clean test sanitize web

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/%.o: src/%.c include/scheduler.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

$(TEST_BIN): tests/test_scheduler.c src/input.c src/scheduler.c src/output.c include/scheduler.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_scheduler.c src/input.c src/scheduler.c src/output.c -o $@

test: $(BIN) $(TEST_BIN)
	./$(TEST_BIN)
	EXEEXT=$(EXEEXT) sh tests/test_cli.sh
	@if [ -f tests/test_api.mjs ] && command -v node >/dev/null 2>&1; then node --test tests/test_api.mjs; else echo "Teste da API ainda indisponível ou Node.js ausente"; fi

sanitize: clean
	$(MAKE) CFLAGS="-std=c17 -Wall -Wextra -Wpedantic -Werror -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer" $(TEST_BIN)
	ASAN_OPTIONS=detect_leaks=0 ./$(TEST_BIN)
	$(MAKE) clean
	$(MAKE)

web: $(BIN)
	node web/server.mjs

clean:
	rm -rf $(BUILD_DIR)
