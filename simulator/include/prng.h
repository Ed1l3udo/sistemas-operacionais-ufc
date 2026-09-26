#ifndef SCHEDULER_PRNG_H
#define SCHEDULER_PRNG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint64_t state;
} Prng;

void prng_init(Prng *prng, uint64_t seed);
uint64_t prng_next(Prng *prng);
bool prng_next_bounded(Prng *prng, size_t bound, size_t *value);

#endif
