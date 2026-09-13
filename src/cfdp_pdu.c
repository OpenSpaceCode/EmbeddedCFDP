/**
 * @file    cfdp_pdu.c
 * @brief   CFDP PDU fixed header and File Data PDU codec
 *
 * Implements CCSDS 727.0-B-5 (CCSDS File Delivery Protocol), Section 5.1
 * (fixed PDU header) and Section 5.3 (File Data PDU).
 * See also: docs/727x0b5e1.pdf
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#include "cfdp_pdu.h"

#include "cfdp_endian.h"

#include <string.h>

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
    /* Table 5-1: both segment flags are always '0' for File Directive PDUs. */
    uint8_t segment_flags_mask = (hdr->pdu_type == CFDP_PDU_TYPE_FILE_DATA) ? 0x1U : 0x0U;

    return (uint8_t)((((uint8_t)hdr->segmentation_control & segment_flags_mask) << 7) |
                     (((hdr->entity_id_length - 1U) & 0x7U) << 4) |
                     (((uint8_t)hdr->segment_metadata_flag & segment_flags_mask) << 3) |
                     ((hdr->transaction_seq_length - 1U) & 0x7U));
}

size_t cfdp_pdu_header_serialize(const cfdp_pdu_header_t *hdr, uint8_t *buf, size_t buf_len)
{
    if ((!hdr) || (!buf) || (hdr->version != CFDP_PROTOCOL_VERSION))
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

    /* Table 5-1: both segment flags are ignored for File Directive PDUs, so a
     * peer that sets them still decodes, with the flags reported as '0'. */
    uint8_t segment_flags_mask = (hdr->pdu_type == CFDP_PDU_TYPE_FILE_DATA) ? 0x1U : 0x0U;

    hdr->segmentation_control = (cfdp_seg_ctrl_t)((o3 >> 7) & segment_flags_mask);
    hdr->entity_id_length = (uint8_t)(((o3 >> 4) & 0x7U) + 1U);
    hdr->segment_metadata_flag = (cfdp_seg_metadata_flag_t)((o3 >> 3) & segment_flags_mask);
    hdr->transaction_seq_length = (uint8_t)((o3 & 0x7U) + 1U);
}

size_t cfdp_pdu_header_deserialize(const uint8_t *buf, size_t buf_len, cfdp_pdu_header_t *hdr)
{
    if ((!buf) || (!hdr) || (buf_len < CFDP_PDU_HEADER_FIXED_LEN))
    {
        return 0;
    }

    cfdp_unpack_flags(buf, hdr);

    /* §5.1.2: only version '001' is defined. Other versions may lay out the
     * header or data field differently, so decoding one here would misread it. */
    if (hdr->version != CFDP_PROTOCOL_VERSION)
    {
        return 0;
    }

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

size_t cfdp_pdu_payload_size(const cfdp_pdu_header_t *hdr)
{
    if (!hdr)
    {
        return 0;
    }
    if (hdr->crc_flag != CFDP_CRC_PRESENT)
    {
        return hdr->data_field_length;
    }
    if (hdr->data_field_length < CFDP_PDU_CRC_LEN)
    {
        return 0;
    }

    return (size_t)hdr->data_field_length - CFDP_PDU_CRC_LEN;
}

/**
 * @brief Whether a File Data payload agrees with the Segment Metadata flag.
 *
 * A payload carrying segment metadata under an absent flag (or the reverse)
 * would put the offset at a different place than the header announces, which
 * the peer decodes as file data at a wild offset rather than as an error.
 *
 * @param[in] fd                    Payload to check.
 * @param[in] segment_metadata_flag Segment Metadata flag of the PDU header.
 * @return true when @p fd may be encoded under @p segment_metadata_flag.
 */
static bool cfdp_segment_metadata_consistent(const cfdp_file_data_pdu_t *fd,
                                             cfdp_seg_metadata_flag_t segment_metadata_flag)
{
    if (segment_metadata_flag != CFDP_SEG_METADATA_PRESENT)
    {
        return (fd->segment_metadata_len == 0) && (!fd->segment_metadata);
    }

    return (fd->segment_metadata_len <= CFDP_SEGMENT_METADATA_MAX_LEN) &&
           ((fd->segment_metadata) || (fd->segment_metadata_len == 0));
}

/**
 * @brief Write the record continuation state, metadata length and metadata.
 *
 * @param[in]  fd  Payload supplying the segment metadata fields.
 * @param[out] buf Output buffer positioned at the first data field octet.
 * @return Bytes written.
 */
static size_t cfdp_segment_metadata_write(const cfdp_file_data_pdu_t *fd, uint8_t *buf)
{
    buf[0] = (uint8_t)((((uint8_t)fd->record_continuation & 0x3U) << 6) |
                       (fd->segment_metadata_len & 0x3FU));

    if (fd->segment_metadata_len > 0)
    {
        memcpy(&buf[1], fd->segment_metadata, fd->segment_metadata_len);
    }

    return (size_t)fd->segment_metadata_len + 1U;
}

size_t cfdp_file_data_serialize(const cfdp_file_data_pdu_t *fd,
                                cfdp_large_file_flag_t large_file_flag,
                                cfdp_seg_metadata_flag_t segment_metadata_flag,
                                uint8_t *buf,
                                size_t buf_len)
{
    if ((!fd) || (!buf) || ((!fd->file_data) && (fd->file_data_len > 0)) ||
        (!cfdp_segment_metadata_consistent(fd, segment_metadata_flag)))
    {
        return 0;
    }

    bool with_metadata = (segment_metadata_flag == CFDP_SEG_METADATA_PRESENT);
    size_t metadata_size = with_metadata ? ((size_t)fd->segment_metadata_len + 1U) : 0U;
    uint8_t offset_octets = cfdp_file_size_octets(large_file_flag);
    size_t size = metadata_size + (size_t)offset_octets + fd->file_data_len;
    if (buf_len < size)
    {
        return 0;
    }

    size_t pos = with_metadata ? cfdp_segment_metadata_write(fd, buf) : 0U;
    cfdp_write_uint(&buf[pos], fd->offset, offset_octets);
    pos += offset_octets;

    if (fd->file_data_len > 0)
    {
        memcpy(&buf[pos], fd->file_data, fd->file_data_len);
        pos += fd->file_data_len;
    }

    return pos;
}

/**
 * @brief Decode the segment metadata preceding the offset, when present.
 *
 * @param[in]  buf     Data field, positioned at its first octet.
 * @param[in]  buf_len Payload length in octets.
 * @param[out] fd      Payload receiving the segment metadata fields.
 * @return Bytes consumed, or 0 if the data field is too short.
 */
static size_t cfdp_segment_metadata_read(const uint8_t *buf,
                                         size_t buf_len,
                                         cfdp_file_data_pdu_t *fd)
{
    if (buf_len < 1U)
    {
        return 0;
    }

    fd->record_continuation = (cfdp_record_continuation_t)((buf[0] >> 6) & 0x3U);
    fd->segment_metadata_len = (uint8_t)(buf[0] & 0x3FU);
    if (buf_len < (size_t)fd->segment_metadata_len + 1U)
    {
        return 0;
    }

    fd->segment_metadata = (fd->segment_metadata_len > 0) ? &buf[1] : NULL;

    return (size_t)fd->segment_metadata_len + 1U;
}

size_t cfdp_file_data_deserialize(const uint8_t *buf,
                                  size_t buf_len,
                                  cfdp_large_file_flag_t large_file_flag,
                                  cfdp_seg_metadata_flag_t segment_metadata_flag,
                                  cfdp_file_data_pdu_t *fd)
{
    if ((!buf) || (!fd))
    {
        return 0;
    }

    fd->record_continuation = CFDP_RECORD_CONT_NEITHER;
    fd->segment_metadata = NULL;
    fd->segment_metadata_len = 0;

    size_t pos = 0;
    if (segment_metadata_flag == CFDP_SEG_METADATA_PRESENT)
    {
        pos = cfdp_segment_metadata_read(buf, buf_len, fd);
        if (pos == 0)
        {
            return 0;
        }
    }

    uint8_t offset_octets = cfdp_file_size_octets(large_file_flag);
    if (buf_len < pos + offset_octets)
    {
        return 0;
    }

    fd->offset = cfdp_read_uint(&buf[pos], offset_octets);
    pos += offset_octets;
    fd->file_data = (buf_len > pos) ? &buf[pos] : NULL;
    fd->file_data_len = buf_len - pos;

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
