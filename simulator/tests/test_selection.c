#include "test_common.h"

#include "selection.h"

static bool make_simulation(Simulation *simulation, SchedulerError *error)
{
    ProcessList source;
    bool result;

    process_list_init(&source);
    if (!process_list_append(&source, 0, 5, 1, error) ||
        !process_list_append(&source, 0, 2, 1, error) ||
        !process_list_append(&source, 0, 2, 1, error)) {
        process_list_destroy(&source);
        return false;
    }
    result = simulation_init(simulation, &source, error) &&
             simulation_admit_current(simulation, NULL, error);
    process_list_destroy(&source);
    return result;
}

static void test_determined_choices(void)
{
    Simulation simulation;
    IndexList candidates;
    SchedulerError error;
    Prng prng;
    Prng before;
    size_t selected;

    error_clear(&error);
    TEST_ASSERT(make_simulation(&simulation, &error));
    index_list_init(&candidates);
    TEST_ASSERT(index_list_append(&candidates, 0, &error));
    prng_init(&prng, 11);
    before = prng;
    TEST_ASSERT(selection_break_tie(&simulation, &candidates,
                                    SIMULATION_NO_PROCESS, &prng, &selected,
                                    &error));
    TEST_ASSERT(selected == 0 && prng.state == before.state);
    TEST_ASSERT(index_list_append(&candidates, 1, &error));
    before = prng;
    TEST_ASSERT(selection_break_tie(&simulation, &candidates,
                                    SIMULATION_NO_PROCESS, &prng, &selected,
                                    &error));
    TEST_ASSERT(selected == 1 && prng.state == before.state);
    TEST_ASSERT(selection_break_tie(&simulation, &candidates, 0, &prng,
                                    &selected, &error));
    TEST_ASSERT(selected == 0 && prng.state == before.state);
    index_list_destroy(&candidates);
    simulation_destroy(&simulation);
}

static void test_random_and_invalid_choices(void)
{
    Simulation simulation;
    IndexList candidates;
    SchedulerError error;
    Prng first;
    Prng second;
    size_t first_selected;
    size_t second_selected;

    error_clear(&error);
    TEST_ASSERT(make_simulation(&simulation, &error));
    index_list_init(&candidates);
    TEST_ASSERT(index_list_append(&candidates, 1, &error));
    TEST_ASSERT(index_list_append(&candidates, 2, &error));
    prng_init(&first, 99);
    prng_init(&second, 99);
    TEST_ASSERT(selection_break_tie(&simulation, &candidates,
                                    SIMULATION_NO_PROCESS, &first,
                                    &first_selected, &error));
    TEST_ASSERT(selection_break_tie(&simulation, &candidates,
                                    SIMULATION_NO_PROCESS, &second,
                                    &second_selected, &error));
    TEST_ASSERT(first_selected == second_selected);
    index_list_clear(&candidates);
    TEST_ASSERT(!selection_break_tie(&simulation, &candidates,
                                     SIMULATION_NO_PROCESS, &first,
                                     &first_selected, &error));
    TEST_ASSERT(index_list_append(&candidates, 99, &error));
    TEST_ASSERT(!selection_break_tie(&simulation, &candidates,
                                     SIMULATION_NO_PROCESS, &first,
                                     &first_selected, &error));
    index_list_destroy(&candidates);
    simulation_destroy(&simulation);
}

void run_selection_tests(void)
{
    test_determined_choices();
    test_random_and_invalid_choices();
}
