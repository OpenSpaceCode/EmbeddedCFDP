/**
 * @file    cfdp_directive.c
 * @brief   CFDP File Directive PDU codecs
 *
 * Implements CCSDS 727.0-B-5 (CCSDS File Delivery Protocol), Section 5.2 and
 * Section 5.4.
 * See also: docs/727x0b5e1.pdf
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#include "cfdp_directive.h"

#include "cfdp_endian.h"

#include <string.h>

/**
 * @brief Whether a condition code obliges a PDU to carry a Fault Location.
 *
 * @param[in] condition_code Condition code of the PDU being built.
 * @return true when the Fault Location TLV must be present (§5.2.3).
 */
static bool cfdp_fault_location_required(cfdp_condition_code_t condition_code)
{
    return (condition_code != CFDP_COND_NO_ERROR) &&
           (condition_code != CFDP_COND_UNSUPPORTED_CHECKSUM_TYPE);
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

    /* §5.2.2: the Fault Location is omitted only on 'No error'. Emitting a
     * fault condition without it would produce a malformed EOF PDU. */
    bool with_fault_location = (eof->condition_code != CFDP_COND_NO_ERROR);
    if ((with_fault_location) && (eof->fault_location_len == 0))
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

    if (!with_fault_location)
    {
        return size;
    }

    size_t n = cfdp_entity_id_tlv_serialize(eof->fault_location_entity_id,
                                            eof->fault_location_len,
                                            &buf[size],
                                            buf_len - size);
    if (n == 0)
    {
        return 0;
    }

    return size + n;
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
    eof->fault_location_entity_id = 0;
    eof->fault_location_len = 0;

    if (buf_len == size)
    {
        return size;
    }

    size_t n = cfdp_entity_id_tlv_deserialize(&buf[size],
                                              buf_len - size,
                                              &eof->fault_location_entity_id,
                                              &eof->fault_location_len);
    if (n == 0)
    {
        return 0;
    }

    return size + n;
}

size_t cfdp_finished_serialize(const cfdp_finished_pdu_t *fin, uint8_t *buf, size_t buf_len)
{
    if ((!fin) || (!buf) || (buf_len < 2U) ||
        ((!fin->filestore_responses) && (fin->filestore_responses_len > 0)))
    {
        return 0;
    }

    /* §5.2.3: the Fault Location is omitted only on 'No error' and
     * 'Unsupported checksum type'. */
    bool with_fault_location = cfdp_fault_location_required(fin->condition_code);
    if ((with_fault_location) && (fin->fault_location_len == 0))
    {
        return 0;
    }

    size_t pos = 2U;
    if (buf_len < pos + fin->filestore_responses_len)
    {
        return 0;
    }

    buf[0] = (uint8_t)CFDP_DIRECTIVE_FINISHED;
    buf[1] =
        (uint8_t)((((uint8_t)fin->condition_code & 0xFU) << 4) |
                  (((uint8_t)fin->delivery_code & 0x1U) << 2) | ((uint8_t)fin->file_status & 0x3U));

    if (fin->filestore_responses_len > 0)
    {
        memcpy(&buf[pos], fin->filestore_responses, fin->filestore_responses_len);
        pos += fin->filestore_responses_len;
    }

    if (!with_fault_location)
    {
        return pos;
    }

    size_t n = cfdp_entity_id_tlv_serialize(fin->fault_location_entity_id,
                                            fin->fault_location_len,
                                            &buf[pos],
                                            buf_len - pos);
    if (n == 0)
    {
        return 0;
    }

    return pos + n;
}

/**
 * @brief Decode the TLV chain trailing a Finished PDU's fixed octets.
 *
 * Filestore Responses are reported as the span they occupy in @p buf; an
 * Entity ID TLV is decoded into the Fault Location fields. §5.2.3 admits no
 * other TLV here, and the responses all precede the Fault Location.
 *
 * @param[in]  buf     Data field, positioned at the directive code.
 * @param[in]  buf_len Length of the data field in octets.
 * @param[out] fin     Finished contents receiving the decoded TLVs.
 * @return Bytes consumed in total, or 0 on a malformed or unexpected TLV.
 */
static size_t cfdp_finished_parse_tlvs(const uint8_t *buf, size_t buf_len, cfdp_finished_pdu_t *fin)
{
    size_t pos = 2U;
    size_t responses_start = pos;

    while (pos < buf_len)
    {
        cfdp_tlv_t tlv;
        size_t n = cfdp_tlv_deserialize(&buf[pos], buf_len - pos, &tlv);
        if (n == 0)
        {
            return 0;
        }

        if (tlv.type == (uint8_t)CFDP_TLV_FILESTORE_RESPONSE)
        {
            if (fin->fault_location_len > 0)
            {
                return 0;
            }
            fin->filestore_responses = &buf[responses_start];
            fin->filestore_responses_len = (uint16_t)((pos + n) - responses_start);
        }
        else if (tlv.type == (uint8_t)CFDP_TLV_ENTITY_ID)
        {
            if (cfdp_entity_id_tlv_deserialize(&buf[pos],
                                               buf_len - pos,
                                               &fin->fault_location_entity_id,
                                               &fin->fault_location_len) == 0)
            {
                return 0;
            }
        }
        else
        {
            return 0;
        }

        pos += n;
    }

    return pos;
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
    fin->filestore_responses = NULL;
    fin->filestore_responses_len = 0;
    fin->fault_location_entity_id = 0;
    fin->fault_location_len = 0;

    return cfdp_finished_parse_tlvs(buf, buf_len, fin);
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
    if ((!md) || (!buf) || ((!md->options) && (md->options_len > 0)))
    {
        return 0;
    }

    uint8_t fs = cfdp_file_size_octets(large_file_flag);
    size_t need = (size_t)fs + (size_t)md->source_filename_len +
                  (size_t)md->destination_filename_len + (size_t)md->options_len + 4U;
    if (buf_len < need)
    {
        return 0;
    }

    buf[0] = (uint8_t)CFDP_DIRECTIVE_METADATA;
    buf[1] =
        (uint8_t)(((md->closure_requested ? 1U : 0U) << 6) | ((uint8_t)md->checksum_type & 0xFU));
    size_t pos = 2;
    cfdp_write_uint(&buf[pos], md->file_size, fs);
    pos += fs;

    size_t n =
        cfdp_lv_serialize(md->source_filename, md->source_filename_len, &buf[pos], buf_len - pos);
    if (n == 0)
    {
        return 0;
    }
    pos += n;

    n = cfdp_lv_serialize(md->destination_filename,
                          md->destination_filename_len,
                          &buf[pos],
                          buf_len - pos);
    if (n == 0)
    {
        return 0;
    }
    pos += n;

    if (md->options_len > 0)
    {
        memcpy(&buf[pos], md->options, md->options_len);
        pos += md->options_len;
    }

    return pos;
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

    size_t n = cfdp_lv_deserialize(&buf[pos],
                                   buf_len - pos,
                                   &md->source_filename,
                                   &md->source_filename_len);
    if (n == 0)
    {
        return 0;
    }
    pos += n;

    n = cfdp_lv_deserialize(&buf[pos],
                            buf_len - pos,
                            &md->destination_filename,
                            &md->destination_filename_len);
    if (n == 0)
    {
        return 0;
    }
    pos += n;

    /* Whatever follows the file names is the option TLV chain (§5.2.5). */
    md->options = (buf_len > pos) ? &buf[pos] : NULL;
    md->options_len = (uint16_t)(buf_len - pos);

    return buf_len;
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

/**
 * @brief Decode the segment request array filling a NAK PDU's data field.
 *
 * @param[in]  buf     Data field, positioned at the first segment request.
 * @param[in]  buf_len Octets remaining in the data field.
 * @param[in]  fs      Octets per file-size-sensitive offset field.
 * @param[out] nak     NAK contents receiving the decoded requests.
 * @return Bytes consumed, or 0 if the requests do not fill @p buf_len exactly
 *         or there are more than ::CFDP_NAK_MAX_SEGMENT_REQUESTS of them.
 */
static size_t cfdp_nak_parse_segment_requests(const uint8_t *buf,
                                              size_t buf_len,
                                              uint8_t fs,
                                              cfdp_nak_pdu_t *nak)
{
    size_t pair = 2U * (size_t)fs;

    /* §5.2.6: the data field is a whole number of segment requests. A ragged
     * tail means the PDU is malformed, not that it ends early. Dropping
     * requests that do not fit would leave the sender believing it had
     * satisfied the NAK, so the missing ranges would never be retransmitted. */
    size_t count = buf_len / pair;
    if (((buf_len % pair) != 0) || (count > CFDP_NAK_MAX_SEGMENT_REQUESTS))
    {
        return 0;
    }

    size_t pos = 0;
    nak->segment_request_count = 0;
    for (size_t i = 0; i < count; i++)
    {
        nak->segment_requests[i].start_offset = cfdp_read_uint(&buf[pos], fs);
        pos += fs;
        nak->segment_requests[i].end_offset = cfdp_read_uint(&buf[pos], fs);
        pos += fs;
        nak->segment_request_count++;
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

    size_t n = cfdp_nak_parse_segment_requests(&buf[pos], buf_len - pos, fs, nak);
    if ((n == 0) && (buf_len != pos))
    {
        return 0;
    }

    return pos + n;
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
