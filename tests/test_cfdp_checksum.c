/**
 * @file    test_cfdp_checksum.c
 * @brief   Unit tests for the CFDP 32-bit modular file checksum
 *
 * Exercises src/cfdp_checksum.c against CCSDS 727.0-B-5 §4.2.2 with
 * known-vector and streamed-accumulation checks.
 * See also: docs/727x0b5e1.pdf
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#include "cfdp.h"
#include "cunit.h"
#include "test_runners.h"

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

static int test_checksum_no_data(void)
{
    const uint8_t data[] = {0xFF};

    /* Both guard conditions must leave the running checksum untouched. */
    ASSERT_TRUE(cfdp_checksum_update(0x11223344U, 0, NULL, 4) == 0x11223344U);
    ASSERT_TRUE(cfdp_checksum_update(0x11223344U, 0, data, 0) == 0x11223344U);
    ASSERT_TRUE(cfdp_checksum_compute(NULL, 8) == 0);
    return 0;
}

static int test_checksum_offset_lanes(void)
{
    const uint8_t data[] = {0x01, 0x02};

    /* An octet's lane follows its absolute file offset, not its index in the
     * segment, so the same two octets weigh differently at offset 1 and 4. */
    ASSERT_TRUE(cfdp_checksum_update(0, 1, data, sizeof(data)) == 0x00010200U);
    ASSERT_TRUE(cfdp_checksum_update(0, 4, data, sizeof(data)) == 0x01020000U);
    return 0;
}

test_result_t test_cfdp_checksum_run_all(void)
{
    RUN_TEST(test_checksum_known);
    RUN_TEST(test_checksum_no_data);
    RUN_TEST(test_checksum_offset_lanes);

    /* cunit.h keeps its tally in file-local statics, so these counters cover
     * only the tests run above. */
    test_result_t result = {cunit_total_tests - cunit_overall_failures, cunit_total_tests};
    return result;
}
