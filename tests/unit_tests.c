/**
 * @file    unit_tests.c
 * @brief   Unit tests for the EmbeddedCFDP library
 *
 * Exercises the PDU header, File Data, File Directive and checksum codecs
 * against CCSDS 727.0-B-5 with round-trip and known-vector checks.
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#include "cunit.h"

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

static int test_eof_roundtrip(void)
{
    const uint8_t expected[] = {0x04, 0x00, 0x01, 0x02, 0x03, 0x04, 0x00, 0x00, 0x00, 0x10};
    cfdp_eof_pdu_t eof = {0};
    eof.condition_code = CFDP_COND_NO_ERROR;
    eof.file_checksum = 0x01020304U;
    eof.file_size = 16;

    uint8_t buf[16];
    size_t n = cfdp_eof_serialize(&eof, CFDP_FILE_SIZE_SMALL, buf, sizeof(buf));
    ASSERT_EQ_INT(sizeof(expected), n);
    ASSERT_EQ_MEM(expected, buf, sizeof(expected));

    cfdp_eof_pdu_t out = {0};
    size_t m = cfdp_eof_deserialize(buf, n, CFDP_FILE_SIZE_SMALL, &out);
    ASSERT_EQ_INT(n, m);
    ASSERT_EQ_INT(CFDP_COND_NO_ERROR, out.condition_code);
    ASSERT_TRUE(out.file_checksum == 0x01020304U);
    ASSERT_TRUE(out.file_size == 16);
    return 0;
}

static int test_finished_roundtrip(void)
{
    cfdp_finished_pdu_t fin = {0};
    fin.condition_code = CFDP_COND_FILE_CHECKSUM_FAILURE;
    fin.delivery_code = CFDP_DELIVERY_INCOMPLETE;
    fin.file_status = CFDP_FILE_STATUS_RETAINED;

    uint8_t buf[8];
    size_t n = cfdp_finished_serialize(&fin, buf, sizeof(buf));
    ASSERT_EQ_INT(2, n);

    cfdp_finished_pdu_t out = {0};
    ASSERT_EQ_INT(2, cfdp_finished_deserialize(buf, n, &out));
    ASSERT_EQ_INT(CFDP_COND_FILE_CHECKSUM_FAILURE, out.condition_code);
    ASSERT_EQ_INT(CFDP_DELIVERY_INCOMPLETE, out.delivery_code);
    ASSERT_EQ_INT(CFDP_FILE_STATUS_RETAINED, out.file_status);
    return 0;
}

static int test_ack_roundtrip(void)
{
    cfdp_ack_pdu_t ack = {0};
    ack.ack_directive_code = CFDP_DIRECTIVE_FINISHED;
    ack.directive_subtype = 1;
    ack.condition_code = CFDP_COND_NO_ERROR;
    ack.transaction_status = CFDP_TXN_STATUS_ACTIVE;

    uint8_t buf[8];
    size_t n = cfdp_ack_serialize(&ack, buf, sizeof(buf));
    ASSERT_EQ_INT(3, n);

    cfdp_ack_pdu_t out = {0};
    ASSERT_EQ_INT(3, cfdp_ack_deserialize(buf, n, &out));
    ASSERT_EQ_INT(CFDP_DIRECTIVE_FINISHED, out.ack_directive_code);
    ASSERT_EQ_INT(1, out.directive_subtype);
    ASSERT_EQ_INT(CFDP_TXN_STATUS_ACTIVE, out.transaction_status);
    return 0;
}

static int test_metadata_roundtrip(void)
{
    cfdp_metadata_pdu_t md = {0};
    md.closure_requested = true;
    md.checksum_type = CFDP_CHECKSUM_MODULAR;
    md.file_size = 1024;
    md.source_filename = "input.bin";
    md.source_filename_len = 9;
    md.destination_filename = "output.bin";
    md.destination_filename_len = 10;

    uint8_t buf[64];
    size_t n = cfdp_metadata_serialize(&md, CFDP_FILE_SIZE_SMALL, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);

    cfdp_metadata_pdu_t out = {0};
    size_t m = cfdp_metadata_deserialize(buf, n, CFDP_FILE_SIZE_SMALL, &out);
    ASSERT_EQ_INT(n, m);
    ASSERT_TRUE(out.closure_requested);
    ASSERT_TRUE(out.file_size == 1024);
    ASSERT_EQ_INT(9, out.source_filename_len);
    ASSERT_EQ_MEM("input.bin", out.source_filename, 9);
    ASSERT_EQ_INT(10, out.destination_filename_len);
    ASSERT_EQ_MEM("output.bin", out.destination_filename, 10);
    return 0;
}

static int test_nak_roundtrip(void)
{
    cfdp_nak_pdu_t nak = {0};
    nak.start_of_scope = 0;
    nak.end_of_scope = 4096;
    nak.segment_request_count = 2;
    nak.segment_requests[0].start_offset = 100;
    nak.segment_requests[0].end_offset = 200;
    nak.segment_requests[1].start_offset = 300;
    nak.segment_requests[1].end_offset = 400;

    uint8_t buf[64];
    size_t n = cfdp_nak_serialize(&nak, CFDP_FILE_SIZE_SMALL, buf, sizeof(buf));
    ASSERT_EQ_INT(1 + 2 * 4 + 2 * (2 * 4), n);

    cfdp_nak_pdu_t out = {0};
    size_t m = cfdp_nak_deserialize(buf, n, CFDP_FILE_SIZE_SMALL, &out);
    ASSERT_EQ_INT(n, m);
    ASSERT_EQ_INT(2, out.segment_request_count);
    ASSERT_TRUE(out.end_of_scope == 4096);
    ASSERT_TRUE(out.segment_requests[1].start_offset == 300);
    ASSERT_TRUE(out.segment_requests[1].end_offset == 400);
    return 0;
}

static int test_prompt_keepalive_roundtrip(void)
{
    uint8_t buf[16];
    size_t n = cfdp_prompt_serialize(CFDP_PROMPT_KEEP_ALIVE, buf, sizeof(buf));
    ASSERT_EQ_INT(2, n);
    cfdp_prompt_response_t resp;
    ASSERT_EQ_INT(2, cfdp_prompt_deserialize(buf, n, &resp));
    ASSERT_EQ_INT(CFDP_PROMPT_KEEP_ALIVE, resp);

    n = cfdp_keep_alive_serialize(0x0A0B0C0DULL, CFDP_FILE_SIZE_SMALL, buf, sizeof(buf));
    ASSERT_EQ_INT(5, n);
    uint64_t progress = 0;
    ASSERT_EQ_INT(5, cfdp_keep_alive_deserialize(buf, n, CFDP_FILE_SIZE_SMALL, &progress));
    ASSERT_TRUE(progress == 0x0A0B0C0DULL);
    return 0;
}

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

static int test_directive_code_peek(void)
{
    const uint8_t buf[] = {CFDP_DIRECTIVE_METADATA, 0x00};
    cfdp_directive_code_t code;
    ASSERT_TRUE(cfdp_pdu_directive_code(buf, sizeof(buf), &code));
    ASSERT_EQ_INT(CFDP_DIRECTIVE_METADATA, code);
    ASSERT_TRUE(!cfdp_pdu_directive_code(buf, 0, &code));
    return 0;
}

static int test_buffer_too_small(void)
{
    cfdp_pdu_header_t hdr;
    fill_header(&hdr);
    uint8_t buf[4];
    ASSERT_EQ_INT(0, cfdp_pdu_header_serialize(&hdr, buf, sizeof(buf)));

    cfdp_eof_pdu_t eof = {0};
    ASSERT_EQ_INT(0, cfdp_eof_serialize(&eof, CFDP_FILE_SIZE_SMALL, buf, 4));

    ASSERT_EQ_INT(0, cfdp_pdu_header_serialize(NULL, buf, sizeof(buf)));
    ASSERT_EQ_INT(0, cfdp_pdu_header_deserialize(buf, 2, &hdr));
    return 0;
}

int main(void)
{
    RUN_TEST(test_header_exact_bytes);
    RUN_TEST(test_header_all_flags);
    RUN_TEST(test_header_roundtrip_large_ids);
    RUN_TEST(test_file_data_roundtrip);
    RUN_TEST(test_eof_roundtrip);
    RUN_TEST(test_finished_roundtrip);
    RUN_TEST(test_ack_roundtrip);
    RUN_TEST(test_metadata_roundtrip);
    RUN_TEST(test_nak_roundtrip);
    RUN_TEST(test_prompt_keepalive_roundtrip);
    RUN_TEST(test_checksum_known);
    RUN_TEST(test_directive_code_peek);
    RUN_TEST(test_buffer_too_small);

    if (cunit_overall_failures == 0)
    {
        printf("ALL TESTS PASSED (%d)\n", cunit_total_tests);
        return 0;
    }
    printf("%d TEST(S) FAILED\n", cunit_overall_failures);
    return 1;
}
