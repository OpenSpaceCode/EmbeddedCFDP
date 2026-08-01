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

static int test_eof_serialize_invalid_args(void)
{
    cfdp_eof_pdu_t eof = {0};
    uint8_t buf[16];

    ASSERT_EQ_INT(0, cfdp_eof_serialize(NULL, CFDP_FILE_SIZE_SMALL, buf, sizeof(buf)));
    ASSERT_EQ_INT(0, cfdp_eof_serialize(&eof, CFDP_FILE_SIZE_SMALL, NULL, sizeof(buf)));
    ASSERT_EQ_INT(0, cfdp_eof_serialize(&eof, CFDP_FILE_SIZE_SMALL, buf, 9));
    return 0;
}

static int test_eof_deserialize_invalid_args(void)
{
    uint8_t buf[10] = {0};
    buf[0] = (uint8_t)CFDP_DIRECTIVE_EOF;
    cfdp_eof_pdu_t eof = {0};

    ASSERT_EQ_INT(0, cfdp_eof_deserialize(NULL, sizeof(buf), CFDP_FILE_SIZE_SMALL, &eof));
    ASSERT_EQ_INT(0, cfdp_eof_deserialize(buf, sizeof(buf), CFDP_FILE_SIZE_SMALL, NULL));
    ASSERT_EQ_INT(0, cfdp_eof_deserialize(buf, 9, CFDP_FILE_SIZE_SMALL, &eof));

    buf[0] = (uint8_t)CFDP_DIRECTIVE_FINISHED;
    ASSERT_EQ_INT(0, cfdp_eof_deserialize(buf, sizeof(buf), CFDP_FILE_SIZE_SMALL, &eof));
    return 0;
}

static int test_eof_large_file_roundtrip(void)
{
    cfdp_eof_pdu_t eof = {0};
    eof.condition_code = CFDP_COND_FILE_SIZE_ERROR;
    eof.file_checksum = 0xAABBCCDDU;
    eof.file_size = 0x0000000100000000ULL;

    uint8_t buf[16];
    size_t n = cfdp_eof_serialize(&eof, CFDP_FILE_SIZE_LARGE, buf, sizeof(buf));
    ASSERT_EQ_INT(14, n);

    cfdp_eof_pdu_t out = {0};
    ASSERT_EQ_INT(n, cfdp_eof_deserialize(buf, n, CFDP_FILE_SIZE_LARGE, &out));
    ASSERT_EQ_INT(CFDP_COND_FILE_SIZE_ERROR, out.condition_code);
    ASSERT_TRUE(out.file_checksum == 0xAABBCCDDU);
    ASSERT_TRUE(out.file_size == eof.file_size);
    return 0;
}

static int test_finished_invalid_args(void)
{
    cfdp_finished_pdu_t fin = {0};
    uint8_t buf[2] = {0};
    buf[0] = (uint8_t)CFDP_DIRECTIVE_FINISHED;

    ASSERT_EQ_INT(0, cfdp_finished_serialize(NULL, buf, sizeof(buf)));
    ASSERT_EQ_INT(0, cfdp_finished_serialize(&fin, NULL, sizeof(buf)));
    ASSERT_EQ_INT(0, cfdp_finished_serialize(&fin, buf, 1));

    ASSERT_EQ_INT(0, cfdp_finished_deserialize(NULL, sizeof(buf), &fin));
    ASSERT_EQ_INT(0, cfdp_finished_deserialize(buf, sizeof(buf), NULL));
    ASSERT_EQ_INT(0, cfdp_finished_deserialize(buf, 1, &fin));

    buf[0] = (uint8_t)CFDP_DIRECTIVE_EOF;
    ASSERT_EQ_INT(0, cfdp_finished_deserialize(buf, sizeof(buf), &fin));
    return 0;
}

static int test_ack_invalid_args(void)
{
    cfdp_ack_pdu_t ack = {0};
    uint8_t buf[3] = {0};
    buf[0] = (uint8_t)CFDP_DIRECTIVE_ACK;

    ASSERT_EQ_INT(0, cfdp_ack_serialize(NULL, buf, sizeof(buf)));
    ASSERT_EQ_INT(0, cfdp_ack_serialize(&ack, NULL, sizeof(buf)));
    ASSERT_EQ_INT(0, cfdp_ack_serialize(&ack, buf, 2));

    ASSERT_EQ_INT(0, cfdp_ack_deserialize(NULL, sizeof(buf), &ack));
    ASSERT_EQ_INT(0, cfdp_ack_deserialize(buf, sizeof(buf), NULL));
    ASSERT_EQ_INT(0, cfdp_ack_deserialize(buf, 2, &ack));

    buf[0] = (uint8_t)CFDP_DIRECTIVE_EOF;
    ASSERT_EQ_INT(0, cfdp_ack_deserialize(buf, sizeof(buf), &ack));
    return 0;
}

static int test_metadata_empty_filenames(void)
{
    cfdp_metadata_pdu_t md = {0};
    md.checksum_type = CFDP_CHECKSUM_NULL;
    md.file_size = 42;

    uint8_t buf[16];
    size_t n = cfdp_metadata_serialize(&md, CFDP_FILE_SIZE_SMALL, buf, sizeof(buf));
    ASSERT_EQ_INT(8, n);

    cfdp_metadata_pdu_t out = {0};
    ASSERT_EQ_INT(n, cfdp_metadata_deserialize(buf, n, CFDP_FILE_SIZE_SMALL, &out));
    ASSERT_TRUE(!out.closure_requested);
    ASSERT_EQ_INT(CFDP_CHECKSUM_NULL, out.checksum_type);
    ASSERT_TRUE(!out.source_filename);
    ASSERT_EQ_INT(0, out.source_filename_len);
    ASSERT_TRUE(!out.destination_filename);
    ASSERT_EQ_INT(0, out.destination_filename_len);
    return 0;
}

static int test_metadata_serialize_invalid_args(void)
{
    cfdp_metadata_pdu_t md = {0};
    md.source_filename = "input.bin";
    md.source_filename_len = 9;
    md.destination_filename = "output.bin";
    md.destination_filename_len = 10;

    uint8_t buf[64];
    ASSERT_EQ_INT(0, cfdp_metadata_serialize(NULL, CFDP_FILE_SIZE_SMALL, buf, sizeof(buf)));
    ASSERT_EQ_INT(0, cfdp_metadata_serialize(&md, CFDP_FILE_SIZE_SMALL, NULL, sizeof(buf)));
    ASSERT_EQ_INT(0, cfdp_metadata_serialize(&md, CFDP_FILE_SIZE_SMALL, buf, 26));

    /* A non-zero name length with no name is rejected by each name field. */
    md.source_filename = NULL;
    ASSERT_EQ_INT(0, cfdp_metadata_serialize(&md, CFDP_FILE_SIZE_SMALL, buf, sizeof(buf)));

    md.source_filename = "input.bin";
    md.destination_filename = NULL;
    ASSERT_EQ_INT(0, cfdp_metadata_serialize(&md, CFDP_FILE_SIZE_SMALL, buf, sizeof(buf)));
    return 0;
}

static int test_metadata_deserialize_invalid_args(void)
{
    uint8_t buf[16] = {0};
    buf[0] = (uint8_t)CFDP_DIRECTIVE_METADATA;
    cfdp_metadata_pdu_t md = {0};

    ASSERT_EQ_INT(0, cfdp_metadata_deserialize(NULL, sizeof(buf), CFDP_FILE_SIZE_SMALL, &md));
    ASSERT_EQ_INT(0, cfdp_metadata_deserialize(buf, sizeof(buf), CFDP_FILE_SIZE_SMALL, NULL));
    ASSERT_EQ_INT(0, cfdp_metadata_deserialize(buf, 5, CFDP_FILE_SIZE_SMALL, &md));

    buf[0] = (uint8_t)CFDP_DIRECTIVE_EOF;
    ASSERT_EQ_INT(0, cfdp_metadata_deserialize(buf, sizeof(buf), CFDP_FILE_SIZE_SMALL, &md));
    return 0;
}

static int test_metadata_deserialize_truncated_names(void)
{
    cfdp_metadata_pdu_t md = {0};

    /* Directive code, flags and a 4-octet file size, then nothing: the source
     * name's length octet is missing. */
    const uint8_t no_source[] = {CFDP_DIRECTIVE_METADATA, 0x00, 0x00, 0x00, 0x00, 0x00};
    ASSERT_EQ_INT(0,
                  cfdp_metadata_deserialize(no_source,
                                            sizeof(no_source),
                                            CFDP_FILE_SIZE_SMALL,
                                            &md));

    /* The source name claims 5 octets but only 1 follows. */
    const uint8_t short_source[] =
        {CFDP_DIRECTIVE_METADATA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 'a'};
    ASSERT_EQ_INT(0,
                  cfdp_metadata_deserialize(short_source,
                                            sizeof(short_source),
                                            CFDP_FILE_SIZE_SMALL,
                                            &md));

    /* An empty source name consumes the last octet, leaving no destination. */
    const uint8_t no_destination[] = {CFDP_DIRECTIVE_METADATA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    ASSERT_EQ_INT(0,
                  cfdp_metadata_deserialize(no_destination,
                                            sizeof(no_destination),
                                            CFDP_FILE_SIZE_SMALL,
                                            &md));
    return 0;
}

static int test_nak_serialize_invalid_args(void)
{
    cfdp_nak_pdu_t nak = {0};
    nak.segment_request_count = 1;
    nak.segment_requests[0].start_offset = 100;
    nak.segment_requests[0].end_offset = 200;

    uint8_t buf[16];
    ASSERT_EQ_INT(0, cfdp_nak_serialize(NULL, CFDP_FILE_SIZE_SMALL, buf, sizeof(buf)));
    ASSERT_EQ_INT(0, cfdp_nak_serialize(&nak, CFDP_FILE_SIZE_SMALL, NULL, sizeof(buf)));

    /* One request needs 17 octets: 1 directive, 8 scope, 8 request. */
    ASSERT_EQ_INT(0, cfdp_nak_serialize(&nak, CFDP_FILE_SIZE_SMALL, buf, sizeof(buf)));

    nak.segment_request_count = CFDP_NAK_MAX_SEGMENT_REQUESTS + 1U;
    ASSERT_EQ_INT(0, cfdp_nak_serialize(&nak, CFDP_FILE_SIZE_SMALL, buf, sizeof(buf)));
    return 0;
}

static int test_nak_deserialize_invalid_args(void)
{
    uint8_t buf[9] = {0};
    buf[0] = (uint8_t)CFDP_DIRECTIVE_NAK;
    cfdp_nak_pdu_t nak = {0};

    ASSERT_EQ_INT(0, cfdp_nak_deserialize(NULL, sizeof(buf), CFDP_FILE_SIZE_SMALL, &nak));
    ASSERT_EQ_INT(0, cfdp_nak_deserialize(buf, sizeof(buf), CFDP_FILE_SIZE_SMALL, NULL));
    ASSERT_EQ_INT(0, cfdp_nak_deserialize(buf, 8, CFDP_FILE_SIZE_SMALL, &nak));

    buf[0] = (uint8_t)CFDP_DIRECTIVE_EOF;
    ASSERT_EQ_INT(0, cfdp_nak_deserialize(buf, sizeof(buf), CFDP_FILE_SIZE_SMALL, &nak));
    return 0;
}

static int test_nak_deserialize_caps_segment_requests(void)
{
    /* One segment request more than the decoder can store. */
    uint8_t buf[1 + 8 + (CFDP_NAK_MAX_SEGMENT_REQUESTS + 1U) * 8U] = {0};
    buf[0] = (uint8_t)CFDP_DIRECTIVE_NAK;

    cfdp_nak_pdu_t out = {0};
    size_t n = cfdp_nak_deserialize(buf, sizeof(buf), CFDP_FILE_SIZE_SMALL, &out);
    ASSERT_EQ_INT(CFDP_NAK_MAX_SEGMENT_REQUESTS, out.segment_request_count);
    ASSERT_EQ_INT(9 + CFDP_NAK_MAX_SEGMENT_REQUESTS * 8U, n);
    return 0;
}

static int test_prompt_nak_roundtrip(void)
{
    uint8_t buf[2];
    ASSERT_EQ_INT(2, cfdp_prompt_serialize(CFDP_PROMPT_NAK, buf, sizeof(buf)));
    ASSERT_EQ_INT(0x00, buf[1]);

    cfdp_prompt_response_t resp = CFDP_PROMPT_KEEP_ALIVE;
    ASSERT_EQ_INT(2, cfdp_prompt_deserialize(buf, sizeof(buf), &resp));
    ASSERT_EQ_INT(CFDP_PROMPT_NAK, resp);
    return 0;
}

static int test_prompt_invalid_args(void)
{
    uint8_t buf[2] = {0};
    buf[0] = (uint8_t)CFDP_DIRECTIVE_PROMPT;
    cfdp_prompt_response_t resp;

    ASSERT_EQ_INT(0, cfdp_prompt_serialize(CFDP_PROMPT_NAK, NULL, sizeof(buf)));
    ASSERT_EQ_INT(0, cfdp_prompt_serialize(CFDP_PROMPT_NAK, buf, 1));

    ASSERT_EQ_INT(0, cfdp_prompt_deserialize(NULL, sizeof(buf), &resp));
    ASSERT_EQ_INT(0, cfdp_prompt_deserialize(buf, sizeof(buf), NULL));
    ASSERT_EQ_INT(0, cfdp_prompt_deserialize(buf, 1, &resp));

    buf[0] = (uint8_t)CFDP_DIRECTIVE_EOF;
    ASSERT_EQ_INT(0, cfdp_prompt_deserialize(buf, sizeof(buf), &resp));
    return 0;
}

static int test_keep_alive_invalid_args(void)
{
    uint8_t buf[5] = {0};
    buf[0] = (uint8_t)CFDP_DIRECTIVE_KEEP_ALIVE;
    uint64_t progress = 0;

    ASSERT_EQ_INT(0, cfdp_keep_alive_serialize(0, CFDP_FILE_SIZE_SMALL, NULL, sizeof(buf)));
    ASSERT_EQ_INT(0, cfdp_keep_alive_serialize(0, CFDP_FILE_SIZE_SMALL, buf, 4));

    ASSERT_EQ_INT(0, cfdp_keep_alive_deserialize(NULL, sizeof(buf), CFDP_FILE_SIZE_SMALL, &progress));
    ASSERT_EQ_INT(0, cfdp_keep_alive_deserialize(buf, sizeof(buf), CFDP_FILE_SIZE_SMALL, NULL));
    ASSERT_EQ_INT(0, cfdp_keep_alive_deserialize(buf, 4, CFDP_FILE_SIZE_SMALL, &progress));

    buf[0] = (uint8_t)CFDP_DIRECTIVE_EOF;
    ASSERT_EQ_INT(0, cfdp_keep_alive_deserialize(buf, sizeof(buf), CFDP_FILE_SIZE_SMALL, &progress));
    return 0;
}

test_result_t test_cfdp_directive_run_all(void)
{
    RUN_TEST(test_eof_roundtrip);
    RUN_TEST(test_eof_large_file_roundtrip);
    RUN_TEST(test_eof_serialize_invalid_args);
    RUN_TEST(test_eof_deserialize_invalid_args);
    RUN_TEST(test_finished_roundtrip);
    RUN_TEST(test_finished_invalid_args);
    RUN_TEST(test_ack_roundtrip);
    RUN_TEST(test_ack_invalid_args);
    RUN_TEST(test_metadata_roundtrip);
    RUN_TEST(test_metadata_empty_filenames);
    RUN_TEST(test_metadata_serialize_invalid_args);
    RUN_TEST(test_metadata_deserialize_invalid_args);
    RUN_TEST(test_metadata_deserialize_truncated_names);
    RUN_TEST(test_nak_roundtrip);
    RUN_TEST(test_nak_serialize_invalid_args);
    RUN_TEST(test_nak_deserialize_invalid_args);
    RUN_TEST(test_nak_deserialize_caps_segment_requests);
    RUN_TEST(test_prompt_keepalive_roundtrip);
    RUN_TEST(test_prompt_nak_roundtrip);
    RUN_TEST(test_prompt_invalid_args);
    RUN_TEST(test_keep_alive_invalid_args);

    /* cunit.h keeps its tally in file-local statics, so these counters cover
     * only the tests run above. */
    test_result_t result = {cunit_total_tests - cunit_overall_failures, cunit_total_tests};
    return result;
}
