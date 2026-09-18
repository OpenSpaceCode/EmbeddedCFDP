/**
 * @file    test_cfdp_directive.c
 * @brief   Unit tests for the File Directive PDU codecs
 *
 * Exercises src/cfdp_directive.c against CCSDS 727.0-B-5 Section 5.2 and
 * Section 5.4 with round-trip and known-vector checks.
 * See also: docs/727x0b5e1.pdf
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#include "cfdp.h"
#include "cunit.h"
#include "test_runners.h"

#include <string.h>

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
    /* 'Unsupported checksum type' is the one fault condition that carries no
     * Fault Location (§5.2.3). */
    fin.condition_code = CFDP_COND_UNSUPPORTED_CHECKSUM_TYPE;
    fin.delivery_code = CFDP_DELIVERY_INCOMPLETE;
    fin.file_status = CFDP_FILE_STATUS_RETAINED;

    uint8_t buf[8];
    size_t n = cfdp_finished_serialize(&fin, buf, sizeof(buf));
    ASSERT_EQ_INT(2, n);

    cfdp_finished_pdu_t out = {0};
    ASSERT_EQ_INT(2, cfdp_finished_deserialize(buf, n, &out));
    ASSERT_EQ_INT(CFDP_COND_UNSUPPORTED_CHECKSUM_TYPE, out.condition_code);
    ASSERT_EQ_INT(CFDP_DELIVERY_INCOMPLETE, out.delivery_code);
    ASSERT_EQ_INT(CFDP_FILE_STATUS_RETAINED, out.file_status);
    ASSERT_TRUE(!out.filestore_responses);
    ASSERT_EQ_INT(0, out.fault_location_len);

    /* The nominal close of a successful transfer carries no TLVs at all. */
    cfdp_finished_pdu_t nominal = {0};
    nominal.condition_code = CFDP_COND_NO_ERROR;
    nominal.delivery_code = CFDP_DELIVERY_COMPLETE;
    nominal.file_status = CFDP_FILE_STATUS_RETAINED;
    ASSERT_EQ_INT(2, cfdp_finished_serialize(&nominal, buf, sizeof(buf)));
    ASSERT_EQ_INT(0x02, buf[1]);
    return 0;
}

/**
 * @brief Encode two Filestore Response TLVs for the Finished PDU tests.
 */
static size_t build_filestore_responses(uint8_t *buf, size_t buf_len)
{
    cfdp_filestore_response_t resp = {0};
    resp.action_code = CFDP_FS_ACTION_DELETE_FILE;
    resp.status_code = CFDP_FS_STATUS_SUCCESSFUL;
    resp.first_filename = "a.dat";
    resp.first_filename_len = 5;

    size_t n = cfdp_filestore_response_tlv_serialize(&resp, buf, buf_len);

    resp.action_code = CFDP_FS_ACTION_CREATE_DIRECTORY;
    resp.status_code = CFDP_FS_STATUS_NOT_PERFORMED;
    resp.first_filename = "dir";
    resp.first_filename_len = 3;

    return n + cfdp_filestore_response_tlv_serialize(&resp, &buf[n], buf_len - n);
}

static int test_finished_filestore_and_fault_location(void)
{
    uint8_t responses[64];
    size_t responses_len = build_filestore_responses(responses, sizeof(responses));

    cfdp_finished_pdu_t fin = {0};
    fin.condition_code = CFDP_COND_FILESTORE_REJECTION;
    fin.delivery_code = CFDP_DELIVERY_INCOMPLETE;
    fin.file_status = CFDP_FILE_STATUS_DISCARDED_FILESTORE_REJECTION;
    fin.filestore_responses = responses;
    fin.filestore_responses_len = (uint16_t)responses_len;
    fin.fault_location_entity_id = 9;
    fin.fault_location_len = 1;

    uint8_t buf[96];
    size_t n = cfdp_finished_serialize(&fin, buf, sizeof(buf));
    ASSERT_EQ_INT(2 + responses_len + 3, n);

    cfdp_finished_pdu_t out = {0};
    ASSERT_EQ_INT(n, cfdp_finished_deserialize(buf, n, &out));
    ASSERT_EQ_INT(responses_len, out.filestore_responses_len);
    ASSERT_EQ_MEM(responses, out.filestore_responses, responses_len);
    ASSERT_TRUE(out.fault_location_entity_id == 9ULL);
    ASSERT_EQ_INT(1, out.fault_location_len);

    /* The reported span decodes back into the two responses it was built from. */
    cfdp_filestore_response_t first = {0};
    size_t consumed =
        cfdp_filestore_response_tlv_deserialize(out.filestore_responses, responses_len, &first);
    ASSERT_TRUE(consumed > 0);
    ASSERT_EQ_INT(CFDP_FS_ACTION_DELETE_FILE, first.action_code);

    cfdp_filestore_response_t second = {0};
    ASSERT_TRUE(cfdp_filestore_response_tlv_deserialize(&out.filestore_responses[consumed],
                                                        responses_len - consumed,
                                                        &second) > 0);
    ASSERT_EQ_INT(CFDP_FS_ACTION_CREATE_DIRECTORY, second.action_code);
    return 0;
}

static int test_finished_serialize_invalid_args(void)
{
    uint8_t responses[64];
    size_t responses_len = build_filestore_responses(responses, sizeof(responses));

    cfdp_finished_pdu_t fin = {0};
    fin.condition_code = CFDP_COND_FILESTORE_REJECTION;

    /* A fault condition demands a Fault Location (§5.2.3). */
    uint8_t buf[96];
    ASSERT_EQ_INT(0, cfdp_finished_serialize(&fin, buf, sizeof(buf)));

    fin.fault_location_entity_id = 9;
    fin.fault_location_len = 1;
    ASSERT_EQ_INT(5, cfdp_finished_serialize(&fin, buf, sizeof(buf)));

    /* No room for the Fault Location TLV after the fixed octets. */
    ASSERT_EQ_INT(0, cfdp_finished_serialize(&fin, buf, 3));

    fin.filestore_responses_len = (uint16_t)responses_len;
    ASSERT_EQ_INT(0, cfdp_finished_serialize(&fin, buf, sizeof(buf)));

    fin.filestore_responses = responses;
    ASSERT_EQ_INT(0, cfdp_finished_serialize(&fin, buf, responses_len));
    return 0;
}

static int test_finished_deserialize_bad_tlvs(void)
{
    cfdp_finished_pdu_t out = {0};

    /* Only Filestore Response and Entity ID TLVs may follow (§5.2.3). */
    const uint8_t wrong_type[] = {CFDP_DIRECTIVE_FINISHED, 0x00, CFDP_TLV_MESSAGE_TO_USER, 0x00};
    ASSERT_EQ_INT(0, cfdp_finished_deserialize(wrong_type, sizeof(wrong_type), &out));

    /* A Filestore Response after the Fault Location breaks the field order. */
    const uint8_t out_of_order[] = {CFDP_DIRECTIVE_FINISHED,
                                    0x00,
                                    CFDP_TLV_ENTITY_ID,
                                    0x01,
                                    0x07,
                                    CFDP_TLV_FILESTORE_RESPONSE,
                                    0x02,
                                    0x10,
                                    0x00};
    ASSERT_EQ_INT(0, cfdp_finished_deserialize(out_of_order, sizeof(out_of_order), &out));

    /* A TLV whose length runs past the end of the data field. */
    const uint8_t truncated[] = {CFDP_DIRECTIVE_FINISHED, 0x00, CFDP_TLV_FILESTORE_RESPONSE, 0x04};
    ASSERT_EQ_INT(0, cfdp_finished_deserialize(truncated, sizeof(truncated), &out));

    /* An Entity ID TLV carrying an illegal identifier length. */
    const uint8_t bad_entity[] = {CFDP_DIRECTIVE_FINISHED, 0x00, CFDP_TLV_ENTITY_ID, 0x00};
    ASSERT_EQ_INT(0, cfdp_finished_deserialize(bad_entity, sizeof(bad_entity), &out));
    return 0;
}

static int test_ack_roundtrip(void)
{
    cfdp_ack_pdu_t ack = {0};
    ack.ack_directive_code = CFDP_DIRECTIVE_FINISHED;
    ack.condition_code = CFDP_COND_NO_ERROR;
    ack.transaction_status = CFDP_TXN_STATUS_ACTIVE;

    uint8_t buf[8];
    size_t n = cfdp_ack_serialize(&ack, buf, sizeof(buf));
    ASSERT_EQ_INT(3, n);

    cfdp_ack_pdu_t out = {0};
    ASSERT_EQ_INT(3, cfdp_ack_deserialize(buf, n, &out));
    ASSERT_EQ_INT(CFDP_DIRECTIVE_FINISHED, out.ack_directive_code);
    ASSERT_EQ_INT(CFDP_TXN_STATUS_ACTIVE, out.transaction_status);
    return 0;
}

static int test_ack_exact_bytes(void)
{
    /* Table 5-8: acknowledged directive code and subtype share octet 1; the
     * condition code, two spare bits and transaction status share octet 2. */
    cfdp_ack_pdu_t ack = {0};
    ack.ack_directive_code = CFDP_DIRECTIVE_FINISHED;
    ack.condition_code = CFDP_COND_FILE_CHECKSUM_FAILURE;
    ack.transaction_status = CFDP_TXN_STATUS_TERMINATED;

    uint8_t buf[8];
    ASSERT_EQ_INT(3, cfdp_ack_serialize(&ack, buf, sizeof(buf)));

    const uint8_t expected_finished[] = {0x06, 0x51, 0x52};
    ASSERT_EQ_MEM(expected_finished, buf, sizeof(expected_finished));

    /* An ACK of EOF takes subtype '0000' instead. */
    ack.ack_directive_code = CFDP_DIRECTIVE_EOF;
    ASSERT_EQ_INT(3, cfdp_ack_serialize(&ack, buf, sizeof(buf)));

    const uint8_t expected_eof[] = {0x06, 0x40, 0x52};
    ASSERT_EQ_MEM(expected_eof, buf, sizeof(expected_eof));
    return 0;
}

static int test_ack_subtype_is_derived(void)
{
    /* The subtype is not a caller field, so an ACK of Finished always carries
     * '0001' and an ACK of any other directive always carries '0000'. */
    cfdp_ack_pdu_t ack = {0};
    uint8_t buf[8];

    ack.ack_directive_code = CFDP_DIRECTIVE_FINISHED;
    ASSERT_EQ_INT(3, cfdp_ack_serialize(&ack, buf, sizeof(buf)));
    ASSERT_EQ_INT(CFDP_ACK_SUBTYPE_FINISHED, buf[1] & 0x0FU);

    ack.ack_directive_code = CFDP_DIRECTIVE_EOF;
    ASSERT_EQ_INT(3, cfdp_ack_serialize(&ack, buf, sizeof(buf)));
    ASSERT_EQ_INT(CFDP_ACK_SUBTYPE_OTHER, buf[1] & 0x0FU);
    return 0;
}

static int test_ack_deserialize_rejects_wrong_subtype(void)
{
    cfdp_ack_pdu_t out = {0};

    /* ACK of Finished must carry subtype '0001', not '0000'. */
    uint8_t finished_zero[] = {0x06, 0x50, 0x00};
    ASSERT_EQ_INT(0, cfdp_ack_deserialize(finished_zero, sizeof(finished_zero), &out));

    /* ACK of EOF must carry subtype '0000', not '0001'. */
    uint8_t eof_one[] = {0x06, 0x41, 0x00};
    ASSERT_EQ_INT(0, cfdp_ack_deserialize(eof_one, sizeof(eof_one), &out));

    /* Any other subtype value is equally invalid. */
    uint8_t finished_seven[] = {0x06, 0x57, 0x00};
    ASSERT_EQ_INT(0, cfdp_ack_deserialize(finished_seven, sizeof(finished_seven), &out));

    /* The conformant encodings of both still decode. */
    uint8_t finished_ok[] = {0x06, 0x51, 0x00};
    ASSERT_EQ_INT(3, cfdp_ack_deserialize(finished_ok, sizeof(finished_ok), &out));
    ASSERT_EQ_INT(CFDP_DIRECTIVE_FINISHED, out.ack_directive_code);

    uint8_t eof_ok[] = {0x06, 0x40, 0x00};
    ASSERT_EQ_INT(3, cfdp_ack_deserialize(eof_ok, sizeof(eof_ok), &out));
    ASSERT_EQ_INT(CFDP_DIRECTIVE_EOF, out.ack_directive_code);
    return 0;
}

static int test_ack_rejects_unacknowledged_directives(void)
{
    /* Table 5-8: only EOF and Finished PDUs are acknowledged. Every other
     * 4-bit directive code - defined or reserved - is rejected on encode. */
    cfdp_ack_pdu_t ack = {0};
    uint8_t buf[8];
    for (uint8_t code = 0x0; code <= 0xF; code++)
    {
        ack.ack_directive_code = (cfdp_directive_code_t)code;
        size_t expected =
            ((code == CFDP_DIRECTIVE_EOF) || (code == CFDP_DIRECTIVE_FINISHED)) ? 3U : 0U;
        ASSERT_EQ_INT(expected, cfdp_ack_serialize(&ack, buf, sizeof(buf)));
    }
    return 0;
}

static int test_ack_deserialize_rejects_unacknowledged_directives(void)
{
    cfdp_ack_pdu_t out = {0};
    out.ack_directive_code = CFDP_DIRECTIVE_FINISHED;
    out.condition_code = CFDP_COND_FILE_SIZE_ERROR;
    out.transaction_status = CFDP_TXN_STATUS_TERMINATED;

    /* ACK of Metadata, with the subtype '0000' table 5-8 would give it. */
    const uint8_t metadata[] = {0x06, 0x70, 0x01};
    ASSERT_EQ_INT(0, cfdp_ack_deserialize(metadata, sizeof(metadata), &out));

    /* ACK of NAK, and of the reserved directive code '0000'. */
    const uint8_t nak[] = {0x06, 0x80, 0x01};
    ASSERT_EQ_INT(0, cfdp_ack_deserialize(nak, sizeof(nak), &out));
    const uint8_t reserved[] = {0x06, 0x00, 0x01};
    ASSERT_EQ_INT(0, cfdp_ack_deserialize(reserved, sizeof(reserved), &out));

    /* A rejected ACK leaves the output as it was. */
    ASSERT_EQ_INT(CFDP_DIRECTIVE_FINISHED, out.ack_directive_code);
    ASSERT_EQ_INT(CFDP_COND_FILE_SIZE_ERROR, out.condition_code);
    ASSERT_EQ_INT(CFDP_TXN_STATUS_TERMINATED, out.transaction_status);
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
    eof.condition_code = CFDP_COND_NO_ERROR;
    eof.file_checksum = 0xAABBCCDDU;
    eof.file_size = 0x0000000100000000ULL;

    uint8_t buf[24];
    size_t n = cfdp_eof_serialize(&eof, CFDP_FILE_SIZE_LARGE, buf, sizeof(buf));
    ASSERT_EQ_INT(14, n);

    cfdp_eof_pdu_t out = {0};
    ASSERT_EQ_INT(n, cfdp_eof_deserialize(buf, n, CFDP_FILE_SIZE_LARGE, &out));
    ASSERT_TRUE(out.file_checksum == 0xAABBCCDDU);
    ASSERT_TRUE(out.file_size == eof.file_size);
    ASSERT_EQ_INT(0, out.fault_location_len);
    return 0;
}

static int test_eof_fault_location_roundtrip(void)
{
    cfdp_eof_pdu_t eof = {0};
    eof.condition_code = CFDP_COND_FILE_SIZE_ERROR;
    eof.file_checksum = 0x01020304U;
    eof.file_size = 16;
    eof.fault_location_entity_id = 0xBEEF;
    eof.fault_location_len = 2;

    uint8_t buf[24];
    size_t n = cfdp_eof_serialize(&eof, CFDP_FILE_SIZE_SMALL, buf, sizeof(buf));

    /* 10 fixed octets, then a 2-octet Entity ID TLV with its type and length. */
    ASSERT_EQ_INT(14, n);
    ASSERT_EQ_INT(CFDP_TLV_ENTITY_ID, buf[10]);
    ASSERT_EQ_INT(2, buf[11]);
    ASSERT_EQ_INT(0xBE, buf[12]);
    ASSERT_EQ_INT(0xEF, buf[13]);

    cfdp_eof_pdu_t out = {0};
    ASSERT_EQ_INT(n, cfdp_eof_deserialize(buf, n, CFDP_FILE_SIZE_SMALL, &out));
    ASSERT_EQ_INT(CFDP_COND_FILE_SIZE_ERROR, out.condition_code);
    ASSERT_TRUE(out.fault_location_entity_id == 0xBEEFULL);
    ASSERT_EQ_INT(2, out.fault_location_len);
    return 0;
}

static int test_eof_fault_location_required(void)
{
    cfdp_eof_pdu_t eof = {0};
    eof.condition_code = CFDP_COND_FILESTORE_REJECTION;
    eof.file_size = 16;

    /* §5.2.2 requires the Fault Location on any condition but 'No error'. */
    uint8_t buf[24];
    ASSERT_EQ_INT(0, cfdp_eof_serialize(&eof, CFDP_FILE_SIZE_SMALL, buf, sizeof(buf)));

    eof.fault_location_entity_id = 3;
    eof.fault_location_len = 1;
    ASSERT_EQ_INT(13, cfdp_eof_serialize(&eof, CFDP_FILE_SIZE_SMALL, buf, sizeof(buf)));

    /* The TLV must still fit once the fixed part has been written. */
    ASSERT_EQ_INT(0, cfdp_eof_serialize(&eof, CFDP_FILE_SIZE_SMALL, buf, 11));
    return 0;
}

static int test_eof_deserialize_bad_fault_location(void)
{
    cfdp_eof_pdu_t eof = {0};
    eof.condition_code = CFDP_COND_NO_ERROR;
    eof.file_size = 16;

    uint8_t buf[24];
    size_t n = cfdp_eof_serialize(&eof, CFDP_FILE_SIZE_SMALL, buf, sizeof(buf));

    /* Trailing octets that are not a well-formed Entity ID TLV. */
    buf[n] = (uint8_t)CFDP_TLV_FLOW_LABEL;
    buf[n + 1U] = 1;
    buf[n + 2U] = 9;
    cfdp_eof_pdu_t out = {0};
    ASSERT_EQ_INT(0, cfdp_eof_deserialize(buf, n + 3U, CFDP_FILE_SIZE_SMALL, &out));
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
    /* A well-formed ACK of EOF, so only the argument under test is at fault. */
    cfdp_ack_pdu_t ack = {0};
    ack.ack_directive_code = CFDP_DIRECTIVE_EOF;
    uint8_t buf[3] = {0};
    buf[0] = (uint8_t)CFDP_DIRECTIVE_ACK;
    buf[1] = 0x40;

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

static int test_metadata_with_options(void)
{
    /* A Filestore Request followed by a Message to User, the two option TLVs a
     * Metadata PDU most commonly carries (§5.2.5). */
    uint8_t options[64];
    cfdp_filestore_request_t req = {0};
    req.action_code = CFDP_FS_ACTION_CREATE_DIRECTORY;
    req.first_filename = "logs";
    req.first_filename_len = 4;
    size_t options_len = cfdp_filestore_request_tlv_serialize(&req, options, sizeof(options));

    cfdp_tlv_t message = {0};
    message.type = (uint8_t)CFDP_TLV_MESSAGE_TO_USER;
    message.length = 4;
    message.value = (const uint8_t *)"cfdp";
    options_len +=
        cfdp_tlv_serialize(&message, &options[options_len], sizeof(options) - options_len);

    cfdp_metadata_pdu_t md = {0};
    md.file_size = 512;
    md.source_filename = "s.dat";
    md.source_filename_len = 5;
    md.destination_filename = "d.dat";
    md.destination_filename_len = 5;
    md.options = options;
    md.options_len = (uint16_t)options_len;

    uint8_t buf[96];
    size_t n = cfdp_metadata_serialize(&md, CFDP_FILE_SIZE_SMALL, buf, sizeof(buf));
    ASSERT_EQ_INT(4 + 5 + 5 + 4 + options_len, n);

    cfdp_metadata_pdu_t out = {0};
    ASSERT_EQ_INT(n, cfdp_metadata_deserialize(buf, n, CFDP_FILE_SIZE_SMALL, &out));
    ASSERT_EQ_INT(options_len, out.options_len);
    ASSERT_EQ_MEM(options, out.options, options_len);

    cfdp_filestore_request_t decoded = {0};
    ASSERT_TRUE(cfdp_filestore_request_tlv_deserialize(out.options, out.options_len, &decoded) > 0);
    ASSERT_EQ_INT(CFDP_FS_ACTION_CREATE_DIRECTORY, decoded.action_code);
    ASSERT_EQ_MEM("logs", decoded.first_filename, 4);
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

    /* Options declared but not supplied, then options that do not fit. */
    static const uint8_t flow_label[] = {CFDP_TLV_FLOW_LABEL, 0x02, 'a', 'b'};
    md.destination_filename = "output.bin";
    md.options_len = (uint16_t)sizeof(flow_label);
    ASSERT_EQ_INT(0, cfdp_metadata_serialize(&md, CFDP_FILE_SIZE_SMALL, buf, sizeof(buf)));

    md.options = flow_label;
    ASSERT_EQ_INT(0, cfdp_metadata_serialize(&md, CFDP_FILE_SIZE_SMALL, buf, 30));
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
    ASSERT_EQ_INT(
        0,
        cfdp_metadata_deserialize(no_source, sizeof(no_source), CFDP_FILE_SIZE_SMALL, &md));

    /* The source name claims 5 octets but only 1 follows. */
    const uint8_t short_source[] =
        {CFDP_DIRECTIVE_METADATA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 'a'};
    ASSERT_EQ_INT(
        0,
        cfdp_metadata_deserialize(short_source, sizeof(short_source), CFDP_FILE_SIZE_SMALL, &md));

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

static int test_nak_deserialize_rejects_excess_segment_requests(void)
{
    /* One segment request more than the decoder can store. Truncating would
     * tell the sender that gaps it was never shown had been satisfied. */
    uint8_t buf[1 + 8 + (CFDP_NAK_MAX_SEGMENT_REQUESTS + 1U) * 8U] = {0};
    buf[0] = (uint8_t)CFDP_DIRECTIVE_NAK;

    cfdp_nak_pdu_t out = {0};
    ASSERT_EQ_INT(0, cfdp_nak_deserialize(buf, sizeof(buf), CFDP_FILE_SIZE_SMALL, &out));
    return 0;
}

static int test_nak_deserialize_accepts_full_segment_requests(void)
{
    /* Exactly the capacity must still decode, and account for every octet. */
    uint8_t buf[1 + 8 + CFDP_NAK_MAX_SEGMENT_REQUESTS * 8U] = {0};
    buf[0] = (uint8_t)CFDP_DIRECTIVE_NAK;

    cfdp_nak_pdu_t out = {0};
    size_t n = cfdp_nak_deserialize(buf, sizeof(buf), CFDP_FILE_SIZE_SMALL, &out);
    ASSERT_EQ_INT(sizeof(buf), n);
    ASSERT_EQ_INT(CFDP_NAK_MAX_SEGMENT_REQUESTS, out.segment_request_count);
    return 0;
}

static int test_nak_deserialize_no_segment_requests(void)
{
    /* §5.2.6 allows N = 0: scope only, with no gaps to report. */
    uint8_t buf[9] = {0};
    buf[0] = (uint8_t)CFDP_DIRECTIVE_NAK;
    buf[5] = 0x10;

    cfdp_nak_pdu_t out;
    memset(&out, 0xFF, sizeof(out));
    ASSERT_EQ_INT(sizeof(buf), cfdp_nak_deserialize(buf, sizeof(buf), CFDP_FILE_SIZE_SMALL, &out));
    ASSERT_EQ_INT(0, out.segment_request_count);
    ASSERT_TRUE(out.start_of_scope == 0);
    ASSERT_TRUE(out.end_of_scope == 0x10000000U);
    return 0;
}

static int test_nak_deserialize_rejects_ragged_tail(void)
{
    /* §5.2.6: the segment requests fill the data field exactly. Three trailing
     * octets are a malformed PDU, not a request that ends early. */
    uint8_t buf[1 + 8 + 8 + 3] = {0};
    buf[0] = (uint8_t)CFDP_DIRECTIVE_NAK;

    cfdp_nak_pdu_t out = {0};
    ASSERT_EQ_INT(0, cfdp_nak_deserialize(buf, sizeof(buf), CFDP_FILE_SIZE_SMALL, &out));

    /* The same data field without the stray octets is well formed. */
    ASSERT_EQ_INT(17, cfdp_nak_deserialize(buf, 17, CFDP_FILE_SIZE_SMALL, &out));
    ASSERT_EQ_INT(1, out.segment_request_count);

    /* A large-file NAK needs 16-octet requests, so the same 8 trailing octets
     * that were a whole request above are now a ragged tail. */
    uint8_t large[1 + 16 + 8] = {0};
    large[0] = (uint8_t)CFDP_DIRECTIVE_NAK;
    ASSERT_EQ_INT(0, cfdp_nak_deserialize(large, sizeof(large), CFDP_FILE_SIZE_LARGE, &out));
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

    ASSERT_EQ_INT(0,
                  cfdp_keep_alive_deserialize(NULL, sizeof(buf), CFDP_FILE_SIZE_SMALL, &progress));
    ASSERT_EQ_INT(0, cfdp_keep_alive_deserialize(buf, sizeof(buf), CFDP_FILE_SIZE_SMALL, NULL));
    ASSERT_EQ_INT(0, cfdp_keep_alive_deserialize(buf, 4, CFDP_FILE_SIZE_SMALL, &progress));

    buf[0] = (uint8_t)CFDP_DIRECTIVE_EOF;
    ASSERT_EQ_INT(0,
                  cfdp_keep_alive_deserialize(buf, sizeof(buf), CFDP_FILE_SIZE_SMALL, &progress));
    return 0;
}

/* -------------------------------------------------------------------------
 * Golden-byte vectors, derived by hand from the tables of §5.2. Each test
 * serialises and compares octets, then decodes the literal vector, so both
 * codecs are pinned to the standard rather than merely to each other.
 * ---------------------------------------------------------------------- */

static int test_eof_exact_bytes_small_file(void)
{
    /* Table 5-6: code 04; condition 'File size error' (0110) in the high
     * nibble, spare low nibble; 32-bit checksum; 32-bit file size; fault
     * location as an Entity ID TLV (06, length 2, ID 0x0C0D). */
    const uint8_t expected[] =
        {0x04, 0x60, 0x12, 0x34, 0x56, 0x78, 0x00, 0x00, 0x0A, 0x0B, 0x06, 0x02, 0x0C, 0x0D};

    cfdp_eof_pdu_t eof = {0};
    eof.condition_code = CFDP_COND_FILE_SIZE_ERROR;
    eof.file_checksum = 0x12345678U;
    eof.file_size = 0x0A0B;
    eof.fault_location_entity_id = 0x0C0D;
    eof.fault_location_len = 2;

    uint8_t buf[32];
    ASSERT_EQ_INT(sizeof(expected),
                  cfdp_eof_serialize(&eof, CFDP_FILE_SIZE_SMALL, buf, sizeof(buf)));
    ASSERT_EQ_MEM(expected, buf, sizeof(expected));

    cfdp_eof_pdu_t out = {0};
    ASSERT_EQ_INT(sizeof(expected),
                  cfdp_eof_deserialize(expected, sizeof(expected), CFDP_FILE_SIZE_SMALL, &out));
    ASSERT_EQ_INT(CFDP_COND_FILE_SIZE_ERROR, out.condition_code);
    ASSERT_TRUE(out.file_checksum == 0x12345678U);
    ASSERT_TRUE(out.file_size == 0x0A0B);
    ASSERT_TRUE(out.fault_location_entity_id == 0x0C0D);
    ASSERT_EQ_INT(2, out.fault_location_len);
    return 0;
}

static int test_eof_exact_bytes_large_file(void)
{
    /* Table 5-6 with the Large File flag: the file size is 64 bits; 'No error'
     * omits the fault location. */
    const uint8_t expected[] =
        {0x04, 0x00, 0x89, 0xAB, 0xCD, 0xEF, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02};

    cfdp_eof_pdu_t eof = {0};
    eof.condition_code = CFDP_COND_NO_ERROR;
    eof.file_checksum = 0x89ABCDEFU;
    eof.file_size = 0x0000000100000002ULL;

    uint8_t buf[32];
    ASSERT_EQ_INT(sizeof(expected),
                  cfdp_eof_serialize(&eof, CFDP_FILE_SIZE_LARGE, buf, sizeof(buf)));
    ASSERT_EQ_MEM(expected, buf, sizeof(expected));

    cfdp_eof_pdu_t out = {0};
    ASSERT_EQ_INT(sizeof(expected),
                  cfdp_eof_deserialize(expected, sizeof(expected), CFDP_FILE_SIZE_LARGE, &out));
    ASSERT_EQ_INT(CFDP_COND_NO_ERROR, out.condition_code);
    ASSERT_TRUE(out.file_checksum == 0x89ABCDEFU);
    ASSERT_TRUE(out.file_size == 0x0000000100000002ULL);
    ASSERT_EQ_INT(0, out.fault_location_len);
    return 0;
}

static int test_finished_exact_bytes(void)
{
    /* Table 5-7: code 05; then condition 'Filestore rejection' (0100), spare
     * (0), delivery 'Data incomplete' (1), file status 'discarded due to
     * filestore rejection' (01) = 0100 0 1 01 = 0x45; one Filestore Response
     * TLV (table 5-17: Create File / 'Create not allowed' = 0x01, name "f",
     * empty message); fault location Entity ID TLV with 1-octet ID 7. */
    const uint8_t responses[] = {0x01, 0x04, 0x01, 0x01, 'f', 0x00};
    const uint8_t expected[] = {0x05, 0x45, 0x01, 0x04, 0x01, 0x01, 'f', 0x00, 0x06, 0x01, 0x07};

    cfdp_finished_pdu_t fin = {0};
    fin.condition_code = CFDP_COND_FILESTORE_REJECTION;
    fin.delivery_code = CFDP_DELIVERY_INCOMPLETE;
    fin.file_status = CFDP_FILE_STATUS_DISCARDED_FILESTORE_REJECTION;
    fin.filestore_responses = responses;
    fin.filestore_responses_len = sizeof(responses);
    fin.fault_location_entity_id = 7;
    fin.fault_location_len = 1;

    uint8_t buf[32];
    ASSERT_EQ_INT(sizeof(expected), cfdp_finished_serialize(&fin, buf, sizeof(buf)));
    ASSERT_EQ_MEM(expected, buf, sizeof(expected));

    cfdp_finished_pdu_t out = {0};
    ASSERT_EQ_INT(sizeof(expected), cfdp_finished_deserialize(expected, sizeof(expected), &out));
    ASSERT_EQ_INT(CFDP_COND_FILESTORE_REJECTION, out.condition_code);
    ASSERT_EQ_INT(CFDP_DELIVERY_INCOMPLETE, out.delivery_code);
    ASSERT_EQ_INT(CFDP_FILE_STATUS_DISCARDED_FILESTORE_REJECTION, out.file_status);
    ASSERT_EQ_INT(sizeof(responses), out.filestore_responses_len);
    ASSERT_EQ_MEM(responses, out.filestore_responses, sizeof(responses));
    ASSERT_TRUE(out.fault_location_entity_id == 7);
    ASSERT_EQ_INT(1, out.fault_location_len);
    return 0;
}

static int test_metadata_exact_bytes_small_file(void)
{
    /* Table 5-9: code 07; reserved (0), closure requested (1), reserved (00),
     * checksum type null (1111) = 0 1 00 1111 = 0x4F; 32-bit size 256; source
     * name LV "a"; destination name LV "bc"; a Flow Label option TLV. */
    const uint8_t options[] = {0x05, 0x01, 0xFF};
    const uint8_t expected[] =
        {0x07, 0x4F, 0x00, 0x00, 0x01, 0x00, 0x01, 'a', 0x02, 'b', 'c', 0x05, 0x01, 0xFF};

    cfdp_metadata_pdu_t md = {0};
    md.closure_requested = true;
    md.checksum_type = CFDP_CHECKSUM_NULL;
    md.file_size = 256;
    md.source_filename = "a";
    md.source_filename_len = 1;
    md.destination_filename = "bc";
    md.destination_filename_len = 2;
    md.options = options;
    md.options_len = sizeof(options);

    uint8_t buf[32];
    ASSERT_EQ_INT(sizeof(expected),
                  cfdp_metadata_serialize(&md, CFDP_FILE_SIZE_SMALL, buf, sizeof(buf)));
    ASSERT_EQ_MEM(expected, buf, sizeof(expected));

    cfdp_metadata_pdu_t out = {0};
    ASSERT_EQ_INT(
        sizeof(expected),
        cfdp_metadata_deserialize(expected, sizeof(expected), CFDP_FILE_SIZE_SMALL, &out));
    ASSERT_TRUE(out.closure_requested);
    ASSERT_EQ_INT(CFDP_CHECKSUM_NULL, out.checksum_type);
    ASSERT_TRUE(out.file_size == 256);
    ASSERT_EQ_INT(1, out.source_filename_len);
    ASSERT_EQ_MEM("a", out.source_filename, 1);
    ASSERT_EQ_INT(2, out.destination_filename_len);
    ASSERT_EQ_MEM("bc", out.destination_filename, 2);
    ASSERT_EQ_INT(sizeof(options), out.options_len);
    ASSERT_EQ_MEM(options, out.options, sizeof(options));
    return 0;
}

static int test_metadata_exact_bytes_large_file(void)
{
    /* Table 5-9 with the Large File flag: closure not requested, modular
     * checksum (0x00), 64-bit size 512, both name LVs empty, no options. */
    const uint8_t expected[] =
        {0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00};

    cfdp_metadata_pdu_t md = {0};
    md.checksum_type = CFDP_CHECKSUM_MODULAR;
    md.file_size = 512;

    uint8_t buf[32];
    ASSERT_EQ_INT(sizeof(expected),
                  cfdp_metadata_serialize(&md, CFDP_FILE_SIZE_LARGE, buf, sizeof(buf)));
    ASSERT_EQ_MEM(expected, buf, sizeof(expected));

    cfdp_metadata_pdu_t out = {0};
    ASSERT_EQ_INT(
        sizeof(expected),
        cfdp_metadata_deserialize(expected, sizeof(expected), CFDP_FILE_SIZE_LARGE, &out));
    ASSERT_TRUE(!out.closure_requested);
    ASSERT_EQ_INT(CFDP_CHECKSUM_MODULAR, out.checksum_type);
    ASSERT_TRUE(out.file_size == 512);
    ASSERT_EQ_INT(0, out.source_filename_len);
    ASSERT_EQ_INT(0, out.destination_filename_len);
    ASSERT_EQ_INT(0, out.options_len);
    return 0;
}

static int test_nak_exact_bytes_small_file(void)
{
    /* Tables 5-10 and 5-11: code 08; 32-bit start and end of scope; then
     * (start, end) offset pairs. The second pair is the all-zero request for
     * the Metadata PDU. */
    const uint8_t expected[] = {0x08, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x02, 0x00,
                                0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x30, 0x00,
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    cfdp_nak_pdu_t nak = {0};
    nak.start_of_scope = 0x10;
    nak.end_of_scope = 0x200;
    nak.segment_request_count = 2;
    nak.segment_requests[0].start_offset = 0x20;
    nak.segment_requests[0].end_offset = 0x30;

    uint8_t buf[64];
    ASSERT_EQ_INT(sizeof(expected),
                  cfdp_nak_serialize(&nak, CFDP_FILE_SIZE_SMALL, buf, sizeof(buf)));
    ASSERT_EQ_MEM(expected, buf, sizeof(expected));

    cfdp_nak_pdu_t out = {0};
    ASSERT_EQ_INT(sizeof(expected),
                  cfdp_nak_deserialize(expected, sizeof(expected), CFDP_FILE_SIZE_SMALL, &out));
    ASSERT_TRUE(out.start_of_scope == 0x10);
    ASSERT_TRUE(out.end_of_scope == 0x200);
    ASSERT_EQ_INT(2, out.segment_request_count);
    ASSERT_TRUE(out.segment_requests[0].start_offset == 0x20);
    ASSERT_TRUE(out.segment_requests[0].end_offset == 0x30);
    ASSERT_TRUE(out.segment_requests[1].start_offset == 0);
    ASSERT_TRUE(out.segment_requests[1].end_offset == 0);
    return 0;
}

static int test_nak_exact_bytes_large_file(void)
{
    /* Tables 5-10 and 5-11 with the Large File flag: every offset is 64 bits,
     * so one segment request is 16 octets, not the 8 of table 5-10's small-file
     * figure. */
    const uint8_t expected[] = {0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00,
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x10};

    cfdp_nak_pdu_t nak = {0};
    nak.start_of_scope = 1;
    nak.end_of_scope = 2;
    nak.segment_request_count = 1;
    nak.segment_requests[0].start_offset = 0x0000000100000000ULL;
    nak.segment_requests[0].end_offset = 0x0000000100000010ULL;

    uint8_t buf[64];
    ASSERT_EQ_INT(sizeof(expected),
                  cfdp_nak_serialize(&nak, CFDP_FILE_SIZE_LARGE, buf, sizeof(buf)));
    ASSERT_EQ_MEM(expected, buf, sizeof(expected));

    cfdp_nak_pdu_t out = {0};
    ASSERT_EQ_INT(sizeof(expected),
                  cfdp_nak_deserialize(expected, sizeof(expected), CFDP_FILE_SIZE_LARGE, &out));
    ASSERT_TRUE(out.start_of_scope == 1);
    ASSERT_TRUE(out.end_of_scope == 2);
    ASSERT_EQ_INT(1, out.segment_request_count);
    ASSERT_TRUE(out.segment_requests[0].start_offset == 0x0000000100000000ULL);
    ASSERT_TRUE(out.segment_requests[0].end_offset == 0x0000000100000010ULL);
    return 0;
}

static int test_prompt_exact_bytes(void)
{
    /* Table 5-12: code 09; response required in the top bit, 7 spare bits. */
    const uint8_t expected_nak[] = {0x09, 0x00};
    const uint8_t expected_keep_alive[] = {0x09, 0x80};

    uint8_t buf[4];
    ASSERT_EQ_INT(2, cfdp_prompt_serialize(CFDP_PROMPT_NAK, buf, sizeof(buf)));
    ASSERT_EQ_MEM(expected_nak, buf, 2);
    ASSERT_EQ_INT(2, cfdp_prompt_serialize(CFDP_PROMPT_KEEP_ALIVE, buf, sizeof(buf)));
    ASSERT_EQ_MEM(expected_keep_alive, buf, 2);

    cfdp_prompt_response_t response = CFDP_PROMPT_NAK;
    ASSERT_EQ_INT(2, cfdp_prompt_deserialize(expected_keep_alive, 2, &response));
    ASSERT_EQ_INT(CFDP_PROMPT_KEEP_ALIVE, response);
    ASSERT_EQ_INT(2, cfdp_prompt_deserialize(expected_nak, 2, &response));
    ASSERT_EQ_INT(CFDP_PROMPT_NAK, response);
    return 0;
}

static int test_keep_alive_exact_bytes(void)
{
    /* Table 5-13: code 0C; progress as a big-endian FSS offset. */
    const uint8_t expected_small[] = {0x0C, 0x01, 0x02, 0x03, 0x04};
    const uint8_t expected_large[] = {0x0C, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

    uint8_t buf[16];
    ASSERT_EQ_INT(5,
                  cfdp_keep_alive_serialize(0x01020304ULL, CFDP_FILE_SIZE_SMALL, buf, sizeof(buf)));
    ASSERT_EQ_MEM(expected_small, buf, sizeof(expected_small));
    ASSERT_EQ_INT(
        9,
        cfdp_keep_alive_serialize(0x0102030405060708ULL, CFDP_FILE_SIZE_LARGE, buf, sizeof(buf)));
    ASSERT_EQ_MEM(expected_large, buf, sizeof(expected_large));

    uint64_t progress = 0;
    ASSERT_EQ_INT(5,
                  cfdp_keep_alive_deserialize(expected_small, 5, CFDP_FILE_SIZE_SMALL, &progress));
    ASSERT_TRUE(progress == 0x01020304ULL);
    ASSERT_EQ_INT(9,
                  cfdp_keep_alive_deserialize(expected_large, 9, CFDP_FILE_SIZE_LARGE, &progress));
    ASSERT_TRUE(progress == 0x0102030405060708ULL);
    return 0;
}

test_result_t test_cfdp_directive_run_all(void)
{
    RUN_TEST(test_eof_roundtrip);
    RUN_TEST(test_eof_large_file_roundtrip);
    RUN_TEST(test_eof_fault_location_roundtrip);
    RUN_TEST(test_eof_fault_location_required);
    RUN_TEST(test_eof_deserialize_bad_fault_location);
    RUN_TEST(test_eof_serialize_invalid_args);
    RUN_TEST(test_eof_deserialize_invalid_args);
    RUN_TEST(test_finished_roundtrip);
    RUN_TEST(test_finished_filestore_and_fault_location);
    RUN_TEST(test_finished_serialize_invalid_args);
    RUN_TEST(test_finished_deserialize_bad_tlvs);
    RUN_TEST(test_finished_invalid_args);
    RUN_TEST(test_ack_roundtrip);
    RUN_TEST(test_ack_exact_bytes);
    RUN_TEST(test_ack_subtype_is_derived);
    RUN_TEST(test_ack_deserialize_rejects_wrong_subtype);
    RUN_TEST(test_ack_rejects_unacknowledged_directives);
    RUN_TEST(test_ack_deserialize_rejects_unacknowledged_directives);
    RUN_TEST(test_ack_invalid_args);
    RUN_TEST(test_metadata_roundtrip);
    RUN_TEST(test_metadata_empty_filenames);
    RUN_TEST(test_metadata_with_options);
    RUN_TEST(test_metadata_serialize_invalid_args);
    RUN_TEST(test_metadata_deserialize_invalid_args);
    RUN_TEST(test_metadata_deserialize_truncated_names);
    RUN_TEST(test_nak_roundtrip);
    RUN_TEST(test_nak_serialize_invalid_args);
    RUN_TEST(test_nak_deserialize_invalid_args);
    RUN_TEST(test_nak_deserialize_rejects_excess_segment_requests);
    RUN_TEST(test_nak_deserialize_accepts_full_segment_requests);
    RUN_TEST(test_nak_deserialize_no_segment_requests);
    RUN_TEST(test_nak_deserialize_rejects_ragged_tail);
    RUN_TEST(test_prompt_keepalive_roundtrip);
    RUN_TEST(test_prompt_nak_roundtrip);
    RUN_TEST(test_prompt_invalid_args);
    RUN_TEST(test_keep_alive_invalid_args);
    RUN_TEST(test_eof_exact_bytes_small_file);
    RUN_TEST(test_eof_exact_bytes_large_file);
    RUN_TEST(test_finished_exact_bytes);
    RUN_TEST(test_metadata_exact_bytes_small_file);
    RUN_TEST(test_metadata_exact_bytes_large_file);
    RUN_TEST(test_nak_exact_bytes_small_file);
    RUN_TEST(test_nak_exact_bytes_large_file);
    RUN_TEST(test_prompt_exact_bytes);
    RUN_TEST(test_keep_alive_exact_bytes);

    /* cunit.h keeps its tally in file-local statics, so these counters cover
     * only the tests run above. */
    test_result_t result = {cunit_total_tests - cunit_overall_failures, cunit_total_tests};
    return result;
}
