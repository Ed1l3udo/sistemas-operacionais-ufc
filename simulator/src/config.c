#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define CONFIG_LINE_CAPACITY 1024

static char *skip_whitespace(char *text)
{
    while (*text != '\0' && isspace((unsigned char)*text)) {
        ++text;
    }

    return text;
}

static bool line_is_blank(const char *line)
{
    while (*line != '\0') {
        if (!isspace((unsigned char)*line)) {
            return false;
        }
        ++line;
    }

    return true;
}

static void discard_overlong_line(FILE *input)
{
    int character;

    do {
        character = fgetc(input);
    } while (character != '\n' && character != EOF);
}

static bool parse_config_value(char **cursor, int *value, size_t line_number,
                               SchedulerError *error)
{
    char *end = NULL;
    long parsed_value;

    *cursor = skip_whitespace(*cursor);
    errno = 0;
    parsed_value = strtol(*cursor, &end, 10);
    if (end == *cursor) {
        error_set(error, EXIT_CODE_CONFIG,
                  "configuration line %zu: expected an integer value", line_number);
        return false;
    }
    if (errno == ERANGE || parsed_value < INT_MIN || parsed_value > INT_MAX) {
        error_set(error, EXIT_CODE_CONFIG,
                  "configuration line %zu: value is outside the int range",
                  line_number);
        return false;
    }

    *cursor = skip_whitespace(end);
    if (**cursor != '\0') {
        error_set(error, EXIT_CODE_CONFIG,
                  "configuration line %zu: unexpected text after value", line_number);
        return false;
    }

    *value = (int)parsed_value;
    return true;
}

bool config_parse_stream(FILE *input, Config *config, SchedulerError *error)
{
    char line[CONFIG_LINE_CAPACITY];
    Config parsed_config = {0, 0};
    bool quantum_seen = false;
    bool aging_seen = false;
    size_t line_number = 0;

    if (input == NULL || config == NULL) {
        error_set(error, EXIT_CODE_INTERNAL, "invalid configuration input stream");
        return false;
    }

    while (fgets(line, sizeof(line), input) != NULL) {
        char *cursor;
        char *key_start;
        size_t key_length;
        int value;

        ++line_number;
        if (strchr(line, '\n') == NULL && !feof(input)) {
            discard_overlong_line(input);
            error_set(error, EXIT_CODE_CONFIG,
                      "configuration line %zu exceeds %d characters", line_number,
                      CONFIG_LINE_CAPACITY - 1);
            return false;
        }
        if (line_is_blank(line)) {
            continue;
        }

        cursor = skip_whitespace(line);
        key_start = cursor;
        while (*cursor != '\0' && *cursor != ':' &&
               !isspace((unsigned char)*cursor)) {
            ++cursor;
        }
        key_length = (size_t)(cursor - key_start);
        if (key_length == 0) {
            error_set(error, EXIT_CODE_CONFIG,
                      "configuration line %zu: expected a field name", line_number);
            return false;
        }
        cursor = skip_whitespace(cursor);
        if (*cursor != ':') {
            error_set(error, EXIT_CODE_CONFIG,
                      "configuration line %zu: expected ':' after field name",
                      line_number);
            return false;
        }
        ++cursor;

        if (key_length == strlen("quantum") &&
            strncmp(key_start, "quantum", key_length) == 0) {
            if (quantum_seen) {
                error_set(error, EXIT_CODE_CONFIG,
                          "configuration line %zu: duplicate quantum field",
                          line_number);
                return false;
            }
            if (!parse_config_value(&cursor, &value, line_number, error)) {
                return false;
            }
            if (value <= 0) {
                error_set(error, EXIT_CODE_CONFIG,
                          "configuration line %zu: quantum must be greater than zero",
                          line_number);
                return false;
            }
            parsed_config.quantum = value;
            quantum_seen = true;
        } else if (key_length == strlen("aging") &&
                   strncmp(key_start, "aging", key_length) == 0) {
            if (aging_seen) {
                error_set(error, EXIT_CODE_CONFIG,
                          "configuration line %zu: duplicate aging field",
                          line_number);
                return false;
            }
            if (!parse_config_value(&cursor, &value, line_number, error)) {
                return false;
            }
            if (value < 0) {
                error_set(error, EXIT_CODE_CONFIG,
                          "configuration line %zu: aging must be non-negative",
                          line_number);
                return false;
            }
            parsed_config.aging = value;
            aging_seen = true;
        } else {
            error_set(error, EXIT_CODE_CONFIG,
                      "configuration line %zu: unknown field", line_number);
            return false;
        }
    }

    if (ferror(input)) {
        error_set(error, EXIT_CODE_CONFIG, "unable to read configuration");
        return false;
    }
    if (!quantum_seen || !aging_seen) {
        error_set(error, EXIT_CODE_CONFIG,
                  "configuration is missing required field%s%s",
                  quantum_seen ? "" : " quantum",
                  aging_seen ? "" : " aging");
        return false;
    }

    *config = parsed_config;
    return true;
}

bool config_load_file(const char *path, Config *config, SchedulerError *error)
{
    FILE *input;
    bool parsed;
    int open_error;

    if (path == NULL || config == NULL) {
        error_set(error, EXIT_CODE_INTERNAL, "invalid configuration file request");
        return false;
    }

    input = fopen(path, "r");
    if (input == NULL) {
        open_error = errno;
        error_set(error, EXIT_CODE_CONFIG,
                  "unable to open configuration file '%s': %s", path,
                  strerror(open_error));
        return false;
    }

    parsed = config_parse_stream(input, config, error);
    if (fclose(input) != 0 && parsed) {
        error_set(error, EXIT_CODE_CONFIG,
                  "unable to close configuration file '%s'", path);
        return false;
    }

    return parsed;
}
