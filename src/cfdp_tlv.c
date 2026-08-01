/**
 * @file    cfdp_tlv.c
 * @brief   CFDP LV and TLV parameter codecs
 *
 * Implements CCSDS 727.0-B-5 (CCSDS File Delivery Protocol), Section 5.1.8
 * (LV objects), Section 5.1.9 (TLV objects) and Section 5.4 (TLV parameters).
 * See also: docs/727x0b5e1.pdf
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#include "cfdp_tlv.h"

#include "cfdp_endian.h"

#include <string.h>

bool cfdp_filestore_action_has_second_filename(cfdp_filestore_action_t action_code)
{
    return (action_code == CFDP_FS_ACTION_RENAME_FILE) ||
           (action_code == CFDP_FS_ACTION_APPEND_FILE) ||
           (action_code == CFDP_FS_ACTION_REPLACE_FILE);
}

size_t cfdp_lv_serialize(const char *value, uint8_t value_len, uint8_t *buf, size_t buf_len)
{
    if ((!buf) || ((!value) && (value_len > 0)) ||
        (buf_len < (size_t)value_len + CFDP_LV_HEADER_LEN))
    {
        return 0;
    }

    buf[0] = value_len;
    if (value_len > 0)
    {
        memcpy(&buf[CFDP_LV_HEADER_LEN], value, value_len);
    }

    return (size_t)value_len + CFDP_LV_HEADER_LEN;
}

size_t cfdp_lv_deserialize(const uint8_t *buf,
                           size_t buf_len,
                           const char **value,
                           uint8_t *value_len)
{
    if ((!buf) || (!value) || (!value_len) || (buf_len < CFDP_LV_HEADER_LEN))
    {
        return 0;
    }

    uint8_t len = buf[0];
    if (buf_len < (size_t)len + CFDP_LV_HEADER_LEN)
    {
        return 0;
    }

    *value = (len > 0) ? (const char *)&buf[CFDP_LV_HEADER_LEN] : NULL;
    *value_len = len;

    return (size_t)len + CFDP_LV_HEADER_LEN;
}

size_t cfdp_tlv_serialize(const cfdp_tlv_t *tlv, uint8_t *buf, size_t buf_len)
{
    if ((!tlv) || (!buf) || ((!tlv->value) && (tlv->length > 0)) ||
        (buf_len < (size_t)tlv->length + CFDP_TLV_HEADER_LEN))
    {
        return 0;
    }

    buf[0] = tlv->type;
    buf[1] = tlv->length;
    if (tlv->length > 0)
    {
        memcpy(&buf[CFDP_TLV_HEADER_LEN], tlv->value, tlv->length);
    }

    return (size_t)tlv->length + CFDP_TLV_HEADER_LEN;
}

size_t cfdp_tlv_deserialize(const uint8_t *buf, size_t buf_len, cfdp_tlv_t *tlv)
{
    if ((!buf) || (!tlv) || (buf_len < CFDP_TLV_HEADER_LEN))
    {
        return 0;
    }

    uint8_t length = buf[1];
    if (buf_len < (size_t)length + CFDP_TLV_HEADER_LEN)
    {
        return 0;
    }

    tlv->type = buf[0];
    tlv->length = length;
    tlv->value = (length > 0) ? &buf[CFDP_TLV_HEADER_LEN] : NULL;

    return (size_t)length + CFDP_TLV_HEADER_LEN;
}

size_t cfdp_entity_id_tlv_serialize(uint64_t entity_id,
                                    uint8_t id_len,
                                    uint8_t *buf,
                                    size_t buf_len)
{
    if ((!buf) || (id_len < CFDP_ID_LEN_MIN) || (id_len > CFDP_ID_LEN_MAX) ||
        (buf_len < (size_t)id_len + CFDP_TLV_HEADER_LEN))
    {
        return 0;
    }

    buf[0] = (uint8_t)CFDP_TLV_ENTITY_ID;
    buf[1] = id_len;
    cfdp_write_uint(&buf[CFDP_TLV_HEADER_LEN], entity_id, id_len);

    return (size_t)id_len + CFDP_TLV_HEADER_LEN;
}

size_t cfdp_entity_id_tlv_deserialize(const uint8_t *buf,
                                      size_t buf_len,
                                      uint64_t *entity_id,
                                      uint8_t *id_len)
{
    cfdp_tlv_t tlv;
    if ((!entity_id) || (!id_len) || (cfdp_tlv_deserialize(buf, buf_len, &tlv) == 0) ||
        (tlv.type != (uint8_t)CFDP_TLV_ENTITY_ID) || (tlv.length < CFDP_ID_LEN_MIN) ||
        (tlv.length > CFDP_ID_LEN_MAX))
    {
        return 0;
    }

    *entity_id = cfdp_read_uint(tlv.value, tlv.length);
    *id_len = tlv.length;

    return (size_t)tlv.length + CFDP_TLV_HEADER_LEN;
}

size_t cfdp_fault_handler_tlv_serialize(cfdp_condition_code_t condition_code,
                                        cfdp_fault_handler_code_t handler_code,
                                        uint8_t *buf,
                                        size_t buf_len)
{
    if ((!buf) || (buf_len < CFDP_TLV_HEADER_LEN + 1U))
    {
        return 0;
    }

    buf[0] = (uint8_t)CFDP_TLV_FAULT_HANDLER_OVERRIDE;
    buf[1] = 1U;
    buf[2] = (uint8_t)((((uint8_t)condition_code & 0xFU) << 4) | ((uint8_t)handler_code & 0xFU));

    return CFDP_TLV_HEADER_LEN + 1U;
}

size_t cfdp_fault_handler_tlv_deserialize(const uint8_t *buf,
                                          size_t buf_len,
                                          cfdp_condition_code_t *condition_code,
                                          cfdp_fault_handler_code_t *handler_code)
{
    cfdp_tlv_t tlv;
    if ((!condition_code) || (!handler_code) || (cfdp_tlv_deserialize(buf, buf_len, &tlv) == 0) ||
        (tlv.type != (uint8_t)CFDP_TLV_FAULT_HANDLER_OVERRIDE) || (tlv.length != 1U))
    {
        return 0;
    }

    *condition_code = (cfdp_condition_code_t)((tlv.value[0] >> 4) & 0xFU);
    *handler_code = (cfdp_fault_handler_code_t)(tlv.value[0] & 0xFU);

    return CFDP_TLV_HEADER_LEN + 1U;
}

/**
 * @brief Write the one or two file name LVs shared by the filestore TLVs.
 *
 * @param[in]  first      First file name; may be NULL only when empty.
 * @param[in]  first_len  First file name length in octets.
 * @param[in]  second     Second file name; read only when @p has_second.
 * @param[in]  second_len Second file name length in octets.
 * @param[in]  has_second Whether the action code carries a second file name.
 * @param[out] buf        Output buffer positioned at the first name's LV.
 * @param[in]  buf_len    Buffer capacity in octets.
 * @return Bytes written, or 0 on error.
 */
static size_t cfdp_filestore_names_serialize(const char *first,
                                             uint8_t first_len,
                                             const char *second,
                                             uint8_t second_len,
                                             bool has_second,
                                             uint8_t *buf,
                                             size_t buf_len)
{
    size_t pos = cfdp_lv_serialize(first, first_len, buf, buf_len);
    if (pos == 0)
    {
        return 0;
    }

    if (has_second)
    {
        size_t n = cfdp_lv_serialize(second, second_len, &buf[pos], buf_len - pos);
        if (n == 0)
        {
            return 0;
        }
        pos += n;
    }

    return pos;
}

/**
 * @brief Read the one or two file name LVs shared by the filestore TLVs.
 *
 * @param[in]  buf        Input buffer positioned at the first name's LV.
 * @param[in]  buf_len    Octets available in @p buf.
 * @param[in]  has_second Whether the action code carries a second file name.
 * @param[out] first      Set to the first file name, which indexes into @p buf.
 * @param[out] first_len  Set to the first file name length.
 * @param[out] second     Set to the second file name when @p has_second.
 * @param[out] second_len Set to the second file name length when @p has_second.
 * @return Bytes consumed, or 0 on error.
 */
static size_t cfdp_filestore_names_deserialize(const uint8_t *buf,
                                               size_t buf_len,
                                               bool has_second,
                                               const char **first,
                                               uint8_t *first_len,
                                               const char **second,
                                               uint8_t *second_len)
{
    size_t pos = cfdp_lv_deserialize(buf, buf_len, first, first_len);
    if (pos == 0)
    {
        return 0;
    }

    if (has_second)
    {
        size_t n = cfdp_lv_deserialize(&buf[pos], buf_len - pos, second, second_len);
        if (n == 0)
        {
            return 0;
        }
        pos += n;
    }

    return pos;
}

/**
 * @brief Finish a filestore TLV by writing its type and value length fields.
 *
 * @param[out] buf  Buffer holding the already-written value.
 * @param[in]  type TLV type code.
 * @param[in]  end  Offset one past the last value octet.
 * @return @p end, or 0 when the value exceeds the 255-octet TLV length field.
 */
static size_t cfdp_filestore_tlv_finish(uint8_t *buf, cfdp_tlv_type_t type, size_t end)
{
    size_t value_len = end - CFDP_TLV_HEADER_LEN;
    if (value_len > UINT8_MAX)
    {
        return 0;
    }

    buf[0] = (uint8_t)type;
    buf[1] = (uint8_t)value_len;

    return end;
}

size_t cfdp_filestore_request_tlv_serialize(const cfdp_filestore_request_t *req,
                                            uint8_t *buf,
                                            size_t buf_len)
{
    if ((!req) || (!buf) || (buf_len < CFDP_TLV_HEADER_LEN + 1U))
    {
        return 0;
    }

    size_t pos = CFDP_TLV_HEADER_LEN;
    buf[pos] = (uint8_t)(((uint8_t)req->action_code & 0xFU) << 4);
    pos += 1U;

    size_t n =
        cfdp_filestore_names_serialize(req->first_filename,
                                       req->first_filename_len,
                                       req->second_filename,
                                       req->second_filename_len,
                                       cfdp_filestore_action_has_second_filename(req->action_code),
                                       &buf[pos],
                                       buf_len - pos);
    if (n == 0)
    {
        return 0;
    }

    return cfdp_filestore_tlv_finish(buf, CFDP_TLV_FILESTORE_REQUEST, pos + n);
}

size_t cfdp_filestore_request_tlv_deserialize(const uint8_t *buf,
                                              size_t buf_len,
                                              cfdp_filestore_request_t *req)
{
    cfdp_tlv_t tlv;
    if ((!req) || (cfdp_tlv_deserialize(buf, buf_len, &tlv) == 0) ||
        (tlv.type != (uint8_t)CFDP_TLV_FILESTORE_REQUEST) || (tlv.length < 1U))
    {
        return 0;
    }

    memset(req, 0, sizeof(*req));
    req->action_code = (cfdp_filestore_action_t)((tlv.value[0] >> 4) & 0xFU);

    size_t n = cfdp_filestore_names_deserialize(
        &tlv.value[1],
        (size_t)tlv.length - 1U,
        cfdp_filestore_action_has_second_filename(req->action_code),
        &req->first_filename,
        &req->first_filename_len,
        &req->second_filename,
        &req->second_filename_len);

    /* The names must account for the whole TLV value: no trailing octets. */
    if ((n == 0) || (n + 1U != (size_t)tlv.length))
    {
        return 0;
    }

    return (size_t)tlv.length + CFDP_TLV_HEADER_LEN;
}

size_t cfdp_filestore_response_tlv_serialize(const cfdp_filestore_response_t *resp,
                                             uint8_t *buf,
                                             size_t buf_len)
{
    if ((!resp) || (!buf) || (buf_len < CFDP_TLV_HEADER_LEN + 1U))
    {
        return 0;
    }

    size_t pos = CFDP_TLV_HEADER_LEN;
    buf[pos] =
        (uint8_t)((((uint8_t)resp->action_code & 0xFU) << 4) | ((uint8_t)resp->status_code & 0xFU));
    pos += 1U;

    size_t n =
        cfdp_filestore_names_serialize(resp->first_filename,
                                       resp->first_filename_len,
                                       resp->second_filename,
                                       resp->second_filename_len,
                                       cfdp_filestore_action_has_second_filename(resp->action_code),
                                       &buf[pos],
                                       buf_len - pos);
    if (n == 0)
    {
        return 0;
    }
    pos += n;

    n = cfdp_lv_serialize(resp->message, resp->message_len, &buf[pos], buf_len - pos);
    if (n == 0)
    {
        return 0;
    }

    return cfdp_filestore_tlv_finish(buf, CFDP_TLV_FILESTORE_RESPONSE, pos + n);
}

size_t cfdp_filestore_response_tlv_deserialize(const uint8_t *buf,
                                               size_t buf_len,
                                               cfdp_filestore_response_t *resp)
{
    cfdp_tlv_t tlv;
    if ((!resp) || (cfdp_tlv_deserialize(buf, buf_len, &tlv) == 0) ||
        (tlv.type != (uint8_t)CFDP_TLV_FILESTORE_RESPONSE) || (tlv.length < 1U))
    {
        return 0;
    }

    memset(resp, 0, sizeof(*resp));
    resp->action_code = (cfdp_filestore_action_t)((tlv.value[0] >> 4) & 0xFU);
    resp->status_code = (cfdp_filestore_status_t)(tlv.value[0] & 0xFU);

    size_t pos = cfdp_filestore_names_deserialize(
        &tlv.value[1],
        (size_t)tlv.length - 1U,
        cfdp_filestore_action_has_second_filename(resp->action_code),
        &resp->first_filename,
        &resp->first_filename_len,
        &resp->second_filename,
        &resp->second_filename_len);
    if (pos == 0)
    {
        return 0;
    }

    size_t n = cfdp_lv_deserialize(&tlv.value[1U + pos],
                                   (size_t)tlv.length - 1U - pos,
                                   &resp->message,
                                   &resp->message_len);

    /* Names plus message must account for the whole TLV value. */
    if ((n == 0) || (pos + n + 1U != (size_t)tlv.length))
    {
        return 0;
    }

    return (size_t)tlv.length + CFDP_TLV_HEADER_LEN;
}
