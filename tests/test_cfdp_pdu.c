/**
 * @file    test_cfdp_pdu.c
 * @brief   Unit tests for the fixed PDU header and File Data PDU codec
 *
 * Exercises src/cfdp_pdu.c against CCSDS 727.0-B-5 Section 5.1 (fixed PDU
 * header) and Section 5.3 (File Data PDU) with round-trip and known-vector
 * checks.
 * See also: docs/727x0b5e1.pdf
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#include "cfdp.h"
#include "cunit.h"
#include "test_runners.h"

#include <string.h>

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
    size_t n = cfdp_file_data_serialize(&fd,
                                        CFDP_FILE_SIZE_SMALL,
                                        CFDP_SEG_METADATA_ABSENT,
                                        buf,
                                        sizeof(buf));
    ASSERT_EQ_INT(4 + sizeof(payload), n);

    cfdp_file_data_pdu_t out = {0};
    size_t m =
        cfdp_file_data_deserialize(buf, n, CFDP_FILE_SIZE_SMALL, CFDP_SEG_METADATA_ABSENT, &out);
    ASSERT_EQ_INT(n, m);
    ASSERT_TRUE(out.offset == fd.offset);
    ASSERT_EQ_INT(sizeof(payload), out.file_data_len);
    ASSERT_EQ_MEM(payload, out.file_data, sizeof(payload));
    return 0;
}

static int test_file_data_empty_payload(void)
{
    cfdp_file_data_pdu_t fd = {0};
    fd.offset = 0x0000BEEFULL;

    uint8_t buf[8];
    size_t n = cfdp_file_data_serialize(&fd,
                                        CFDP_FILE_SIZE_SMALL,
                                        CFDP_SEG_METADATA_ABSENT,
                                        buf,
                                        sizeof(buf));
    ASSERT_EQ_INT(4, n);

    cfdp_file_data_pdu_t out = {0};
    ASSERT_EQ_INT(
        4,
        cfdp_file_data_deserialize(buf, n, CFDP_FILE_SIZE_SMALL, CFDP_SEG_METADATA_ABSENT, &out));
    ASSERT_TRUE(out.offset == fd.offset);
    ASSERT_TRUE(!out.file_data);
    ASSERT_EQ_INT(0, out.file_data_len);
    return 0;
}

static int test_file_data_large_file_roundtrip(void)
{
    const uint8_t payload[] = {0x11, 0x22};
    cfdp_file_data_pdu_t fd = {0};
    fd.offset = 0x0102030405060708ULL;
    fd.file_data = payload;
    fd.file_data_len = sizeof(payload);

    uint8_t buf[32];
    size_t n = cfdp_file_data_serialize(&fd,
                                        CFDP_FILE_SIZE_LARGE,
                                        CFDP_SEG_METADATA_ABSENT,
                                        buf,
                                        sizeof(buf));
    ASSERT_EQ_INT(8 + sizeof(payload), n);

    cfdp_file_data_pdu_t out = {0};
    ASSERT_EQ_INT(
        n,
        cfdp_file_data_deserialize(buf, n, CFDP_FILE_SIZE_LARGE, CFDP_SEG_METADATA_ABSENT, &out));
    ASSERT_TRUE(out.offset == fd.offset);
    ASSERT_EQ_MEM(payload, out.file_data, sizeof(payload));
    return 0;
}

static int test_directive_code_peek(void)
{
    const uint8_t buf[] = {CFDP_DIRECTIVE_METADATA, 0x00};
    cfdp_directive_code_t code;
    ASSERT_TRUE(cfdp_pdu_directive_code(buf, sizeof(buf), &code));
    ASSERT_EQ_INT(CFDP_DIRECTIVE_METADATA, code);

    ASSERT_TRUE(!cfdp_pdu_directive_code(NULL, sizeof(buf), &code));
    ASSERT_TRUE(!cfdp_pdu_directive_code(buf, sizeof(buf), NULL));
    ASSERT_TRUE(!cfdp_pdu_directive_code(buf, 0, &code));
    return 0;
}

static int test_header_size_invalid_lengths(void)
{
    cfdp_pdu_header_t hdr;
    fill_header(&hdr);
    ASSERT_EQ_INT(7, cfdp_pdu_header_size(&hdr));
    ASSERT_EQ_INT(0, cfdp_pdu_header_size(NULL));

    /* Both identifier lengths are rejected below 1 and above 8 octets. */
    fill_header(&hdr);
    hdr.entity_id_length = 0;
    ASSERT_EQ_INT(0, cfdp_pdu_header_size(&hdr));
    hdr.entity_id_length = (uint8_t)(CFDP_ID_LEN_MAX + 1U);
    ASSERT_EQ_INT(0, cfdp_pdu_header_size(&hdr));

    fill_header(&hdr);
    hdr.transaction_seq_length = 0;
    ASSERT_EQ_INT(0, cfdp_pdu_header_size(&hdr));
    hdr.transaction_seq_length = (uint8_t)(CFDP_ID_LEN_MAX + 1U);
    ASSERT_EQ_INT(0, cfdp_pdu_header_size(&hdr));
    return 0;
}

static int test_header_serialize_invalid_args(void)
{
    cfdp_pdu_header_t hdr;
    fill_header(&hdr);
    uint8_t buf[CFDP_PDU_HEADER_MAX_LEN];

    ASSERT_EQ_INT(0, cfdp_pdu_header_serialize(NULL, buf, sizeof(buf)));
    ASSERT_EQ_INT(0, cfdp_pdu_header_serialize(&hdr, NULL, sizeof(buf)));
    ASSERT_EQ_INT(0, cfdp_pdu_header_serialize(&hdr, buf, 4));

    hdr.entity_id_length = 0;
    ASSERT_EQ_INT(0, cfdp_pdu_header_serialize(&hdr, buf, sizeof(buf)));
    return 0;
}

static int test_header_deserialize_invalid_args(void)
{
    cfdp_pdu_header_t hdr;
    uint8_t buf[CFDP_PDU_HEADER_MAX_LEN] = {0};
    buf[0] = 0x20; /* version '001', so only the argument under test is at fault */

    ASSERT_EQ_INT(0, cfdp_pdu_header_deserialize(NULL, sizeof(buf), &hdr));
    ASSERT_EQ_INT(0, cfdp_pdu_header_deserialize(buf, sizeof(buf), NULL));
    ASSERT_EQ_INT(0, cfdp_pdu_header_deserialize(buf, 2, &hdr));

    /* Octet 3 asks for 8-octet identifiers, so the 4 octets supplied cannot
     * hold the identifier fields the header announces. */
    const uint8_t truncated[] = {0x20, 0x00, 0x0A, 0x77};
    ASSERT_EQ_INT(0, cfdp_pdu_header_deserialize(truncated, sizeof(truncated), &hdr));
    return 0;
}

static int test_header_directive_segment_flags_forced_zero(void)
{
    cfdp_pdu_header_t hdr;
    fill_header(&hdr);
    hdr.entity_id_length = 2;
    hdr.transaction_seq_length = 3;
    hdr.segmentation_control = CFDP_SEG_CTRL_BOUNDARIES_PRESERVED;
    hdr.segment_metadata_flag = CFDP_SEG_METADATA_PRESENT;

    uint8_t buf[CFDP_PDU_HEADER_MAX_LEN];

    /* Table 5-1: always '0' for File Directive PDUs. Octet 3 keeps only the
     * ID length fields: '001' (2 octets) and '010' (3 octets). */
    ASSERT_TRUE(cfdp_pdu_header_serialize(&hdr, buf, sizeof(buf)) > 0);
    ASSERT_EQ_INT(0x12, buf[3]);

    /* File Data PDUs carry both flags: bits 7 and 3 are set as well. */
    hdr.pdu_type = CFDP_PDU_TYPE_FILE_DATA;
    ASSERT_TRUE(cfdp_pdu_header_serialize(&hdr, buf, sizeof(buf)) > 0);
    ASSERT_EQ_INT(0x9A, buf[3]);
    return 0;
}

static int test_header_directive_segment_flags_ignored_on_decode(void)
{
    cfdp_pdu_header_t hdr;

    /* A File Directive PDU header from a peer that set both segment flags. */
    const uint8_t directive[] = {0x20, 0x00, 0x0A, 0x88, 0x01, 0x02, 0x03};
    ASSERT_EQ_INT(sizeof(directive),
                  cfdp_pdu_header_deserialize(directive, sizeof(directive), &hdr));
    ASSERT_EQ_INT(CFDP_PDU_TYPE_DIRECTIVE, hdr.pdu_type);
    ASSERT_EQ_INT(CFDP_SEG_CTRL_BOUNDARIES_NOT_PRESERVED, hdr.segmentation_control);
    ASSERT_EQ_INT(CFDP_SEG_METADATA_ABSENT, hdr.segment_metadata_flag);
    ASSERT_EQ_INT(1, hdr.entity_id_length);
    ASSERT_EQ_INT(1, hdr.transaction_seq_length);

    /* The same octet 3 on a File Data PDU is meaningful and decoded as sent. */
    const uint8_t file_data[] = {0x30, 0x00, 0x0A, 0x88, 0x01, 0x02, 0x03};
    ASSERT_EQ_INT(sizeof(file_data),
                  cfdp_pdu_header_deserialize(file_data, sizeof(file_data), &hdr));
    ASSERT_EQ_INT(CFDP_PDU_TYPE_FILE_DATA, hdr.pdu_type);
    ASSERT_EQ_INT(CFDP_SEG_CTRL_BOUNDARIES_PRESERVED, hdr.segmentation_control);
    ASSERT_EQ_INT(CFDP_SEG_METADATA_PRESENT, hdr.segment_metadata_flag);
    return 0;
}

static int test_header_serialize_rejects_other_versions(void)
{
    cfdp_pdu_header_t hdr;
    fill_header(&hdr);
    uint8_t buf[CFDP_PDU_HEADER_MAX_LEN];

    /* §5.1.2, table 5-1: the version field is '001'. Version 9 is included
     * because masking it to 3 bits would silently emit '001'. */
    const uint8_t bad_versions[] = {0, 2, 7, 9};
    for (size_t i = 0; i < sizeof(bad_versions); i++)
    {
        hdr.version = bad_versions[i];
        ASSERT_EQ_INT(0, cfdp_pdu_header_serialize(&hdr, buf, sizeof(buf)));
    }

    hdr.version = CFDP_PROTOCOL_VERSION;
    ASSERT_EQ_INT(7, cfdp_pdu_header_serialize(&hdr, buf, sizeof(buf)));
    return 0;
}

static int test_header_deserialize_rejects_other_versions(void)
{
    cfdp_pdu_header_t hdr;

    /* A complete header with 1-octet IDs; only the version bits vary. The
     * other octet-0 bits are left clear. */
    uint8_t buf[] = {0x00, 0x00, 0x0A, 0x00, 0x01, 0x02, 0x03};
    for (uint8_t version = 0; version <= 7; version++)
    {
        buf[0] = (uint8_t)(version << 5);
        size_t expected = (version == CFDP_PROTOCOL_VERSION) ? sizeof(buf) : 0U;
        ASSERT_EQ_INT(expected, cfdp_pdu_header_deserialize(buf, sizeof(buf), &hdr));
    }
    return 0;
}

static int test_file_data_serialize_invalid_args(void)
{
    const uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x42};
    cfdp_file_data_pdu_t fd = {0};
    fd.file_data = payload;
    fd.file_data_len = sizeof(payload);

    uint8_t buf[8];
    ASSERT_EQ_INT(0,
                  cfdp_file_data_serialize(NULL,
                                           CFDP_FILE_SIZE_SMALL,
                                           CFDP_SEG_METADATA_ABSENT,
                                           buf,
                                           sizeof(buf)));
    ASSERT_EQ_INT(0,
                  cfdp_file_data_serialize(&fd,
                                           CFDP_FILE_SIZE_SMALL,
                                           CFDP_SEG_METADATA_ABSENT,
                                           NULL,
                                           sizeof(buf)));

    /* 4 offset octets plus 5 payload octets do not fit in 8. */
    ASSERT_EQ_INT(0,
                  cfdp_file_data_serialize(&fd,
                                           CFDP_FILE_SIZE_SMALL,
                                           CFDP_SEG_METADATA_ABSENT,
                                           buf,
                                           sizeof(buf)));

    cfdp_file_data_pdu_t no_data = {0};
    no_data.file_data_len = 3;
    ASSERT_EQ_INT(0,
                  cfdp_file_data_serialize(&no_data,
                                           CFDP_FILE_SIZE_SMALL,
                                           CFDP_SEG_METADATA_ABSENT,
                                           buf,
                                           sizeof(buf)));
    return 0;
}

static int test_file_data_deserialize_invalid_args(void)
{
    const uint8_t buf[8] = {0};
    cfdp_file_data_pdu_t fd = {0};

    ASSERT_EQ_INT(0,
                  cfdp_file_data_deserialize(NULL,
                                             sizeof(buf),
                                             CFDP_FILE_SIZE_SMALL,
                                             CFDP_SEG_METADATA_ABSENT,
                                             &fd));
    ASSERT_EQ_INT(0,
                  cfdp_file_data_deserialize(buf,
                                             sizeof(buf),
                                             CFDP_FILE_SIZE_SMALL,
                                             CFDP_SEG_METADATA_ABSENT,
                                             NULL));
    ASSERT_EQ_INT(
        0,
        cfdp_file_data_deserialize(buf, 3, CFDP_FILE_SIZE_SMALL, CFDP_SEG_METADATA_ABSENT, &fd));
    return 0;
}

static int test_file_data_segment_metadata_exact_bytes(void)
{
    const uint8_t metadata[] = {0xAA, 0xBB, 0xCC};
    const uint8_t payload[] = {'H', 'I'};

    cfdp_file_data_pdu_t fd = {0};
    fd.offset = 16;
    fd.file_data = payload;
    fd.file_data_len = sizeof(payload);
    fd.record_continuation = CFDP_RECORD_CONT_END;
    fd.segment_metadata = metadata;
    fd.segment_metadata_len = sizeof(metadata);

    uint8_t buf[32];
    size_t n = cfdp_file_data_serialize(&fd,
                                        CFDP_FILE_SIZE_SMALL,
                                        CFDP_SEG_METADATA_PRESENT,
                                        buf,
                                        sizeof(buf));

    /* Table 5-14: record continuation state (2 bits) and segment metadata
     * length (6 bits) share the first octet, then the metadata, then the
     * offset, then the file data. 0x83 is state '10' with length 3. */
    const uint8_t expected[] = {0x83, 0xAA, 0xBB, 0xCC, 0x00, 0x00, 0x00, 0x10, 'H', 'I'};
    ASSERT_EQ_INT(sizeof(expected), n);
    ASSERT_EQ_MEM(expected, buf, sizeof(expected));
    return 0;
}

static int test_file_data_segment_metadata_roundtrip(void)
{
    const uint8_t metadata[] = {0x01, 0x02};
    const uint8_t payload[] = {'a', 'b', 'c'};

    cfdp_file_data_pdu_t fd = {0};
    fd.offset = 0x1122334455667788ULL;
    fd.file_data = payload;
    fd.file_data_len = sizeof(payload);
    fd.record_continuation = CFDP_RECORD_CONT_START_AND_END;
    fd.segment_metadata = metadata;
    fd.segment_metadata_len = sizeof(metadata);

    uint8_t buf[64];
    size_t n = cfdp_file_data_serialize(&fd,
                                        CFDP_FILE_SIZE_LARGE,
                                        CFDP_SEG_METADATA_PRESENT,
                                        buf,
                                        sizeof(buf));
    ASSERT_TRUE(n > 0);

    cfdp_file_data_pdu_t out;
    ASSERT_EQ_INT(
        n,
        cfdp_file_data_deserialize(buf, n, CFDP_FILE_SIZE_LARGE, CFDP_SEG_METADATA_PRESENT, &out));
    ASSERT_TRUE(out.offset == fd.offset);
    ASSERT_EQ_INT(CFDP_RECORD_CONT_START_AND_END, out.record_continuation);
    ASSERT_EQ_INT(sizeof(metadata), out.segment_metadata_len);
    ASSERT_EQ_MEM(metadata, out.segment_metadata, sizeof(metadata));
    ASSERT_EQ_INT(sizeof(payload), out.file_data_len);
    ASSERT_EQ_MEM(payload, out.file_data, sizeof(payload));
    return 0;
}

static int test_file_data_segment_metadata_absent_fields_cleared(void)
{
    const uint8_t wire[] = {0x00, 0x00, 0x00, 0x04, 'x'};

    cfdp_file_data_pdu_t out;
    memset(&out, 0xFF, sizeof(out));
    ASSERT_EQ_INT(sizeof(wire),
                  cfdp_file_data_deserialize(wire,
                                             sizeof(wire),
                                             CFDP_FILE_SIZE_SMALL,
                                             CFDP_SEG_METADATA_ABSENT,
                                             &out));
    ASSERT_EQ_INT(CFDP_RECORD_CONT_NEITHER, out.record_continuation);
    ASSERT_EQ_INT(0, out.segment_metadata_len);
    ASSERT_TRUE(!out.segment_metadata);
    ASSERT_TRUE(out.offset == 4);
    return 0;
}

static int test_file_data_segment_metadata_mismatched_flag(void)
{
    const uint8_t metadata[] = {0xAA};
    const uint8_t payload[] = {'x'};

    cfdp_file_data_pdu_t fd = {0};
    fd.file_data = payload;
    fd.file_data_len = sizeof(payload);
    fd.segment_metadata = metadata;
    fd.segment_metadata_len = sizeof(metadata);

    uint8_t buf[32];

    /* Metadata supplied while the header flag says absent would put the offset
     * somewhere the peer does not look for it. */
    ASSERT_EQ_INT(0,
                  cfdp_file_data_serialize(&fd,
                                           CFDP_FILE_SIZE_SMALL,
                                           CFDP_SEG_METADATA_ABSENT,
                                           buf,
                                           sizeof(buf)));

    /* The 6-bit length field cannot express more than 63 octets. */
    fd.segment_metadata_len = CFDP_SEGMENT_METADATA_MAX_LEN + 1U;
    ASSERT_EQ_INT(0,
                  cfdp_file_data_serialize(&fd,
                                           CFDP_FILE_SIZE_SMALL,
                                           CFDP_SEG_METADATA_PRESENT,
                                           buf,
                                           sizeof(buf)));

    /* A present flag with a NULL metadata pointer but a non-zero length. */
    fd.segment_metadata = NULL;
    fd.segment_metadata_len = 1;
    ASSERT_EQ_INT(0,
                  cfdp_file_data_serialize(&fd,
                                           CFDP_FILE_SIZE_SMALL,
                                           CFDP_SEG_METADATA_PRESENT,
                                           buf,
                                           sizeof(buf)));
    return 0;
}

static int test_file_data_segment_metadata_truncated(void)
{
    cfdp_file_data_pdu_t out;

    /* Empty data field: no room for the record continuation octet. */
    const uint8_t empty[] = {0x00};
    ASSERT_EQ_INT(0,
                  cfdp_file_data_deserialize(empty,
                                             0,
                                             CFDP_FILE_SIZE_SMALL,
                                             CFDP_SEG_METADATA_PRESENT,
                                             &out));

    /* Announces 5 metadata octets but carries 2. */
    const uint8_t short_metadata[] = {0x05, 0xAA, 0xBB};
    ASSERT_EQ_INT(0,
                  cfdp_file_data_deserialize(short_metadata,
                                             sizeof(short_metadata),
                                             CFDP_FILE_SIZE_SMALL,
                                             CFDP_SEG_METADATA_PRESENT,
                                             &out));

    /* Metadata complete, but the 4-octet offset is truncated. */
    const uint8_t short_offset[] = {0x01, 0xAA, 0x00, 0x00};
    ASSERT_EQ_INT(0,
                  cfdp_file_data_deserialize(short_offset,
                                             sizeof(short_offset),
                                             CFDP_FILE_SIZE_SMALL,
                                             CFDP_SEG_METADATA_PRESENT,
                                             &out));
    return 0;
}

static int test_pdu_payload_size_excludes_crc(void)
{
    cfdp_pdu_header_t hdr;
    fill_header(&hdr);
    hdr.data_field_length = 10;

    hdr.crc_flag = CFDP_CRC_ABSENT;
    ASSERT_EQ_INT(10, cfdp_pdu_payload_size(&hdr));

    /* §4.1.3.2: the CRC sits in the final octets of the data field and its
     * length is counted in the data field length. */
    hdr.crc_flag = CFDP_CRC_PRESENT;
    ASSERT_EQ_INT(8, cfdp_pdu_payload_size(&hdr));

    /* A data field too short to hold the CRC it claims is malformed. */
    hdr.data_field_length = 1;
    ASSERT_EQ_INT(0, cfdp_pdu_payload_size(&hdr));

    ASSERT_EQ_INT(0, cfdp_pdu_payload_size(NULL));
    return 0;
}

static int test_file_data_crc_octets_not_file_data(void)
{
    /* A File Data PDU whose data field is a 4-octet offset, three file octets
     * and a 2-octet CRC. Only the three file octets may reach the file. */
    const uint8_t data_field[] = {0x00, 0x00, 0x00, 0x00, 'A', 'B', 'C', 0x12, 0x34};

    cfdp_pdu_header_t hdr;
    fill_header(&hdr);
    hdr.pdu_type = CFDP_PDU_TYPE_FILE_DATA;
    hdr.crc_flag = CFDP_CRC_PRESENT;
    hdr.data_field_length = (uint16_t)sizeof(data_field);

    cfdp_file_data_pdu_t fd;
    size_t payload_len = cfdp_pdu_payload_size(&hdr);
    ASSERT_EQ_INT(sizeof(data_field) - CFDP_PDU_CRC_LEN, payload_len);
    ASSERT_EQ_INT(payload_len,
                  cfdp_file_data_deserialize(data_field,
                                             payload_len,
                                             hdr.large_file_flag,
                                             hdr.segment_metadata_flag,
                                             &fd));
    ASSERT_EQ_INT(3, fd.file_data_len);
    ASSERT_EQ_MEM("ABC", fd.file_data, 3);
    return 0;
}

/**
 * @brief Assemble a File Data PDU with the CRC flag set, ready for the CRC.
 *
 * @param[out] buf     Buffer receiving header plus payload.
 * @param[in]  buf_len Capacity of @p buf.
 * @return Octets written (header plus payload, no CRC yet), or 0 on error.
 */
static size_t build_crc_flagged_pdu(uint8_t *buf, size_t buf_len)
{
    static const uint8_t payload[] = {0x00, 0x00, 0x00, 0x00, 'A', 'B', 'C'};

    cfdp_pdu_header_t hdr;
    fill_header(&hdr);
    hdr.pdu_type = CFDP_PDU_TYPE_FILE_DATA;
    hdr.crc_flag = CFDP_CRC_PRESENT;
    hdr.data_field_length = (uint16_t)(sizeof(payload) + CFDP_PDU_CRC_LEN);

    size_t hlen = cfdp_pdu_header_serialize(&hdr, buf, buf_len);
    if ((hlen == 0) || (buf_len < hlen + sizeof(payload)))
    {
        return 0;
    }
    memcpy(&buf[hlen], payload, sizeof(payload));
    return hlen + sizeof(payload);
}

static int test_pdu_crc_append_and_verify(void)
{
    uint8_t buf[32];
    size_t pdu_len = build_crc_flagged_pdu(buf, sizeof(buf));
    ASSERT_TRUE(pdu_len > 0);

    size_t total = cfdp_pdu_crc_append(buf, pdu_len, sizeof(buf));
    ASSERT_EQ_INT(pdu_len + CFDP_PDU_CRC_LEN, total);

    /* §4.1.3.2: computed from the first header octet to the last payload
     * octet, and placed big-endian in the final two data field octets. */
    uint16_t crc = cfdp_crc_compute(buf, pdu_len);
    ASSERT_EQ_INT((uint8_t)(crc >> 8), buf[pdu_len]);
    ASSERT_EQ_INT((uint8_t)(crc & 0xFFU), buf[pdu_len + 1]);

    ASSERT_TRUE(cfdp_pdu_crc_verify(buf, total));

    /* Trailing link-layer padding beyond the PDU is ignored. */
    ASSERT_TRUE(cfdp_pdu_crc_verify(buf, sizeof(buf)));
    return 0;
}

static int test_pdu_crc_verify_detects_corruption(void)
{
    uint8_t buf[32];
    size_t pdu_len = build_crc_flagged_pdu(buf, sizeof(buf));
    size_t total = cfdp_pdu_crc_append(buf, pdu_len, sizeof(buf));
    ASSERT_TRUE(total > 0);

    /* A flipped bit in the header, in the payload, and in the CRC itself. */
    const size_t corrupt_at[] = {2, pdu_len - 1, total - 1};
    for (size_t i = 0; i < sizeof(corrupt_at) / sizeof(corrupt_at[0]); i++)
    {
        buf[corrupt_at[i]] ^= 0x01;
        ASSERT_TRUE(!cfdp_pdu_crc_verify(buf, total));
        buf[corrupt_at[i]] ^= 0x01;
    }
    ASSERT_TRUE(cfdp_pdu_crc_verify(buf, total));

    /* Fewer octets than the header announces cannot be verified. */
    ASSERT_TRUE(!cfdp_pdu_crc_verify(buf, total - 1));
    return 0;
}

static int test_pdu_crc_verify_without_crc(void)
{
    /* §4.1.2 applies only when the CRC flag is set: a PDU without one passes. */
    const uint8_t plain[] = {0x20, 0x00, 0x03, 0x00, 0x01, 0x02, 0x03, 0x04, 0x00, 0x05};
    ASSERT_TRUE(cfdp_pdu_crc_verify(plain, sizeof(plain)));

    /* An undecodable header (version '000') is discarded regardless. */
    const uint8_t bad_version[] = {0x00, 0x00, 0x03, 0x00, 0x01, 0x02, 0x03, 0x04, 0x00, 0x05};
    ASSERT_TRUE(!cfdp_pdu_crc_verify(bad_version, sizeof(bad_version)));

    /* CRC flag set but a data field too short to hold a CRC. */
    const uint8_t no_room[] = {0x22, 0x00, 0x01, 0x00, 0x01, 0x02, 0x03, 0x04};
    ASSERT_TRUE(!cfdp_pdu_crc_verify(no_room, sizeof(no_room)));

    ASSERT_TRUE(!cfdp_pdu_crc_verify(NULL, sizeof(plain)));
    return 0;
}

static int test_pdu_crc_append_rejects_inconsistent_header(void)
{
    uint8_t buf[32];
    size_t pdu_len = build_crc_flagged_pdu(buf, sizeof(buf));
    ASSERT_TRUE(pdu_len > 0);

    ASSERT_EQ_INT(0, cfdp_pdu_crc_append(NULL, pdu_len, sizeof(buf)));

    /* No room for the two CRC octets. */
    ASSERT_EQ_INT(0, cfdp_pdu_crc_append(buf, pdu_len, pdu_len + 1));

    /* Data field length sized for the payload alone: the header would then
     * describe a PDU two octets shorter than the one being built. */
    uint8_t short_len[32];
    memcpy(short_len, buf, sizeof(buf));
    short_len[2] = (uint8_t)(short_len[2] - CFDP_PDU_CRC_LEN);
    ASSERT_EQ_INT(0, cfdp_pdu_crc_append(short_len, pdu_len, sizeof(short_len)));

    /* CRC flag clear: there is nowhere the standard puts a CRC. */
    uint8_t no_flag[32];
    memcpy(no_flag, buf, sizeof(buf));
    no_flag[0] = (uint8_t)(no_flag[0] & ~0x02U);
    ASSERT_EQ_INT(0, cfdp_pdu_crc_append(no_flag, pdu_len, sizeof(no_flag)));

    /* Fewer octets than a header needs. */
    ASSERT_EQ_INT(0, cfdp_pdu_crc_append(buf, 3, sizeof(buf)));
    return 0;
}

static int test_pdu_crc_end_to_end_file_data(void)
{
    /* Sender: header with CRC, payload, CRC. Receiver: verify, then decode
     * only the payload, so the CRC octets never reach the file. */
    uint8_t buf[32];
    size_t pdu_len = build_crc_flagged_pdu(buf, sizeof(buf));
    size_t total = cfdp_pdu_crc_append(buf, pdu_len, sizeof(buf));
    ASSERT_TRUE(total > 0);
    ASSERT_TRUE(cfdp_pdu_crc_verify(buf, total));

    cfdp_pdu_header_t hdr;
    size_t hlen = cfdp_pdu_header_deserialize(buf, total, &hdr);
    ASSERT_TRUE(hlen > 0);
    ASSERT_EQ_INT(CFDP_CRC_PRESENT, hdr.crc_flag);

    cfdp_file_data_pdu_t fd;
    size_t payload_len = cfdp_pdu_payload_size(&hdr);
    ASSERT_EQ_INT(7, payload_len);
    ASSERT_EQ_INT(payload_len,
                  cfdp_file_data_deserialize(&buf[hlen],
                                             payload_len,
                                             hdr.large_file_flag,
                                             hdr.segment_metadata_flag,
                                             &fd));
    ASSERT_EQ_INT(3, fd.file_data_len);
    ASSERT_EQ_MEM("ABC", fd.file_data, 3);
    return 0;
}

static int test_file_data_exact_bytes_no_segment_metadata(void)
{
    /* Table 5-14 with the segment metadata flag clear: the data field is the
     * FSS offset followed by the file data, nothing else. */
    const uint8_t payload[] = {'x', 'y'};
    const uint8_t expected_small[] = {0x00, 0x00, 0x10, 0x00, 'x', 'y'};
    const uint8_t expected_large[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 'x', 'y'};

    cfdp_file_data_pdu_t fd = {0};
    fd.offset = 0x1000;
    fd.file_data = payload;
    fd.file_data_len = sizeof(payload);

    uint8_t buf[16];
    ASSERT_EQ_INT(sizeof(expected_small),
                  cfdp_file_data_serialize(&fd,
                                           CFDP_FILE_SIZE_SMALL,
                                           CFDP_SEG_METADATA_ABSENT,
                                           buf,
                                           sizeof(buf)));
    ASSERT_EQ_MEM(expected_small, buf, sizeof(expected_small));
    ASSERT_EQ_INT(sizeof(expected_large),
                  cfdp_file_data_serialize(&fd,
                                           CFDP_FILE_SIZE_LARGE,
                                           CFDP_SEG_METADATA_ABSENT,
                                           buf,
                                           sizeof(buf)));
    ASSERT_EQ_MEM(expected_large, buf, sizeof(expected_large));

    cfdp_file_data_pdu_t out;
    ASSERT_EQ_INT(sizeof(expected_large),
                  cfdp_file_data_deserialize(expected_large,
                                             sizeof(expected_large),
                                             CFDP_FILE_SIZE_LARGE,
                                             CFDP_SEG_METADATA_ABSENT,
                                             &out));
    ASSERT_TRUE(out.offset == 0x1000);
    ASSERT_EQ_INT(2, out.file_data_len);
    ASSERT_EQ_MEM(payload, out.file_data, 2);
    return 0;
}

test_result_t test_cfdp_pdu_run_all(void)
{
    RUN_TEST(test_header_exact_bytes);
    RUN_TEST(test_header_all_flags);
    RUN_TEST(test_header_roundtrip_large_ids);
    RUN_TEST(test_file_data_roundtrip);
    RUN_TEST(test_file_data_empty_payload);
    RUN_TEST(test_file_data_large_file_roundtrip);
    RUN_TEST(test_directive_code_peek);
    RUN_TEST(test_header_size_invalid_lengths);
    RUN_TEST(test_header_serialize_invalid_args);
    RUN_TEST(test_header_deserialize_invalid_args);
    RUN_TEST(test_header_serialize_rejects_other_versions);
    RUN_TEST(test_header_deserialize_rejects_other_versions);
    RUN_TEST(test_header_directive_segment_flags_forced_zero);
    RUN_TEST(test_header_directive_segment_flags_ignored_on_decode);
    RUN_TEST(test_file_data_serialize_invalid_args);
    RUN_TEST(test_file_data_deserialize_invalid_args);
    RUN_TEST(test_file_data_exact_bytes_no_segment_metadata);
    RUN_TEST(test_file_data_segment_metadata_exact_bytes);
    RUN_TEST(test_file_data_segment_metadata_roundtrip);
    RUN_TEST(test_file_data_segment_metadata_absent_fields_cleared);
    RUN_TEST(test_file_data_segment_metadata_mismatched_flag);
    RUN_TEST(test_file_data_segment_metadata_truncated);
    RUN_TEST(test_pdu_payload_size_excludes_crc);
    RUN_TEST(test_file_data_crc_octets_not_file_data);
    RUN_TEST(test_pdu_crc_append_and_verify);
    RUN_TEST(test_pdu_crc_verify_detects_corruption);
    RUN_TEST(test_pdu_crc_verify_without_crc);
    RUN_TEST(test_pdu_crc_append_rejects_inconsistent_header);
    RUN_TEST(test_pdu_crc_end_to_end_file_data);

    /* cunit.h keeps its tally in file-local statics, so these counters cover
     * only the tests run above. */
    test_result_t result = {cunit_total_tests - cunit_overall_failures, cunit_total_tests};
    return result;
}
