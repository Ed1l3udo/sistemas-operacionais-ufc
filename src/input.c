#define _POSIX_C_SOURCE 200809L

#include "scheduler.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static void set_error(char *error, size_t size, const char *format, ...) {
    va_list args;
    if (size == 0) return;
    va_start(args, format);
    vsnprintf(error, size, format, args);
    va_end(args);
}

static char *trim(char *text) {
    char *end;
    while (isspace((unsigned char)*text)) text++;
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) end--;
    *end = '\0';
    return text;
}

static bool parse_nonnegative(const char *text, int *value) {
    char *end = NULL;
    long parsed;
    errno = 0;
    parsed = strtol(text, &end, 10);
    while (end && isspace((unsigned char)*end)) end++;
    if (errno || end == text || (end && *end != '\0') || parsed < 0 || parsed > INT_MAX) return false;
    *value = (int)parsed;
    return true;
}

bool read_config(const char *path, SchedulerConfig *config, char *error, size_t error_size) {
    FILE *file = fopen(path, "r");
    char *line = NULL;
    size_t capacity = 0;
    ssize_t length;
    unsigned line_number = 0;
    bool has_quantum = false, has_aging = false;

    if (!file) {
        set_error(error, error_size, "não foi possível abrir a configuração '%s': %s", path, strerror(errno));
        return false;
    }
    while ((length = getline(&line, &capacity, file)) >= 0) {
        char *content, *colon, *key, *value;
        int parsed;
        (void)length;
        line_number++;
        content = trim(line);
        if (*content == '\0' || *content == '#') continue;
        colon = strchr(content, ':');
        if (!colon) {
            set_error(error, error_size, "configuração inválida na linha %u: esperado chave:valor", line_number);
            goto fail;
        }
        *colon = '\0';
        key = trim(content);
        value = trim(colon + 1);
        if (!parse_nonnegative(value, &parsed)) {
            set_error(error, error_size, "valor inválido para '%s' na linha %u", key, line_number);
            goto fail;
        }
        if (strcmp(key, "quantum") == 0) {
            if (has_quantum) {
                set_error(error, error_size, "quantum duplicado na configuração");
                goto fail;
            }
            config->quantum = parsed;
            has_quantum = true;
        } else if (strcmp(key, "aging") == 0) {
            if (has_aging) {
                set_error(error, error_size, "aging duplicado na configuração");
                goto fail;
            }
            config->aging = parsed;
            has_aging = true;
        } else {
            set_error(error, error_size, "chave desconhecida '%s' na linha %u", key, line_number);
            goto fail;
        }
    }
    free(line);
    fclose(file);
    if (!has_quantum || !has_aging) {
        set_error(error, error_size, "a configuração deve informar quantum e aging");
        return false;
    }
    if (config->quantum <= 0) {
        set_error(error, error_size, "quantum deve ser positivo");
        return false;
    }
    return true;

fail:
    free(line);
    fclose(file);
    return false;
}

bool read_processes(FILE *stream, ProcessSpec **processes, size_t *count,
                    char *error, size_t error_size) {
    ProcessSpec *items = NULL;
    size_t used = 0, allocated = 0, line_capacity = 0;
    char *line = NULL;
    ssize_t length;
    unsigned line_number = 0;

    while ((length = getline(&line, &line_capacity, stream)) >= 0) {
        char extra;
        int arrival, burst, priority;
        char *content;
        (void)length;
        line_number++;
        content = trim(line);
        if (*content == '\0' || *content == '#') continue;
        if (sscanf(content, "%d %d %d %c", &arrival, &burst, &priority, &extra) != 3) {
            set_error(error, error_size, "processo inválido na linha %u: use chegada duração prioridade", line_number);
            goto fail;
        }
        if (arrival < 0 || burst <= 0 || priority <= 0) {
            set_error(error, error_size,
                      "processo inválido na linha %u: chegada >= 0, duração > 0 e prioridade > 0",
                      line_number);
            goto fail;
        }
        if (used == SCHEDULER_MAX_PROCESSES) {
            set_error(error, error_size, "limite de %d processos excedido", SCHEDULER_MAX_PROCESSES);
            goto fail;
        }
        if (used == allocated) {
            size_t next = allocated ? allocated * 2 : 16;
            ProcessSpec *grown = realloc(items, next * sizeof(*items));
            if (!grown) {
                set_error(error, error_size, "memória insuficiente para ler os processos");
                goto fail;
            }
            items = grown;
            allocated = next;
        }
        items[used] = (ProcessSpec){(int)used + 1, arrival, burst, priority};
        used++;
    }
    free(line);
    if (ferror(stream)) {
        set_error(error, error_size, "erro durante a leitura da entrada");
        free(items);
        return false;
    }
    if (used == 0) {
        set_error(error, error_size, "nenhum processo foi informado");
        free(items);
        return false;
    }
    *processes = items;
    *count = used;
    return true;

fail:
    free(line);
    free(items);
    return false;
}
