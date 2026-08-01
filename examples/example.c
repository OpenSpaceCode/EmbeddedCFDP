/**
 * @file    example.c
 * @brief   Worked example: build and parse a minimal CFDP file transfer
 *
 * Assembles the three PDUs of a tiny unacknowledged-mode transfer — Metadata,
 * File Data and EOF — for an in-memory "file", prints each PDU as hex, then
 * parses them back and verifies the file checksum.
 * Demonstrates CCSDS 727.0-B-5 (CCSDS File Delivery Protocol).
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#include "cfdp.h"

#include <stdio.h>

static const uint8_t g_file[] = "OpenSpaceCode CFDP demo payload";
static const size_t g_file_len = sizeof(g_file) - 1U; /* drop the NUL terminator */

static void print_hex(const char *label, const uint8_t *buf, size_t len)
{
    printf("%-10s (%2zu octets):", label, len);
    for (size_t i = 0; i < len; i++)
    {
        printf(" %02X", buf[i]);
    }
    printf("\n");
}

static void fill_common_header(cfdp_pdu_header_t *hdr, cfdp_pdu_type_t type)
{
    hdr->version = CFDP_PROTOCOL_VERSION;
    hdr->pdu_type = type;
    hdr->direction = CFDP_DIRECTION_TOWARD_RECEIVER;
    hdr->transmission_mode = CFDP_TRANS_MODE_UNACKNOWLEDGED;
    hdr->crc_flag = CFDP_CRC_ABSENT;
    hdr->large_file_flag = CFDP_FILE_SIZE_SMALL;
    hdr->segmentation_control = CFDP_SEG_CTRL_BOUNDARIES_NOT_PRESERVED;
    hdr->segment_metadata_flag = CFDP_SEG_METADATA_ABSENT;
    hdr->entity_id_length = 1;
    hdr->transaction_seq_length = 2;
    hdr->source_entity_id = 1;
    hdr->transaction_seq_number = 42;
    hdr->destination_entity_id = 2;
}

static size_t emit_pdu(const char *label,
                       cfdp_pdu_header_t *hdr,
                       const uint8_t *payload,
                       size_t payload_len,
                       uint8_t *out,
                       size_t out_len)
{
    size_t hlen = cfdp_pdu_header_size(hdr);
    hdr->data_field_length = (uint16_t)payload_len;
    if ((cfdp_pdu_header_serialize(hdr, out, out_len) == 0) || (out_len < hlen + payload_len))
    {
        return 0;
    }
    for (size_t i = 0; i < payload_len; i++)
    {
        out[hlen + i] = payload[i];
    }
    print_hex(label, out, hlen + payload_len);
    return hlen + payload_len;
}

static void build_metadata(uint8_t *out, size_t out_len)
{
    cfdp_pdu_header_t hdr;
    fill_common_header(&hdr, CFDP_PDU_TYPE_DIRECTIVE);

    cfdp_metadata_pdu_t md = {0};
    md.closure_requested = false;
    md.checksum_type = CFDP_CHECKSUM_MODULAR;
    md.file_size = g_file_len;
    md.source_filename = "src.dat";
    md.source_filename_len = 7;
    md.destination_filename = "dst.dat";
    md.destination_filename_len = 7;

    uint8_t payload[64];
    size_t plen = cfdp_metadata_serialize(&md, hdr.large_file_flag, payload, sizeof(payload));
    emit_pdu("Metadata", &hdr, payload, plen, out, out_len);
}

static void build_file_data(uint8_t *out, size_t out_len)
{
    cfdp_pdu_header_t hdr;
    fill_common_header(&hdr, CFDP_PDU_TYPE_FILE_DATA);

    cfdp_file_data_pdu_t fd = {0};
    fd.offset = 0;
    fd.file_data = g_file;
    fd.file_data_len = g_file_len;

    uint8_t payload[64];
    size_t plen = cfdp_file_data_serialize(&fd, hdr.large_file_flag, payload, sizeof(payload));
    emit_pdu("File Data", &hdr, payload, plen, out, out_len);
}

static void build_eof(uint8_t *out, size_t out_len)
{
    cfdp_pdu_header_t hdr;
    fill_common_header(&hdr, CFDP_PDU_TYPE_DIRECTIVE);

    cfdp_eof_pdu_t eof = {0};
    eof.condition_code = CFDP_COND_NO_ERROR;
    eof.file_checksum = cfdp_checksum_compute(g_file, g_file_len);
    eof.file_size = g_file_len;

    uint8_t payload[16];
    size_t plen = cfdp_eof_serialize(&eof, hdr.large_file_flag, payload, sizeof(payload));
    emit_pdu("EOF", &hdr, payload, plen, out, out_len);
}

static void parse_and_verify(const uint8_t *pdu, size_t pdu_len)
{
    cfdp_pdu_header_t hdr;
    size_t hlen = cfdp_pdu_header_deserialize(pdu, pdu_len, &hdr);
    cfdp_file_data_pdu_t fd;
    cfdp_file_data_deserialize(&pdu[hlen], hdr.data_field_length, hdr.large_file_flag, &fd);

    uint32_t checksum = cfdp_checksum_update(0, fd.offset, fd.file_data, fd.file_data_len);
    printf("\nReceiver reconstructed %zu octets at offset %llu, checksum 0x%08X\n",
           fd.file_data_len,
           (unsigned long long)fd.offset,
           checksum);
    printf("Expected file checksum:                              0x%08X\n",
           cfdp_checksum_compute(g_file, g_file_len));
}

int main(void)
{
    uint8_t metadata_pdu[96];
    uint8_t file_data_pdu[96];
    uint8_t eof_pdu[32];

    printf("=== CFDP small-file transfer (unacknowledged mode) ===\n\n");
    build_metadata(metadata_pdu, sizeof(metadata_pdu));
    build_file_data(file_data_pdu, sizeof(file_data_pdu));
    build_eof(eof_pdu, sizeof(eof_pdu));

    parse_and_verify(file_data_pdu, sizeof(file_data_pdu));
    return 0;
}
