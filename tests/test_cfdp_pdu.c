/**
 * @file    test_cfdp_pdu.c
 * @brief   Unit tests for the fixed PDU header and File Data PDU codec
 *
 * Exercises src/cfdp_pdu.c against CCSDS 727.0-B-5 Section 5.1 (fixed PDU
 * header) and Section 5.3 (File Data PDU) with round-trip and known-vector
 * checks.
 * See also: docs/ccsds_cfdp.md
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#include "cunit.h"
#include "test_runners.h"

#include <string.h>

#include "cfdp.h"

static void fill_header(cfdp_pdu_header_t *hdr)
{
    memset(hdr, 0, sizeof(*hdr));
    hdr->version = CFDP_PROTOCOL_VERSION;
    hdr->pdu_type = CFDP_PDU_TYPE_DIRECTIVE;
    hdr->direction = CFDP_DIRECTION_TOWARD_RECEIVER;
    hdr->transmission_mode = CFDP_TRANS_MODE_ACKNOWLEDGED;
    hdr->crc_flag = CFDP_CRC_ABSENT;
    hdr->large_file_flag = CFDP_FILE_SIZE_SMALL;
    hdr->data_field_length = 10;
    hdr->segmentation_control = CFDP_SEG_CTRL_BOUNDARIES_NOT_PRESERVED;
    hdr->segment_metadata_flag = CFDP_SEG_METADATA_ABSENT;
    hdr->entity_id_length = 1;
    hdr->transaction_seq_length = 1;
    hdr->source_entity_id = 1;
    hdr->transaction_seq_number = 2;
    hdr->destination_entity_id = 3;
}

static int test_header_exact_bytes(void)
{
    cfdp_pdu_header_t hdr;
    fill_header(&hdr);

    uint8_t buf[CFDP_PDU_HEADER_MAX_LEN];
    size_t n = cfdp_pdu_header_serialize(&hdr, buf, sizeof(buf));

    const uint8_t expected[] = {0x20, 0x00, 0x0A, 0x00, 0x01, 0x02, 0x03};
    ASSERT_EQ_INT(sizeof(expected), n);
    ASSERT_EQ_MEM(expected, buf, sizeof(expected));
    return 0;
}

static int test_header_all_flags(void)
{
    cfdp_pdu_header_t hdr;
    fill_header(&hdr);
    hdr.pdu_type = CFDP_PDU_TYPE_FILE_DATA;
    hdr.direction = CFDP_DIRECTION_TOWARD_SENDER;
    hdr.transmission_mode = CFDP_TRANS_MODE_UNACKNOWLEDGED;
    hdr.crc_flag = CFDP_CRC_PRESENT;
    hdr.large_file_flag = CFDP_FILE_SIZE_LARGE;

    uint8_t buf[CFDP_PDU_HEADER_MAX_LEN];
    size_t n = cfdp_pdu_header_serialize(&hdr, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);
    ASSERT_EQ_INT(0x3F, buf[0]);
    return 0;
}

static int test_header_roundtrip_large_ids(void)
{
    cfdp_pdu_header_t hdr;
    fill_header(&hdr);
    hdr.large_file_flag = CFDP_FILE_SIZE_LARGE;
    hdr.entity_id_length = 4;
    hdr.transaction_seq_length = 8;
    hdr.source_entity_id = 0x11223344ULL;
    hdr.transaction_seq_number = 0x0102030405060708ULL;
    hdr.destination_entity_id = 0xAABBCCDDULL;

    uint8_t buf[CFDP_PDU_HEADER_MAX_LEN];
    size_t n = cfdp_pdu_header_serialize(&hdr, buf, sizeof(buf));
    ASSERT_EQ_INT(4 + 2 * 4 + 8, n);

    cfdp_pdu_header_t out;
    size_t m = cfdp_pdu_header_deserialize(buf, n, &out);
    ASSERT_EQ_INT(n, m);
    ASSERT_EQ_INT(hdr.entity_id_length, out.entity_id_length);
    ASSERT_EQ_INT(hdr.transaction_seq_length, out.transaction_seq_length);
    ASSERT_EQ_INT(CFDP_FILE_SIZE_LARGE, out.large_file_flag);
    ASSERT_TRUE(out.source_entity_id == hdr.source_entity_id);
    ASSERT_TRUE(out.transaction_seq_number == hdr.transaction_seq_number);
    ASSERT_TRUE(out.destination_entity_id == hdr.destination_entity_id);
    return 0;
}

static int test_file_data_roundtrip(void)
{
    const uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x42};
    cfdp_file_data_pdu_t fd = {0};
    fd.offset = 0x12345678ULL;
    fd.file_data = payload;
    fd.file_data_len = sizeof(payload);

    uint8_t buf[32];
    size_t n = cfdp_file_data_serialize(&fd, CFDP_FILE_SIZE_SMALL, buf, sizeof(buf));
    ASSERT_EQ_INT(4 + sizeof(payload), n);

    cfdp_file_data_pdu_t out = {0};
    size_t m = cfdp_file_data_deserialize(buf, n, CFDP_FILE_SIZE_SMALL, &out);
    ASSERT_EQ_INT(n, m);
    ASSERT_TRUE(out.offset == fd.offset);
    ASSERT_EQ_INT(sizeof(payload), out.file_data_len);
    ASSERT_EQ_MEM(payload, out.file_data, sizeof(payload));
    return 0;
}

static int test_directive_code_peek(void)
{
    const uint8_t buf[] = {CFDP_DIRECTIVE_METADATA, 0x00};
    cfdp_directive_code_t code;
    ASSERT_TRUE(cfdp_pdu_directive_code(buf, sizeof(buf), &code));
    ASSERT_EQ_INT(CFDP_DIRECTIVE_METADATA, code);
    ASSERT_TRUE(!cfdp_pdu_directive_code(buf, 0, &code));
    return 0;
}

static int test_header_invalid_args(void)
{
    cfdp_pdu_header_t hdr;
    fill_header(&hdr);
    uint8_t buf[4];
    ASSERT_EQ_INT(0, cfdp_pdu_header_serialize(&hdr, buf, sizeof(buf)));

    ASSERT_EQ_INT(0, cfdp_pdu_header_serialize(NULL, buf, sizeof(buf)));
    ASSERT_EQ_INT(0, cfdp_pdu_header_deserialize(buf, 2, &hdr));
    return 0;
}

test_result_t test_cfdp_pdu_run_all(void)
{
    RUN_TEST(test_header_exact_bytes);
    RUN_TEST(test_header_all_flags);
    RUN_TEST(test_header_roundtrip_large_ids);
    RUN_TEST(test_file_data_roundtrip);
    RUN_TEST(test_directive_code_peek);
    RUN_TEST(test_header_invalid_args);

    /* cunit.h keeps its tally in file-local statics, so these counters cover
     * only the tests run above. */
    test_result_t result = {cunit_total_tests - cunit_overall_failures, cunit_total_tests};
    return result;
}
