/**
 * @file    test_cfdp_checksum.c
 * @brief   Unit tests for the CFDP 32-bit modular file checksum
 *
 * Exercises src/cfdp_checksum.c against CCSDS 727.0-B-5 §4.2.2 with
 * known-vector and streamed-accumulation checks.
 * See also: docs/ccsds_cfdp.md
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#include "cunit.h"
#include "test_runners.h"

#include "cfdp.h"

static int test_checksum_known(void)
{
    const uint8_t a[] = {0x01, 0x02, 0x03, 0x04};
    ASSERT_TRUE(cfdp_checksum_compute(a, sizeof(a)) == 0x01020304U);

    const uint8_t b[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    ASSERT_TRUE(cfdp_checksum_compute(b, sizeof(b)) == 0x06020304U);

    /* Segment-by-segment accumulation must match a single-shot computation. */
    uint32_t streamed = cfdp_checksum_update(0, 0, a, 2);
    streamed = cfdp_checksum_update(streamed, 2, &a[2], 2);
    ASSERT_TRUE(streamed == cfdp_checksum_compute(a, sizeof(a)));
    return 0;
}

test_result_t test_cfdp_checksum_run_all(void)
{
    RUN_TEST(test_checksum_known);

    /* cunit.h keeps its tally in file-local statics, so these counters cover
     * only the tests run above. */
    test_result_t result = {cunit_total_tests - cunit_overall_failures, cunit_total_tests};
    return result;
}
