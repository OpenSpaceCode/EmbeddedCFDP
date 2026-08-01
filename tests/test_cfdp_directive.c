/**
 * @file    test_cfdp_directive.c
 * @brief   Unit tests for the File Directive PDU codecs
 *
 * Exercises src/cfdp_directive.c against CCSDS 727.0-B-5 Section 5.2 and
 * Section 5.4 with round-trip and known-vector checks.
 * See also: docs/ccsds_cfdp.md
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#include "cunit.h"
#include "test_runners.h"

#include "cfdp.h"

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

static int test_eof_buffer_too_small(void)
{
    uint8_t buf[4];
    cfdp_eof_pdu_t eof = {0};
    ASSERT_EQ_INT(0, cfdp_eof_serialize(&eof, CFDP_FILE_SIZE_SMALL, buf, sizeof(buf)));
    return 0;
}

test_result_t test_cfdp_directive_run_all(void)
{
    RUN_TEST(test_eof_roundtrip);
    RUN_TEST(test_finished_roundtrip);
    RUN_TEST(test_ack_roundtrip);
    RUN_TEST(test_metadata_roundtrip);
    RUN_TEST(test_nak_roundtrip);
    RUN_TEST(test_prompt_keepalive_roundtrip);
    RUN_TEST(test_eof_buffer_too_small);

    /* cunit.h keeps its tally in file-local statics, so these counters cover
     * only the tests run above. */
    test_result_t result = {cunit_total_tests - cunit_overall_failures, cunit_total_tests};
    return result;
}
