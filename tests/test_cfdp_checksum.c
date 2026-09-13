/**
 * @file    test_cfdp_checksum.c
 * @brief   Unit tests for the CFDP 32-bit modular file checksum
 *
 * Exercises src/cfdp_checksum.c against CCSDS 727.0-B-5 §4.2 with
 * known-vector (including the annex F example) and streamed-accumulation
 * checks of the modular and null checksums.
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

static int test_checksum_type_supported(void)
{
    /* §4.2.2.3-4.2.2.4: types 0 and 15 are mandatory; 1-14 are optional and
     * not implemented here. */
    for (uint8_t type = 0; type <= 15; type++)
    {
        bool expected = (type == CFDP_CHECKSUM_MODULAR) || (type == CFDP_CHECKSUM_NULL);
        ASSERT_TRUE(cfdp_checksum_type_supported((cfdp_checksum_type_t)type) == expected);
    }
    return 0;
}

static int test_checksum_null_is_zero(void)
{
    const uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x01};

    /* §4.2.2.4: the null checksum is simply zero, whatever the data, offset
     * or running value. */
    uint32_t checksum = 0x12345678U;
    ASSERT_TRUE(cfdp_checksum_update_by_type(CFDP_CHECKSUM_NULL, &checksum, 7, data, sizeof(data)));
    ASSERT_TRUE(checksum == 0U);

    checksum = 0xFFFFFFFFU;
    ASSERT_TRUE(cfdp_checksum_update_by_type(CFDP_CHECKSUM_NULL, &checksum, 0, NULL, 0));
    ASSERT_TRUE(checksum == 0U);
    return 0;
}

static int test_checksum_modular_by_type_annex_f(void)
{
    /* Annex F: a 15-octet file 00..0e sent in 6-octet segments sums to
     * 00010203 + 04050607 + 08090a0b + 0c0d0e00. */
    uint8_t file[15];
    for (uint8_t i = 0; i < sizeof(file); i++)
    {
        file[i] = i;
    }
    const uint32_t expected = 0x181C2015U;

    uint32_t in_order = 0;
    ASSERT_TRUE(cfdp_checksum_update_by_type(CFDP_CHECKSUM_MODULAR, &in_order, 0, &file[0], 6));
    ASSERT_TRUE(cfdp_checksum_update_by_type(CFDP_CHECKSUM_MODULAR, &in_order, 6, &file[6], 6));
    ASSERT_TRUE(cfdp_checksum_update_by_type(CFDP_CHECKSUM_MODULAR, &in_order, 12, &file[12], 3));
    ASSERT_TRUE(in_order == expected);

    /* Annex F's out-of-order arrival: offsets 0, 12, then 6. */
    uint32_t out_of_order = 0;
    ASSERT_TRUE(cfdp_checksum_update_by_type(CFDP_CHECKSUM_MODULAR, &out_of_order, 0, &file[0], 6));
    ASSERT_TRUE(
        cfdp_checksum_update_by_type(CFDP_CHECKSUM_MODULAR, &out_of_order, 12, &file[12], 3));
    ASSERT_TRUE(cfdp_checksum_update_by_type(CFDP_CHECKSUM_MODULAR, &out_of_order, 6, &file[6], 6));
    ASSERT_TRUE(out_of_order == expected);

    ASSERT_TRUE(cfdp_checksum_compute(file, sizeof(file)) == expected);
    return 0;
}

static int test_checksum_update_by_type_invalid_args(void)
{
    const uint8_t data[] = {0x01, 0x02, 0x03, 0x04};

    /* An unsupported type fails and leaves the running checksum untouched, so
     * the caller can raise the fault and apply the §4.2.2.8 fallback. */
    uint32_t checksum = 0xCAFEF00DU;
    ASSERT_TRUE(
        !cfdp_checksum_update_by_type((cfdp_checksum_type_t)1, &checksum, 0, data, sizeof(data)));
    ASSERT_TRUE(
        !cfdp_checksum_update_by_type((cfdp_checksum_type_t)14, &checksum, 0, data, sizeof(data)));
    ASSERT_TRUE(checksum == 0xCAFEF00DU);

    ASSERT_TRUE(!cfdp_checksum_update_by_type(CFDP_CHECKSUM_MODULAR, NULL, 0, data, sizeof(data)));
    ASSERT_TRUE(!cfdp_checksum_update_by_type(CFDP_CHECKSUM_NULL, NULL, 0, data, sizeof(data)));
    return 0;
}

test_result_t test_cfdp_checksum_run_all(void)
{
    RUN_TEST(test_checksum_known);
    RUN_TEST(test_checksum_no_data);
    RUN_TEST(test_checksum_offset_lanes);
    RUN_TEST(test_checksum_type_supported);
    RUN_TEST(test_checksum_null_is_zero);
    RUN_TEST(test_checksum_modular_by_type_annex_f);
    RUN_TEST(test_checksum_update_by_type_invalid_args);

    /* cunit.h keeps its tally in file-local statics, so these counters cover
     * only the tests run above. */
    test_result_t result = {cunit_total_tests - cunit_overall_failures, cunit_total_tests};
    return result;
}
