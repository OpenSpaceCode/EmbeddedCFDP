/**
 * @file    test_cfdp_crc.c
 * @brief   Unit tests for the CFDP 16-bit PDU CRC
 *
 * Exercises src/cfdp_crc.c against CCSDS 727.0-B-5 §4.1.3 with the CCSDS
 * 232.0-B-3 §4.2.1.3 algorithm's catalogue check values, streamed
 * accumulation and the zero-residue decoding property.
 * See also: docs/727x0b5e1.pdf
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#include "cfdp.h"
#include "cunit.h"
#include "test_runners.h"

static int test_crc_check_values(void)
{
    /* CRC-16/CCITT-FALSE catalogue check value, and a one-octet vector. */
    const uint8_t check[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    ASSERT_EQ_INT(0x29B1, cfdp_crc_compute(check, sizeof(check)));

    const uint8_t a[] = {'A'};
    ASSERT_EQ_INT(0xB915, cfdp_crc_compute(a, sizeof(a)));
    return 0;
}

static int test_crc_empty_is_preset(void)
{
    /* No octets shifted in leaves the all-ones preset of §4.2.1.3. */
    ASSERT_EQ_INT(CFDP_CRC_INIT, cfdp_crc_compute(NULL, 0));

    const uint8_t data[] = {0xFF};
    ASSERT_EQ_INT(CFDP_CRC_INIT, cfdp_crc_compute(data, 0));
    ASSERT_EQ_INT(0x1234, cfdp_crc_update(0x1234, NULL, 4));
    return 0;
}

static int test_crc_streaming(void)
{
    const uint8_t check[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};

    /* Feeding the same octets in three calls must match one call. */
    uint16_t crc = (uint16_t)CFDP_CRC_INIT;
    crc = cfdp_crc_update(crc, &check[0], 4);
    crc = cfdp_crc_update(crc, &check[4], 1);
    crc = cfdp_crc_update(crc, &check[5], 4);
    ASSERT_EQ_INT(0x29B1, crc);
    return 0;
}

static int test_crc_residue_is_zero(void)
{
    /* CCSDS 232.0-B-3 §4.2.1.3 decodes by running the register over the
     * message and its appended CRC: an error-free sequence leaves zero. */
    uint8_t message[11] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    uint16_t crc = cfdp_crc_compute(message, 9);
    message[9] = (uint8_t)(crc >> 8);
    message[10] = (uint8_t)(crc & 0xFFU);
    ASSERT_EQ_INT(0, cfdp_crc_compute(message, sizeof(message)));

    /* And any single corrupted octet breaks that. */
    message[3] ^= 0x01;
    ASSERT_TRUE(cfdp_crc_compute(message, sizeof(message)) != 0);
    return 0;
}

test_result_t test_cfdp_crc_run_all(void)
{
    RUN_TEST(test_crc_check_values);
    RUN_TEST(test_crc_empty_is_preset);
    RUN_TEST(test_crc_streaming);
    RUN_TEST(test_crc_residue_is_zero);

    /* cunit.h keeps its tally in file-local statics, so these counters cover
     * only the tests run above. */
    test_result_t result = {cunit_total_tests - cunit_overall_failures, cunit_total_tests};
    return result;
}
