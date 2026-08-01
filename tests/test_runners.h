/**
 * @file    test_runners.h
 * @brief   Per-module unit test runner declarations
 *
 * One runner per source file under src/, each reporting its own tally to the
 * unit_tests.c entry point.
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#ifndef TEST_RUNNERS_H
#define TEST_RUNNERS_H

/** @brief Outcome tally of a module's test suite. */
typedef struct
{
    int passed; /**< Number of tests that passed. */
    int total;  /**< Number of tests run. */
} test_result_t;

/* Per-module test runners. Each runs all of its module's tests and returns the
 * passed/total tally. */
test_result_t test_cfdp_pdu_run_all(void);
test_result_t test_cfdp_directive_run_all(void);
test_result_t test_cfdp_checksum_run_all(void);

#endif /* TEST_RUNNERS_H */
