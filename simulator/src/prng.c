#include "prng.h"

void prng_init(Prng *prng, uint64_t seed)
{
    if (prng != NULL) {
        prng->state = seed;
    }
}

uint64_t prng_next(Prng *prng)
{
    uint64_t value;

    if (prng == NULL) {
        return 0;
    }
    prng->state += UINT64_C(0x9E3779B97F4A7C15);
    value = prng->state;
    value = (value ^ (value >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    value = (value ^ (value >> 27)) * UINT64_C(0x94D049BB133111EB);
    return value ^ (value >> 31);
}

bool prng_next_bounded(Prng *prng, size_t bound, size_t *value)
{
    uint64_t bound_value;
    uint64_t threshold;
    uint64_t random_value;

    if (prng == NULL || value == NULL || bound == 0) {
        return false;
    }
    bound_value = (uint64_t)bound;
    if ((size_t)bound_value != bound) {
        return false;
    }
    threshold = (UINT64_C(0) - bound_value) % bound_value;
    do {
        random_value = prng_next(prng);
    } while (random_value < threshold);
    *value = (size_t)(random_value % bound_value);
    return true;
}
