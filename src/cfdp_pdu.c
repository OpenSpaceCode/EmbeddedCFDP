/**
 * @file    cfdp_pdu.c
 * @brief   CFDP PDU fixed header and File Data PDU codec
 *
 * Implements CCSDS 727.0-B-5 (CCSDS File Delivery Protocol), Section 5.1
 * (fixed PDU header) and Section 5.3 (File Data PDU).
 * See also: docs/ccsds_cfdp.md
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#include "cfdp_pdu.h"

#include <string.h>

#include "cfdp_endian.h"

/**
 * @brief Whether an identifier length is within the CFDP 1..8 octet range.
 *
 * @param[in] len Identifier length in octets.
 * @return true if @p len is a legal entity ID / sequence number length.
 */
static bool cfdp_id_len_valid(uint8_t len)
{
    return (len >= CFDP_ID_LEN_MIN) && (len <= CFDP_ID_LEN_MAX);
}

size_t cfdp_pdu_header_size(const cfdp_pdu_header_t *hdr)
{
    if (!hdr)
    {
        return 0;
    }
    if ((!cfdp_id_len_valid(hdr->entity_id_length)) ||
        (!cfdp_id_len_valid(hdr->transaction_seq_length)))
    {
        return 0;
    }
    return CFDP_PDU_HEADER_FIXED_LEN + (size_t)(2U * hdr->entity_id_length) +
           hdr->transaction_seq_length;
}

/**
 * @brief Pack the first octet of the fixed header (flags nibble/bit fields).
 *
 * @param[in] hdr Header supplying the flag fields.
 * @return The encoded octet 0.
 */
static uint8_t cfdp_pack_octet0(const cfdp_pdu_header_t *hdr)
{
    return (uint8_t)(((hdr->version & 0x7U) << 5) | (((uint8_t)hdr->pdu_type & 0x1U) << 4) |
                     (((uint8_t)hdr->direction & 0x1U) << 3) |
                     (((uint8_t)hdr->transmission_mode & 0x1U) << 2) |
                     (((uint8_t)hdr->crc_flag & 0x1U) << 1) |
                     ((uint8_t)hdr->large_file_flag & 0x1U));
}

/**
 * @brief Pack the fourth octet of the fixed header (identifier lengths).
 *
 * @param[in] hdr Header supplying the segmentation flags and ID lengths.
 * @return The encoded octet 3.
 */
static uint8_t cfdp_pack_octet3(const cfdp_pdu_header_t *hdr)
{
    return (uint8_t)((((uint8_t)hdr->segmentation_control & 0x1U) << 7) |
                     (((hdr->entity_id_length - 1U) & 0x7U) << 4) |
                     (((uint8_t)hdr->segment_metadata_flag & 0x1U) << 3) |
                     ((hdr->transaction_seq_length - 1U) & 0x7U));
}

size_t cfdp_pdu_header_serialize(const cfdp_pdu_header_t *hdr, uint8_t *buf, size_t buf_len)
{
    if ((!hdr) || (!buf))
    {
        return 0;
    }

    size_t size = cfdp_pdu_header_size(hdr);
    if ((size == 0) || (buf_len < size))
    {
        return 0;
    }

    buf[0] = cfdp_pack_octet0(hdr);
    cfdp_write_uint(&buf[1], hdr->data_field_length, 2);
    buf[3] = cfdp_pack_octet3(hdr);

    size_t pos = CFDP_PDU_HEADER_FIXED_LEN;
    cfdp_write_uint(&buf[pos], hdr->source_entity_id, hdr->entity_id_length);
    pos += hdr->entity_id_length;
    cfdp_write_uint(&buf[pos], hdr->transaction_seq_number, hdr->transaction_seq_length);
    pos += hdr->transaction_seq_length;
    cfdp_write_uint(&buf[pos], hdr->destination_entity_id, hdr->entity_id_length);

    return size;
}

/**
 * @brief Decode the two bitpacked octets of the fixed header into @p hdr.
 *
 * @param[in]  buf Input buffer positioned at octet 0 (at least 4 octets).
 * @param[out] hdr Header receiving the decoded flag and length fields.
 */
static void cfdp_unpack_flags(const uint8_t *buf, cfdp_pdu_header_t *hdr)
{
    uint8_t o0 = buf[0];
    uint8_t o3 = buf[3];

    hdr->version = (uint8_t)((o0 >> 5) & 0x7U);
    hdr->pdu_type = (cfdp_pdu_type_t)((o0 >> 4) & 0x1U);
    hdr->direction = (cfdp_direction_t)((o0 >> 3) & 0x1U);
    hdr->transmission_mode = (cfdp_transmission_mode_t)((o0 >> 2) & 0x1U);
    hdr->crc_flag = (cfdp_crc_flag_t)((o0 >> 1) & 0x1U);
    hdr->large_file_flag = (cfdp_large_file_flag_t)(o0 & 0x1U);

    hdr->data_field_length = (uint16_t)cfdp_read_uint(&buf[1], 2);

    hdr->segmentation_control = (cfdp_seg_ctrl_t)((o3 >> 7) & 0x1U);
    hdr->entity_id_length = (uint8_t)(((o3 >> 4) & 0x7U) + 1U);
    hdr->segment_metadata_flag = (cfdp_seg_metadata_flag_t)((o3 >> 3) & 0x1U);
    hdr->transaction_seq_length = (uint8_t)((o3 & 0x7U) + 1U);
}

size_t cfdp_pdu_header_deserialize(const uint8_t *buf, size_t buf_len, cfdp_pdu_header_t *hdr)
{
    if ((!buf) || (!hdr) || (buf_len < CFDP_PDU_HEADER_FIXED_LEN))
    {
        return 0;
    }

    cfdp_unpack_flags(buf, hdr);

    /* cfdp_unpack_flags always yields identifier lengths of 1..8 octets, so the
     * size == 0 arm is unreachable here; it guards against future decoding
     * changes, and keeps the line's branches out of the coverage report. */
    size_t size = cfdp_pdu_header_size(hdr);
    if ((size == 0) || (buf_len < size)) /* GCOVR_EXCL_BR_LINE */
    {
        return 0;
    }

    size_t pos = CFDP_PDU_HEADER_FIXED_LEN;
    hdr->source_entity_id = cfdp_read_uint(&buf[pos], hdr->entity_id_length);
    pos += hdr->entity_id_length;
    hdr->transaction_seq_number = cfdp_read_uint(&buf[pos], hdr->transaction_seq_length);
    pos += hdr->transaction_seq_length;
    hdr->destination_entity_id = cfdp_read_uint(&buf[pos], hdr->entity_id_length);

    return size;
}

size_t cfdp_file_data_serialize(const cfdp_file_data_pdu_t *fd,
                                cfdp_large_file_flag_t large_file_flag,
                                uint8_t *buf,
                                size_t buf_len)
{
    if ((!fd) || (!buf) || ((!fd->file_data) && (fd->file_data_len > 0)))
    {
        return 0;
    }

    uint8_t offset_octets = cfdp_file_size_octets(large_file_flag);
    size_t size = (size_t)offset_octets + fd->file_data_len;
    if (buf_len < size)
    {
        return 0;
    }

    cfdp_write_uint(buf, fd->offset, offset_octets);
    if (fd->file_data_len > 0)
    {
        memcpy(&buf[offset_octets], fd->file_data, fd->file_data_len);
    }

    return size;
}

size_t cfdp_file_data_deserialize(const uint8_t *buf,
                                  size_t buf_len,
                                  cfdp_large_file_flag_t large_file_flag,
                                  cfdp_file_data_pdu_t *fd)
{
    if ((!buf) || (!fd))
    {
        return 0;
    }

    uint8_t offset_octets = cfdp_file_size_octets(large_file_flag);
    if (buf_len < offset_octets)
    {
        return 0;
    }

    fd->offset = cfdp_read_uint(buf, offset_octets);
    fd->file_data = (buf_len > offset_octets) ? &buf[offset_octets] : NULL;
    fd->file_data_len = buf_len - offset_octets;

    return buf_len;
}

bool cfdp_pdu_directive_code(const uint8_t *buf, size_t buf_len, cfdp_directive_code_t *code)
{
    if ((!buf) || (!code) || (buf_len == 0))
    {
        return false;
    }
    *code = (cfdp_directive_code_t)buf[0];
    return true;
}
