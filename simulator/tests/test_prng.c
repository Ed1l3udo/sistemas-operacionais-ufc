#include "test_common.h"

#include "prng.h"

static void test_reproducibility_and_independence(void)
{
    Prng first;
    Prng second;
    Prng different;
    size_t index;
    bool observed_difference = false;

    prng_init(&first, 0);
    prng_init(&second, 0);
    prng_init(&different, 1);
    for (index = 0; index < 16; ++index) {
        uint64_t first_value = prng_next(&first);

        TEST_ASSERT(first_value == prng_next(&second));
        if (first_value != prng_next(&different)) {
            observed_difference = true;
        }
    }
    TEST_ASSERT(observed_difference);
}

static void test_bounded_values(void)
{
    Prng prng;
    size_t value;
    size_t index;

    prng_init(&prng, 42);
    for (index = 0; index < 100; ++index) {
        TEST_ASSERT(prng_next_bounded(&prng, 7, &value));
        TEST_ASSERT(value < 7);
    }
    TEST_ASSERT(prng_next_bounded(&prng, 1, &value) && value == 0);
    TEST_ASSERT(!prng_next_bounded(&prng, 0, &value));
    TEST_ASSERT(!prng_next_bounded(NULL, 1, &value));
}

void run_prng_tests(void)
{
    test_reproducibility_and_independence();
    test_bounded_values();
}
