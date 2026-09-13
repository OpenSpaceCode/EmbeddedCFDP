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
 * @note @p segmentation_control and @p segment_metadata_flag apply to File Data
 *       PDUs only. For a File Directive PDU they are written as '0' whatever
 *       their value here, and decoded as '0' whatever the wire carries
 *       (table 5-1: "Always '0' (and ignored) for File Directive PDUs").
 */
typedef struct
{
    uint8_t version;                                /**< Must be ::CFDP_PROTOCOL_VERSION. */
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
 * @brief File Data PDU payload (CCSDS 727.0-B-5 §5.3, table 5-14).
 *
 * @note @p file_data and @p segment_metadata point into caller-owned memory;
 *       the library neither copies nor frees them.
 * @note @p record_continuation, @p segment_metadata and @p segment_metadata_len
 *       are carried on the wire only when the PDU header's Segment Metadata
 *       flag is ::CFDP_SEG_METADATA_PRESENT, and must be left zero/NULL when it
 *       is not — the codecs reject a payload that disagrees with the flag.
 */
typedef struct
{
    uint64_t offset;          /**< Offset of this segment within the file, in octets. */
    const uint8_t *file_data; /**< File data octets. */
    size_t file_data_len;     /**< Number of file data octets. */
    cfdp_record_continuation_t record_continuation; /**< Record boundaries within this segment. */
    const uint8_t *segment_metadata;                /**< Segment metadata octets, or NULL. */
    uint8_t segment_metadata_len;                   /**< Segment metadata length, 0..63 octets. */
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
 * @return Bytes written, or 0 on error (NULL args, a version other than
 *         ::CFDP_PROTOCOL_VERSION, bad identifier lengths, or buffer too small).
 */
size_t cfdp_pdu_header_serialize(const cfdp_pdu_header_t *hdr, uint8_t *buf, size_t buf_len);

/**
 * @brief Deserialise a fixed PDU header from a buffer.
 *
 * @param[in]  buf     Input buffer positioned at the start of the header.
 * @param[in]  buf_len Number of octets available in @p buf.
 * @param[out] hdr     Decoded header; may be partly written on error.
 * @return Header size in octets consumed, or 0 on error (NULL args, a version
 *         other than ::CFDP_PROTOCOL_VERSION, or truncated header).
 */
size_t cfdp_pdu_header_deserialize(const uint8_t *buf, size_t buf_len, cfdp_pdu_header_t *hdr);

/**
 * @brief Length of a received PDU's payload, excluding any trailing CRC.
 *
 * §4.1.3.2 places the 16-bit CRC in the final octets of the PDU data field and
 * counts it in the data field length, so @p hdr->data_field_length is not the
 * payload length when the CRC flag is set. Pass the result of this function,
 * not @p hdr->data_field_length, to the payload deserialisers; feeding them the
 * raw data field length would decode the CRC octets as file data or as part of
 * a TLV chain.
 *
 * @param[in] hdr Decoded header of the received PDU.
 * @return Payload length in octets, or 0 if @p hdr is NULL or the data field is
 *         too short to hold the CRC its flag claims. Every valid CFDP PDU has a
 *         non-empty payload, so 0 is unambiguously an error.
 */
size_t cfdp_pdu_payload_size(const cfdp_pdu_header_t *hdr);

/**
 * @brief Serialise a File Data PDU payload (§5.3, table 5-14).
 *
 * Writes the data field only; serialise the fixed header separately. When
 * @p segment_metadata_flag is ::CFDP_SEG_METADATA_PRESENT the record
 * continuation state, segment metadata length and segment metadata precede the
 * offset; otherwise the data field starts at the offset.
 *
 * @param[in]  fd                    File Data payload to serialise.
 * @param[in]  large_file_flag       Selects a 32- or 64-bit offset field.
 * @param[in]  segment_metadata_flag Segment Metadata flag of the PDU header;
 *                                   must match the header actually sent.
 * @param[out] buf                   Output buffer.
 * @param[in]  buf_len               Buffer capacity in octets.
 * @return Bytes written, or 0 on error (NULL args, buffer too small, segment
 *         metadata longer than ::CFDP_SEGMENT_METADATA_MAX_LEN, or segment
 *         metadata supplied while @p segment_metadata_flag is absent).
 */
size_t cfdp_file_data_serialize(const cfdp_file_data_pdu_t *fd,
                                cfdp_large_file_flag_t large_file_flag,
                                cfdp_seg_metadata_flag_t segment_metadata_flag,
                                uint8_t *buf,
                                size_t buf_len);

/**
 * @brief Deserialise a File Data PDU payload from a data-field slice (§5.3).
 *
 * @param[in]  buf                   Data field, positioned at its first octet.
 * @param[in]  buf_len               Payload length in octets — use
 *                                   cfdp_pdu_payload_size(), not the header's
 *                                   raw data field length, or a trailing CRC
 *                                   would be decoded as file data.
 * @param[in]  large_file_flag       Selects a 32- or 64-bit offset field.
 * @param[in]  segment_metadata_flag Segment Metadata flag from the PDU header;
 *                                   selects the data field layout.
 * @param[out] fd                    Decoded payload; @p fd->file_data and
 *                                   @p fd->segment_metadata point into @p buf.
 * @return Bytes consumed (equal to @p buf_len), or 0 on error (NULL args or a
 *         data field too short for the layout the flags describe).
 */
size_t cfdp_file_data_deserialize(const uint8_t *buf,
                                  size_t buf_len,
                                  cfdp_large_file_flag_t large_file_flag,
                                  cfdp_seg_metadata_flag_t segment_metadata_flag,
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
