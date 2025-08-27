#include <time.h>
#include "CuTest.h"
#include "shared_data.h" // El .h con tu struct MasterConfig
#include <stdio.h>
#include "master_logic.h"

void test_default_config(CuTest *tc)
{
    int argc = 3;
    char *argv[] = {"./master", "-p", "./player", NULL};
    MasterConfig config;

    int result = parse_args(argc, argv, &config);

    CuAssertIntEquals(tc, 0, result);
    CuAssertIntEquals(tc, 10, config.height);
    CuAssertIntEquals(tc, 10, config.width);
    CuAssertIntEquals(tc, 200, config.delay);
    CuAssertIntEquals(tc, 10, config.timeout);
    CuAssertPtrEquals(tc, NULL, config.view_path);
    CuAssertStrEquals(tc, "./player", config.player_paths[0]);
    CuAssertIntEquals(tc, 1, config.player_count);
}

void test_custom_config(CuTest *tc)
{
    char *argv[] = {"./master", "-w", "25", "-h", "30", "-v", "./vista", "-p", "./p1", "./p2", NULL};
    int argc = 10;
    MasterConfig config;

    int result = parse_args(argc, argv, &config);

    CuAssertIntEquals(tc, 0, result);
    CuAssertIntEquals(tc, 25, config.width);
    CuAssertIntEquals(tc, 30, config.height);
    CuAssertIntEquals(tc, 200, config.delay);
    CuAssertIntEquals(tc, 10, config.timeout);
    CuAssertStrEquals(tc, "./vista", config.view_path);
    CuAssertStrEquals(tc, "./p1", config.player_paths[0]);
    CuAssertStrEquals(tc, "./p2", config.player_paths[1]);
    CuAssertIntEquals(tc, 2, config.player_count);
}

void test_bad_argument(CuTest *tc)
{
    char *argv[] = {"./master", "-w", "25", "-h", "30", "-v", "./vista", "-j", "./p1", "./p2", NULL};
    int argc = 10;
    MasterConfig config;

    int result = parse_args(argc, argv, &config);

    CuAssertIntEquals(tc, -1, result);
}

void test_not_enough_players(CuTest *tc) {
    char *argv[] = {"./master", "-w", "25", "-h", "30", "-v", "./vista", NULL};
    int argc = 7;
    MasterConfig config;

    int result = parse_args(argc, argv, &config);

    CuAssertIntEquals(tc, -1, result);
}

CuSuite *master_get_suite()
{
    CuSuite *suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, test_default_config);
    SUITE_ADD_TEST(suite, test_custom_config);
    SUITE_ADD_TEST(suite, test_bad_argument);
    SUITE_ADD_TEST(suite, test_not_enough_players);
    return suite;
}

int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;
    CuString *output = CuStringNew();
    CuSuite *master = master_get_suite();

    CuSuiteRun(master);
    CuSuiteSummary(master, output);
    CuSuiteDetails(master, output);
    printf("%s\n", output->buffer);
    return 0;
}
