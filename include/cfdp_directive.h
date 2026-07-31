/**
 * @file    cfdp_directive.h
 * @brief   CFDP File Directive PDU codecs
 *
 * Serialises and deserialises the File Directive PDUs used to control a CFDP
 * file transfer: EOF, Finished, ACK, Metadata, NAK, Prompt and Keep Alive.
 * Implements CCSDS 727.0-B-5 (CCSDS File Delivery Protocol), Section 5.2 and
 * Section 5.4.
 *
 * @note Optional Type-Length-Value fields (fault location, filestore
 *       responses, filestore requests and message-to-user options) are not
 *       encoded or decoded by this basic implementation.
 * See also: docs/ccsds_cfdp.md
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#ifndef CFDP_DIRECTIVE_H
#define CFDP_DIRECTIVE_H

#include <stddef.h>
#include <stdint.h>

#include "cfdp_common.h"

/* -------------------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------------- */

/** @brief Maximum number of segment requests decoded from a NAK PDU. */
#define CFDP_NAK_MAX_SEGMENT_REQUESTS 32U

/* -------------------------------------------------------------------------
 * Types
 * ---------------------------------------------------------------------- */

/**
 * @brief End-of-File PDU contents (CCSDS 727.0-B-5 §5.2.2).
 *
 * @note The fault location TLV, present on a non-nominal condition code, is
 *       not encoded by this basic implementation.
 */
typedef struct
{
    cfdp_condition_code_t condition_code; /**< Condition at the sending entity. */
    uint32_t file_checksum;               /**< Modular checksum of the whole file. */
    uint64_t file_size;                   /**< Total file size in octets. */
} cfdp_eof_pdu_t;

/**
 * @brief Finished PDU contents (CCSDS 727.0-B-5 §5.2.3).
 *
 * @note Filestore responses and the fault location TLV are not encoded by
 *       this basic implementation.
 */
typedef struct
{
    cfdp_condition_code_t condition_code; /**< Condition at the receiving entity. */
    cfdp_delivery_code_t delivery_code;   /**< Data complete or incomplete. */
    cfdp_file_status_t file_status;       /**< Fate of the delivered file. */
} cfdp_finished_pdu_t;

/**
 * @brief Acknowledgement PDU contents (CCSDS 727.0-B-5 §5.2.4).
 */
typedef struct
{
    cfdp_directive_code_t ack_directive_code; /**< Directive being acknowledged (EOF/Finished). */
    uint8_t directive_subtype;                /**< Directive subtype code (4-bit field). */
    cfdp_condition_code_t condition_code;     /**< Condition code being acknowledged. */
    cfdp_transaction_status_t transaction_status; /**< Sender's view of the transaction. */
} cfdp_ack_pdu_t;

/**
 * @brief Metadata PDU contents (CCSDS 727.0-B-5 §5.2.5).
 *
 * @note @p source_filename and @p destination_filename point into
 *       caller-owned memory; the library neither copies nor frees them.
 *       Option TLVs are not encoded or decoded by this basic implementation.
 */
typedef struct
{
    bool closure_requested;              /**< Whether transaction closure is requested. */
    cfdp_checksum_type_t checksum_type;  /**< Checksum algorithm identifier. */
    uint64_t file_size;                  /**< Total file size in octets. */
    const char *source_filename;         /**< Source file name (may be NULL when empty). */
    uint8_t source_filename_len;         /**< Source file name length in octets. */
    const char *destination_filename;    /**< Destination file name (may be NULL when empty). */
    uint8_t destination_filename_len;    /**< Destination file name length in octets. */
} cfdp_metadata_pdu_t;

/**
 * @brief A single NAK segment request (CCSDS 727.0-B-5 §5.2.6).
 */
typedef struct
{
    uint64_t start_offset; /**< Offset of the first missing octet. */
    uint64_t end_offset;   /**< Offset one past the last missing octet. */
} cfdp_segment_request_t;

/**
 * @brief Negative Acknowledgement PDU contents (CCSDS 727.0-B-5 §5.2.6).
 *
 * @note On decode, at most ::CFDP_NAK_MAX_SEGMENT_REQUESTS requests are
 *       stored; @p segment_request_count reflects how many were kept.
 */
typedef struct
{
    uint64_t start_of_scope;          /**< Start offset of the reported scope. */
    uint64_t end_of_scope;            /**< End offset of the reported scope. */
    cfdp_segment_request_t segment_requests[CFDP_NAK_MAX_SEGMENT_REQUESTS]; /**< Missing ranges. */
    size_t segment_request_count;     /**< Number of valid entries in @p segment_requests. */
} cfdp_nak_pdu_t;

/* -------------------------------------------------------------------------
 * Function Declarations
 * ---------------------------------------------------------------------- */

/**
 * @brief Serialise an EOF PDU data field (directive code plus contents).
 *
 * @param[in]  eof             EOF contents to serialise.
 * @param[in]  large_file_flag Selects a 32- or 64-bit file size field.
 * @param[out] buf             Output buffer.
 * @param[in]  buf_len         Buffer capacity in octets.
 * @return Bytes written, or 0 on error (NULL args or buffer too small).
 */
size_t cfdp_eof_serialize(const cfdp_eof_pdu_t *eof,
                          cfdp_large_file_flag_t large_file_flag,
                          uint8_t *buf,
                          size_t buf_len);

/**
 * @brief Deserialise an EOF PDU data field.
 *
 * @param[in]  buf             Data field, positioned at the directive code.
 * @param[in]  buf_len         Length of the data field in octets.
 * @param[in]  large_file_flag Selects a 32- or 64-bit file size field.
 * @param[out] eof             Decoded EOF contents.
 * @return Bytes consumed, or 0 on error (NULL args, wrong directive code, or
 *         truncated input).
 */
size_t cfdp_eof_deserialize(const uint8_t *buf,
                            size_t buf_len,
                            cfdp_large_file_flag_t large_file_flag,
                            cfdp_eof_pdu_t *eof);

/**
 * @brief Serialise a Finished PDU data field.
 *
 * @param[in]  fin     Finished contents to serialise.
 * @param[out] buf     Output buffer.
 * @param[in]  buf_len Buffer capacity in octets.
 * @return Bytes written, or 0 on error.
 */
size_t cfdp_finished_serialize(const cfdp_finished_pdu_t *fin, uint8_t *buf, size_t buf_len);

/**
 * @brief Deserialise a Finished PDU data field.
 *
 * @param[in]  buf     Data field, positioned at the directive code.
 * @param[in]  buf_len Length of the data field in octets.
 * @param[out] fin     Decoded Finished contents.
 * @return Bytes consumed, or 0 on error.
 */
size_t cfdp_finished_deserialize(const uint8_t *buf, size_t buf_len, cfdp_finished_pdu_t *fin);

/**
 * @brief Serialise an ACK PDU data field.
 *
 * @param[in]  ack     ACK contents to serialise.
 * @param[out] buf     Output buffer.
 * @param[in]  buf_len Buffer capacity in octets.
 * @return Bytes written, or 0 on error.
 */
size_t cfdp_ack_serialize(const cfdp_ack_pdu_t *ack, uint8_t *buf, size_t buf_len);

/**
 * @brief Deserialise an ACK PDU data field.
 *
 * @param[in]  buf     Data field, positioned at the directive code.
 * @param[in]  buf_len Length of the data field in octets.
 * @param[out] ack     Decoded ACK contents.
 * @return Bytes consumed, or 0 on error.
 */
size_t cfdp_ack_deserialize(const uint8_t *buf, size_t buf_len, cfdp_ack_pdu_t *ack);

/**
 * @brief Serialise a Metadata PDU data field.
 *
 * @param[in]  md              Metadata contents to serialise.
 * @param[in]  large_file_flag Selects a 32- or 64-bit file size field.
 * @param[out] buf             Output buffer.
 * @param[in]  buf_len         Buffer capacity in octets.
 * @return Bytes written, or 0 on error.
 */
size_t cfdp_metadata_serialize(const cfdp_metadata_pdu_t *md,
                               cfdp_large_file_flag_t large_file_flag,
                               uint8_t *buf,
                               size_t buf_len);

/**
 * @brief Deserialise a Metadata PDU data field.
 *
 * @param[in]  buf             Data field, positioned at the directive code.
 * @param[in]  buf_len         Length of the data field in octets.
 * @param[in]  large_file_flag Selects a 32- or 64-bit file size field.
 * @param[out] md              Decoded contents; file name pointers index into @p buf.
 * @return Bytes consumed, or 0 on error.
 */
size_t cfdp_metadata_deserialize(const uint8_t *buf,
                                 size_t buf_len,
                                 cfdp_large_file_flag_t large_file_flag,
                                 cfdp_metadata_pdu_t *md);

/**
 * @brief Serialise a NAK PDU data field.
 *
 * @param[in]  nak             NAK contents to serialise.
 * @param[in]  large_file_flag Selects 32- or 64-bit offset fields.
 * @param[out] buf             Output buffer.
 * @param[in]  buf_len         Buffer capacity in octets.
 * @return Bytes written, or 0 on error (including too many segment requests).
 */
size_t cfdp_nak_serialize(const cfdp_nak_pdu_t *nak,
                          cfdp_large_file_flag_t large_file_flag,
                          uint8_t *buf,
                          size_t buf_len);

/**
 * @brief Deserialise a NAK PDU data field.
 *
 * @param[in]  buf             Data field, positioned at the directive code.
 * @param[in]  buf_len         Length of the data field in octets.
 * @param[in]  large_file_flag Selects 32- or 64-bit offset fields.
 * @param[out] nak             Decoded contents.
 * @return Bytes consumed, or 0 on error.
 */
size_t cfdp_nak_deserialize(const uint8_t *buf,
                            size_t buf_len,
                            cfdp_large_file_flag_t large_file_flag,
                            cfdp_nak_pdu_t *nak);

/**
 * @brief Serialise a Prompt PDU data field.
 *
 * @param[in]  response Prompt response type (NAK or Keep Alive).
 * @param[out] buf      Output buffer.
 * @param[in]  buf_len  Buffer capacity in octets.
 * @return Bytes written, or 0 on error.
 */
size_t cfdp_prompt_serialize(cfdp_prompt_response_t response, uint8_t *buf, size_t buf_len);

/**
 * @brief Deserialise a Prompt PDU data field.
 *
 * @param[in]  buf      Data field, positioned at the directive code.
 * @param[in]  buf_len  Length of the data field in octets.
 * @param[out] response Decoded prompt response type.
 * @return Bytes consumed, or 0 on error.
 */
size_t cfdp_prompt_deserialize(const uint8_t *buf, size_t buf_len, cfdp_prompt_response_t *response);

/**
 * @brief Serialise a Keep Alive PDU data field.
 *
 * @param[in]  progress        Receiver's reported file progress in octets.
 * @param[in]  large_file_flag Selects a 32- or 64-bit progress field.
 * @param[out] buf             Output buffer.
 * @param[in]  buf_len         Buffer capacity in octets.
 * @return Bytes written, or 0 on error.
 */
size_t cfdp_keep_alive_serialize(uint64_t progress,
                                 cfdp_large_file_flag_t large_file_flag,
                                 uint8_t *buf,
                                 size_t buf_len);

/**
 * @brief Deserialise a Keep Alive PDU data field.
 *
 * @param[in]  buf             Data field, positioned at the directive code.
 * @param[in]  buf_len         Length of the data field in octets.
 * @param[in]  large_file_flag Selects a 32- or 64-bit progress field.
 * @param[out] progress        Decoded file progress in octets.
 * @return Bytes consumed, or 0 on error.
 */
size_t cfdp_keep_alive_deserialize(const uint8_t *buf,
                                   size_t buf_len,
                                   cfdp_large_file_flag_t large_file_flag,
                                   uint64_t *progress);

#endif /* CFDP_DIRECTIVE_H */
