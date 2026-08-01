/**
 * @file    cfdp_pdu.h
 * @brief   CFDP PDU fixed header and File Data PDU codec
 *
 * Serialises and deserialises the fixed PDU header shared by every CFDP PDU
 * and the File Data PDU payload.
 * Implements CCSDS 727.0-B-5 (CCSDS File Delivery Protocol), Section 5.1
 * (fixed PDU header) and Section 5.3 (File Data PDU).
 * See also: docs/727x0b5e1.pdf
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#ifndef CFDP_PDU_H
#define CFDP_PDU_H

#include "cfdp_common.h"

#include <stddef.h>
#include <stdint.h>

/* -------------------------------------------------------------------------
 * Types
 * ---------------------------------------------------------------------- */

/**
 * @brief Fixed PDU header shared by every CFDP PDU (CCSDS 727.0-B-5 §5.1).
 *
 * @note @p entity_id_length and @p transaction_seq_length hold the actual
 *       octet counts (1..8), not the (count-1) form used on the wire.
 */
typedef struct
{
    uint8_t version;                                /**< Protocol version (3-bit field). */
    cfdp_pdu_type_t pdu_type;                       /**< Directive or File Data. */
    cfdp_direction_t direction;                     /**< Toward receiver or sender. */
    cfdp_transmission_mode_t transmission_mode;     /**< Acknowledged or unacknowledged. */
    cfdp_crc_flag_t crc_flag;                       /**< Whether a CRC trails the PDU. */
    cfdp_large_file_flag_t large_file_flag;         /**< Selects 32- or 64-bit file fields. */
    uint16_t data_field_length;                     /**< PDU data field length in octets. */
    cfdp_seg_ctrl_t segmentation_control;           /**< Record boundary preservation. */
    cfdp_seg_metadata_flag_t segment_metadata_flag; /**< Segment metadata present flag. */
    uint8_t entity_id_length;                       /**< Entity ID length in octets (1..8). */
    uint8_t transaction_seq_length;  /**< Transaction sequence length in octets (1..8). */
    uint64_t source_entity_id;       /**< Source entity ID. */
    uint64_t transaction_seq_number; /**< Transaction sequence number. */
    uint64_t destination_entity_id;  /**< Destination entity ID. */
} cfdp_pdu_header_t;

/**
 * @brief File Data PDU payload (CCSDS 727.0-B-5 §5.3).
 *
 * @note @p file_data points into caller-owned memory; the library neither
 *       copies nor frees it. Segment metadata is not supported: the header's
 *       segment metadata flag must be CFDP_SEG_METADATA_ABSENT.
 */
typedef struct
{
    uint64_t offset;          /**< Offset of this segment within the file, in octets. */
    const uint8_t *file_data; /**< File data octets. */
    size_t file_data_len;     /**< Number of file data octets. */
} cfdp_file_data_pdu_t;

/* -------------------------------------------------------------------------
 * Function Declarations
 * ---------------------------------------------------------------------- */

/**
 * @brief Total on-wire size of the header described by @p hdr.
 *
 * @param[in] hdr Header whose identifier lengths determine the size.
 * @return Header size in octets, or 0 if @p hdr is NULL or its identifier
 *         lengths are out of the 1..8 range.
 */
size_t cfdp_pdu_header_size(const cfdp_pdu_header_t *hdr);

/**
 * @brief Serialise a fixed PDU header into a caller-supplied buffer.
 *
 * The caller is responsible for setting @p hdr->data_field_length to the
 * length of the payload that will follow the header.
 *
 * @param[in]  hdr     Header to serialise.
 * @param[out] buf     Output buffer.
 * @param[in]  buf_len Buffer capacity in octets.
 * @return Bytes written, or 0 on error (NULL args, bad identifier lengths,
 *         or buffer too small).
 */
size_t cfdp_pdu_header_serialize(const cfdp_pdu_header_t *hdr, uint8_t *buf, size_t buf_len);

/**
 * @brief Deserialise a fixed PDU header from a buffer.
 *
 * @param[in]  buf     Input buffer positioned at the start of the header.
 * @param[in]  buf_len Number of octets available in @p buf.
 * @param[out] hdr     Decoded header.
 * @return Header size in octets consumed, or 0 on error (NULL args or
 *         truncated header).
 */
size_t cfdp_pdu_header_deserialize(const uint8_t *buf, size_t buf_len, cfdp_pdu_header_t *hdr);

/**
 * @brief Serialise a File Data PDU payload (offset plus file data).
 *
 * Writes the data field only; serialise the fixed header separately.
 *
 * @param[in]  fd              File Data payload to serialise.
 * @param[in]  large_file_flag Selects a 32- or 64-bit offset field.
 * @param[out] buf             Output buffer.
 * @param[in]  buf_len         Buffer capacity in octets.
 * @return Bytes written, or 0 on error (NULL args or buffer too small).
 */
size_t cfdp_file_data_serialize(const cfdp_file_data_pdu_t *fd,
                                cfdp_large_file_flag_t large_file_flag,
                                uint8_t *buf,
                                size_t buf_len);

/**
 * @brief Deserialise a File Data PDU payload from a data-field slice.
 *
 * @param[in]  buf             Data field, positioned at the segment offset.
 * @param[in]  buf_len         Length of the data field in octets.
 * @param[in]  large_file_flag Selects a 32- or 64-bit offset field.
 * @param[out] fd              Decoded payload; @p fd->file_data points into @p buf.
 * @return Bytes consumed (equal to @p buf_len), or 0 on error.
 */
size_t cfdp_file_data_deserialize(const uint8_t *buf,
                                  size_t buf_len,
                                  cfdp_large_file_flag_t large_file_flag,
                                  cfdp_file_data_pdu_t *fd);

/**
 * @brief Peek the directive code of a File Directive PDU data field.
 *
 * @param[in]  buf  Data field, positioned at the directive code octet.
 * @param[in]  buf_len Length of the data field in octets.
 * @param[out] code Decoded directive code.
 * @return true on success, false if @p buf is NULL or @p buf_len is 0.
 */
bool cfdp_pdu_directive_code(const uint8_t *buf, size_t buf_len, cfdp_directive_code_t *code);

#endif /* CFDP_PDU_H */
