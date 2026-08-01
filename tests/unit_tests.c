/**
 * @file    unit_tests.c
 * @brief   Unit test entry point: runs each module's suite and reports the tally
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#include "test_runners.h"

#include <stdio.h>

/** @brief Print one module's tally in the summary layout. */
#define REPORT(label, r) printf("  %-18s Passed %d/%d\n\n", label ":", (r).passed, (r).total)

int main(void)
{
    test_result_t r;
    int total_passed = 0;
    int total_tests = 0;

    r = test_cfdp_pdu_run_all();
    REPORT("cfdp_pdu", r);
    total_passed += r.passed;
    total_tests += r.total;

    r = test_cfdp_directive_run_all();
    REPORT("cfdp_directive", r);
    total_passed += r.passed;
    total_tests += r.total;

    r = test_cfdp_checksum_run_all();
    REPORT("cfdp_checksum", r);
    total_passed += r.passed;
    total_tests += r.total;

    printf("  ------------------------------\n");
    printf("  %-18s Passed %d/%d\n", "All UT:", total_passed, total_tests);

    return (total_passed == total_tests) ? 0 : 1;
}
