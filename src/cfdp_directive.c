/**
 * @file    cfdp_directive.c
 * @brief   CFDP File Directive PDU codecs
 *
 * Implements CCSDS 727.0-B-5 (CCSDS File Delivery Protocol), Section 5.2 and
 * Section 5.4.
 * See also: docs/ccsds_cfdp.md
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#include "cfdp_directive.h"

#include <string.h>

#include "cfdp_endian.h"

/**
 * @brief Write a Length-Value field (1-octet length prefix plus value).
 *
 * @param[out] buf       Output buffer.
 * @param[in]  buf_len   Buffer capacity in octets.
 * @param[in]  value     Value octets; may be NULL only when @p value_len is 0.
 * @param[in]  value_len Value length in octets.
 * @return Bytes written, or 0 on error.
 */
static size_t cfdp_write_lv(uint8_t *buf, size_t buf_len, const char *value, uint8_t value_len)
{
    if ((value_len > 0) && (!value))
    {
        return 0;
    }
    if (buf_len < (size_t)value_len + 1U)
    {
        return 0;
    }
    buf[0] = value_len;
    if (value_len > 0)
    {
        memcpy(&buf[1], value, value_len);
    }
    return (size_t)value_len + 1U;
}

/**
 * @brief Read a Length-Value field, pointing @p value into @p buf.
 *
 * @param[in]  buf       Input buffer positioned at the length octet.
 * @param[in]  buf_len   Octets available in @p buf.
 * @param[out] value     Set to the value octets, or NULL when the value is empty.
 * @param[out] value_len Set to the value length in octets.
 * @return Bytes consumed, or 0 on error.
 */
static size_t cfdp_read_lv(const uint8_t *buf, size_t buf_len, const char **value,
                           uint8_t *value_len)
{
    if (buf_len < 1U)
    {
        return 0;
    }
    uint8_t len = buf[0];
    if (buf_len < (size_t)len + 1U)
    {
        return 0;
    }
    *value = (len > 0) ? (const char *)&buf[1] : NULL;
    *value_len = len;
    return (size_t)len + 1U;
}

size_t cfdp_eof_serialize(const cfdp_eof_pdu_t *eof,
                          cfdp_large_file_flag_t large_file_flag,
                          uint8_t *buf,
                          size_t buf_len)
{
    if ((!eof) || (!buf))
    {
        return 0;
    }

    uint8_t fs = cfdp_file_size_octets(large_file_flag);
    size_t size = (size_t)fs + 6U;
    if (buf_len < size)
    {
        return 0;
    }

    buf[0] = (uint8_t)CFDP_DIRECTIVE_EOF;
    buf[1] = (uint8_t)(((uint8_t)eof->condition_code & 0xFU) << 4);
    cfdp_write_uint(&buf[2], eof->file_checksum, 4);
    cfdp_write_uint(&buf[6], eof->file_size, fs);

    return size;
}

size_t cfdp_eof_deserialize(const uint8_t *buf,
                            size_t buf_len,
                            cfdp_large_file_flag_t large_file_flag,
                            cfdp_eof_pdu_t *eof)
{
    if ((!buf) || (!eof))
    {
        return 0;
    }

    uint8_t fs = cfdp_file_size_octets(large_file_flag);
    size_t size = (size_t)fs + 6U;
    if ((buf_len < size) || (buf[0] != (uint8_t)CFDP_DIRECTIVE_EOF))
    {
        return 0;
    }

    eof->condition_code = (cfdp_condition_code_t)((buf[1] >> 4) & 0xFU);
    eof->file_checksum = (uint32_t)cfdp_read_uint(&buf[2], 4);
    eof->file_size = cfdp_read_uint(&buf[6], fs);

    return size;
}

size_t cfdp_finished_serialize(const cfdp_finished_pdu_t *fin, uint8_t *buf, size_t buf_len)
{
    if ((!fin) || (!buf) || (buf_len < 2U))
    {
        return 0;
    }

    buf[0] = (uint8_t)CFDP_DIRECTIVE_FINISHED;
    buf[1] = (uint8_t)((((uint8_t)fin->condition_code & 0xFU) << 4) |
                       (((uint8_t)fin->delivery_code & 0x1U) << 2) |
                       ((uint8_t)fin->file_status & 0x3U));

    return 2;
}

size_t cfdp_finished_deserialize(const uint8_t *buf, size_t buf_len, cfdp_finished_pdu_t *fin)
{
    if ((!buf) || (!fin) || (buf_len < 2U) || (buf[0] != (uint8_t)CFDP_DIRECTIVE_FINISHED))
    {
        return 0;
    }

    fin->condition_code = (cfdp_condition_code_t)((buf[1] >> 4) & 0xFU);
    fin->delivery_code = (cfdp_delivery_code_t)((buf[1] >> 2) & 0x1U);
    fin->file_status = (cfdp_file_status_t)(buf[1] & 0x3U);

    return 2;
}

size_t cfdp_ack_serialize(const cfdp_ack_pdu_t *ack, uint8_t *buf, size_t buf_len)
{
    if ((!ack) || (!buf) || (buf_len < 3U))
    {
        return 0;
    }

    buf[0] = (uint8_t)CFDP_DIRECTIVE_ACK;
    buf[1] = (uint8_t)((((uint8_t)ack->ack_directive_code & 0xFU) << 4) |
                       (ack->directive_subtype & 0xFU));
    buf[2] = (uint8_t)((((uint8_t)ack->condition_code & 0xFU) << 4) |
                       ((uint8_t)ack->transaction_status & 0x3U));

    return 3;
}

size_t cfdp_ack_deserialize(const uint8_t *buf, size_t buf_len, cfdp_ack_pdu_t *ack)
{
    if ((!buf) || (!ack) || (buf_len < 3U) || (buf[0] != (uint8_t)CFDP_DIRECTIVE_ACK))
    {
        return 0;
    }

    ack->ack_directive_code = (cfdp_directive_code_t)((buf[1] >> 4) & 0xFU);
    ack->directive_subtype = (uint8_t)(buf[1] & 0xFU);
    ack->condition_code = (cfdp_condition_code_t)((buf[2] >> 4) & 0xFU);
    ack->transaction_status = (cfdp_transaction_status_t)(buf[2] & 0x3U);

    return 3;
}

size_t cfdp_metadata_serialize(const cfdp_metadata_pdu_t *md,
                               cfdp_large_file_flag_t large_file_flag,
                               uint8_t *buf,
                               size_t buf_len)
{
    if ((!md) || (!buf))
    {
        return 0;
    }

    uint8_t fs = cfdp_file_size_octets(large_file_flag);
    size_t need = (size_t)fs + (size_t)md->source_filename_len +
                  (size_t)md->destination_filename_len + 4U;
    if (buf_len < need)
    {
        return 0;
    }

    buf[0] = (uint8_t)CFDP_DIRECTIVE_METADATA;
    buf[1] = (uint8_t)(((md->closure_requested ? 1U : 0U) << 6) |
                       ((uint8_t)md->checksum_type & 0xFU));
    size_t pos = 2;
    cfdp_write_uint(&buf[pos], md->file_size, fs);
    pos += fs;

    size_t n = cfdp_write_lv(&buf[pos], buf_len - pos, md->source_filename,
                             md->source_filename_len);
    if (n == 0)
    {
        return 0;
    }
    pos += n;

    n = cfdp_write_lv(&buf[pos], buf_len - pos, md->destination_filename,
                      md->destination_filename_len);
    if (n == 0)
    {
        return 0;
    }
    return pos + n;
}

size_t cfdp_metadata_deserialize(const uint8_t *buf,
                                 size_t buf_len,
                                 cfdp_large_file_flag_t large_file_flag,
                                 cfdp_metadata_pdu_t *md)
{
    if ((!buf) || (!md))
    {
        return 0;
    }

    uint8_t fs = cfdp_file_size_octets(large_file_flag);
    if ((buf_len < (size_t)fs + 2U) || (buf[0] != (uint8_t)CFDP_DIRECTIVE_METADATA))
    {
        return 0;
    }

    md->closure_requested = (((buf[1] >> 6) & 0x1U) != 0);
    md->checksum_type = (cfdp_checksum_type_t)(buf[1] & 0xFU);
    size_t pos = 2;
    md->file_size = cfdp_read_uint(&buf[pos], fs);
    pos += fs;

    size_t n = cfdp_read_lv(&buf[pos], buf_len - pos, &md->source_filename,
                            &md->source_filename_len);
    if (n == 0)
    {
        return 0;
    }
    pos += n;

    n = cfdp_read_lv(&buf[pos], buf_len - pos, &md->destination_filename,
                     &md->destination_filename_len);
    if (n == 0)
    {
        return 0;
    }
    return pos + n;
}

size_t cfdp_nak_serialize(const cfdp_nak_pdu_t *nak,
                          cfdp_large_file_flag_t large_file_flag,
                          uint8_t *buf,
                          size_t buf_len)
{
    if ((!nak) || (!buf) || (nak->segment_request_count > CFDP_NAK_MAX_SEGMENT_REQUESTS))
    {
        return 0;
    }

    uint8_t fs = cfdp_file_size_octets(large_file_flag);
    size_t pair = 2U * (size_t)fs;
    size_t size = 1U + pair + nak->segment_request_count * pair;
    if (buf_len < size)
    {
        return 0;
    }

    buf[0] = (uint8_t)CFDP_DIRECTIVE_NAK;
    size_t pos = 1;
    cfdp_write_uint(&buf[pos], nak->start_of_scope, fs);
    pos += fs;
    cfdp_write_uint(&buf[pos], nak->end_of_scope, fs);
    pos += fs;

    for (size_t i = 0; i < nak->segment_request_count; i++)
    {
        cfdp_write_uint(&buf[pos], nak->segment_requests[i].start_offset, fs);
        pos += fs;
        cfdp_write_uint(&buf[pos], nak->segment_requests[i].end_offset, fs);
        pos += fs;
    }

    return pos;
}

size_t cfdp_nak_deserialize(const uint8_t *buf,
                            size_t buf_len,
                            cfdp_large_file_flag_t large_file_flag,
                            cfdp_nak_pdu_t *nak)
{
    if ((!buf) || (!nak))
    {
        return 0;
    }

    uint8_t fs = cfdp_file_size_octets(large_file_flag);
    size_t pair = 2U * (size_t)fs;
    if ((buf_len < 1U + pair) || (buf[0] != (uint8_t)CFDP_DIRECTIVE_NAK))
    {
        return 0;
    }

    size_t pos = 1;
    nak->start_of_scope = cfdp_read_uint(&buf[pos], fs);
    pos += fs;
    nak->end_of_scope = cfdp_read_uint(&buf[pos], fs);
    pos += fs;

    size_t count = (buf_len - pos) / pair;
    nak->segment_request_count = 0;
    for (size_t i = 0; (i < count) && (i < CFDP_NAK_MAX_SEGMENT_REQUESTS); i++)
    {
        nak->segment_requests[i].start_offset = cfdp_read_uint(&buf[pos], fs);
        pos += fs;
        nak->segment_requests[i].end_offset = cfdp_read_uint(&buf[pos], fs);
        pos += fs;
        nak->segment_request_count++;
    }

    return pos;
}

size_t cfdp_prompt_serialize(cfdp_prompt_response_t response, uint8_t *buf, size_t buf_len)
{
    if ((!buf) || (buf_len < 2U))
    {
        return 0;
    }

    buf[0] = (uint8_t)CFDP_DIRECTIVE_PROMPT;
    buf[1] = (uint8_t)(((uint8_t)response & 0x1U) << 7);

    return 2;
}

size_t cfdp_prompt_deserialize(const uint8_t *buf, size_t buf_len, cfdp_prompt_response_t *response)
{
    if ((!buf) || (!response) || (buf_len < 2U) || (buf[0] != (uint8_t)CFDP_DIRECTIVE_PROMPT))
    {
        return 0;
    }

    *response = (cfdp_prompt_response_t)((buf[1] >> 7) & 0x1U);

    return 2;
}

size_t cfdp_keep_alive_serialize(uint64_t progress,
                                 cfdp_large_file_flag_t large_file_flag,
                                 uint8_t *buf,
                                 size_t buf_len)
{
    if (!buf)
    {
        return 0;
    }

    uint8_t fs = cfdp_file_size_octets(large_file_flag);
    size_t size = (size_t)fs + 1U;
    if (buf_len < size)
    {
        return 0;
    }

    buf[0] = (uint8_t)CFDP_DIRECTIVE_KEEP_ALIVE;
    cfdp_write_uint(&buf[1], progress, fs);

    return size;
}

size_t cfdp_keep_alive_deserialize(const uint8_t *buf,
                                   size_t buf_len,
                                   cfdp_large_file_flag_t large_file_flag,
                                   uint64_t *progress)
{
    if ((!buf) || (!progress))
    {
        return 0;
    }

    uint8_t fs = cfdp_file_size_octets(large_file_flag);
    size_t size = (size_t)fs + 1U;
    if ((buf_len < size) || (buf[0] != (uint8_t)CFDP_DIRECTIVE_KEEP_ALIVE))
    {
        return 0;
    }

    *progress = cfdp_read_uint(&buf[1], fs);

    return size;
}
