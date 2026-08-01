/**
 * @file    cfdp_tlv.h
 * @brief   CFDP LV and TLV parameter codecs
 *
 * Serialises and deserialises the Length-Value and Type-Length-Value objects
 * carried by the File Directive PDUs: filestore requests and responses,
 * messages to user, fault handler overrides, flow labels and entity IDs.
 * Implements CCSDS 727.0-B-5 (CCSDS File Delivery Protocol), Section 5.1.8,
 * Section 5.1.9 and Section 5.4.
 * See also: docs/727x0b5e1.pdf
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#ifndef CFDP_TLV_H
#define CFDP_TLV_H

#include "cfdp_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* -------------------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------------- */

/** @brief Octets preceding the value of an LV object: the length field. */
#define CFDP_LV_HEADER_LEN 1U

/** @brief Octets preceding the value of a TLV object: the type and length fields. */
#define CFDP_TLV_HEADER_LEN 2U

/* -------------------------------------------------------------------------
 * Types
 * ---------------------------------------------------------------------- */

/**
 * @brief TLV type codes (CCSDS 727.0-B-5 §5.4).
 *
 * Enum values equal the 1-octet Type field on the wire.
 */
typedef enum
{
    CFDP_TLV_FILESTORE_REQUEST = 0x00,      /**< Filestore request (§5.4.1). */
    CFDP_TLV_FILESTORE_RESPONSE = 0x01,     /**< Filestore response (§5.4.2). */
    CFDP_TLV_MESSAGE_TO_USER = 0x02,        /**< Message to user, opaque value (§5.4.3). */
    CFDP_TLV_FAULT_HANDLER_OVERRIDE = 0x04, /**< Fault handler override (§5.4.4). */
    CFDP_TLV_FLOW_LABEL = 0x05,             /**< Flow label, opaque value (§5.4.5). */
    CFDP_TLV_ENTITY_ID = 0x06               /**< Entity ID, used as fault location (§5.4.6). */
} cfdp_tlv_type_t;

/**
 * @brief Filestore request action codes (CCSDS 727.0-B-5 §5.4.1, table 5-16).
 *
 * Enum values equal the 4-bit action code on the wire.
 */
typedef enum
{
    CFDP_FS_ACTION_CREATE_FILE = 0x0,      /**< Create the named file. */
    CFDP_FS_ACTION_DELETE_FILE = 0x1,      /**< Delete the named file. */
    CFDP_FS_ACTION_RENAME_FILE = 0x2,      /**< Rename first file to second. */
    CFDP_FS_ACTION_APPEND_FILE = 0x3,      /**< Append second file to first. */
    CFDP_FS_ACTION_REPLACE_FILE = 0x4,     /**< Replace first file's contents with second's. */
    CFDP_FS_ACTION_CREATE_DIRECTORY = 0x5, /**< Create the named directory. */
    CFDP_FS_ACTION_REMOVE_DIRECTORY = 0x6, /**< Remove the named directory. */
    CFDP_FS_ACTION_DENY_FILE = 0x7,        /**< Delete the named file if present. */
    CFDP_FS_ACTION_DENY_DIRECTORY = 0x8    /**< Remove the named directory if present. */
} cfdp_filestore_action_t;

/**
 * @brief Filestore response status codes (CCSDS 727.0-B-5 §5.4.2, table 5-18).
 *
 * @note Codes 0x1..0x3 are action specific — for example 0x1 means 'file does
 *       not exist' for a Delete but 'create not allowed' for a Create. See
 *       table 5-18 for the meaning under each action code.
 */
typedef enum
{
    CFDP_FS_STATUS_SUCCESSFUL = 0x0,   /**< Action performed successfully. */
    CFDP_FS_STATUS_ERROR_1 = 0x1,      /**< First action-specific failure. */
    CFDP_FS_STATUS_ERROR_2 = 0x2,      /**< Second action-specific failure. */
    CFDP_FS_STATUS_ERROR_3 = 0x3,      /**< Third action-specific failure. */
    CFDP_FS_STATUS_NOT_PERFORMED = 0xF /**< Action not performed. */
} cfdp_filestore_status_t;

/**
 * @brief Fault handler codes (CCSDS 727.0-B-5 §5.4.4, table 5-19).
 *
 * Enum values equal the 4-bit handler code on the wire.
 */
typedef enum
{
    CFDP_HANDLER_RESERVED = 0x0,               /**< Reserved for future expansion. */
    CFDP_HANDLER_NOTICE_OF_CANCELLATION = 0x1, /**< Issue a Notice of Cancellation. */
    CFDP_HANDLER_NOTICE_OF_SUSPENSION = 0x2,   /**< Issue a Notice of Suspension. */
    CFDP_HANDLER_IGNORE_ERROR = 0x3,           /**< Ignore the error. */
    CFDP_HANDLER_ABANDON_TRANSACTION = 0x4     /**< Abandon the transaction. */
} cfdp_fault_handler_code_t;

/**
 * @brief A generic TLV object (CCSDS 727.0-B-5 §5.1.9).
 *
 * @note @p value points into caller-owned memory; the library neither copies
 *       nor frees it.
 */
typedef struct
{
    uint8_t type;         /**< Type field; one of ::cfdp_tlv_type_t. */
    uint8_t length;       /**< Value length in octets. */
    const uint8_t *value; /**< Value octets, or NULL when @p length is zero. */
} cfdp_tlv_t;

/**
 * @brief Filestore Request TLV contents (CCSDS 727.0-B-5 §5.4.1, table 5-15).
 *
 * @note Both file names point into caller-owned memory. The second file name
 *       is present only for the action codes listed in table 5-16; see
 *       ::cfdp_filestore_action_has_second_filename.
 */
typedef struct
{
    cfdp_filestore_action_t action_code; /**< Filestore action to perform. */
    const char *first_filename;          /**< First file name (may be NULL when empty). */
    uint8_t first_filename_len;          /**< First file name length in octets. */
    const char *second_filename;         /**< Second file name (may be NULL when empty). */
    uint8_t second_filename_len;         /**< Second file name length in octets. */
} cfdp_filestore_request_t;

/**
 * @brief Filestore Response TLV contents (CCSDS 727.0-B-5 §5.4.2, table 5-17).
 *
 * @note All three values point into caller-owned memory.
 */
typedef struct
{
    cfdp_filestore_action_t action_code; /**< Action the response reports on. */
    cfdp_filestore_status_t status_code; /**< Outcome of the action. */
    const char *first_filename;          /**< First file name (may be NULL when empty). */
    uint8_t first_filename_len;          /**< First file name length in octets. */
    const char *second_filename;         /**< Second file name (may be NULL when empty). */
    uint8_t second_filename_len;         /**< Second file name length in octets. */
    const char *message;                 /**< Implementation-specific message (may be NULL). */
    uint8_t message_len;                 /**< Message length in octets. */
} cfdp_filestore_response_t;

/* -------------------------------------------------------------------------
 * Function Declarations
 * ---------------------------------------------------------------------- */

/**
 * @brief Whether an action code carries a second file name (table 5-16).
 *
 * @param[in] action_code Filestore action code.
 * @return true for Rename, Append and Replace; false for every other action.
 */
bool cfdp_filestore_action_has_second_filename(cfdp_filestore_action_t action_code);

/**
 * @brief Serialise an LV object: a 1-octet length followed by the value.
 *
 * @param[in]  value     Value octets; may be NULL only when @p value_len is 0.
 * @param[in]  value_len Value length in octets.
 * @param[out] buf       Output buffer.
 * @param[in]  buf_len   Buffer capacity in octets.
 * @return Bytes written, or 0 on error (missing value or buffer too small).
 */
size_t cfdp_lv_serialize(const char *value, uint8_t value_len, uint8_t *buf, size_t buf_len);

/**
 * @brief Deserialise an LV object, pointing @p value into @p buf.
 *
 * @param[in]  buf       Input buffer positioned at the length octet.
 * @param[in]  buf_len   Octets available in @p buf.
 * @param[out] value     Set to the value octets, or NULL when the value is empty.
 * @param[out] value_len Set to the value length in octets.
 * @return Bytes consumed, or 0 on error (NULL args or truncated input).
 */
size_t cfdp_lv_deserialize(const uint8_t *buf,
                           size_t buf_len,
                           const char **value,
                           uint8_t *value_len);

/**
 * @brief Serialise a TLV object: type, length, then the value.
 *
 * @param[in]  tlv     TLV to serialise; @p value may be NULL only when the
 *                     length is 0.
 * @param[out] buf     Output buffer.
 * @param[in]  buf_len Buffer capacity in octets.
 * @return Bytes written, or 0 on error.
 */
size_t cfdp_tlv_serialize(const cfdp_tlv_t *tlv, uint8_t *buf, size_t buf_len);

/**
 * @brief Deserialise a TLV object, pointing @p tlv->value into @p buf.
 *
 * Successive calls advance through a chain of TLVs by the returned length.
 *
 * @param[in]  buf     Input buffer positioned at the type octet.
 * @param[in]  buf_len Octets available in @p buf.
 * @param[out] tlv     Decoded TLV.
 * @return Bytes consumed, or 0 on error.
 */
size_t cfdp_tlv_deserialize(const uint8_t *buf, size_t buf_len, cfdp_tlv_t *tlv);

/**
 * @brief Serialise an Entity ID TLV, used as a Fault Location (§5.4.6).
 *
 * @param[in]  entity_id Entity at which transaction cancellation was initiated.
 * @param[in]  id_len    Octets used to encode @p entity_id (1..8).
 * @param[out] buf       Output buffer.
 * @param[in]  buf_len   Buffer capacity in octets.
 * @return Bytes written, or 0 on error (bad length or buffer too small).
 */
size_t cfdp_entity_id_tlv_serialize(uint64_t entity_id,
                                    uint8_t id_len,
                                    uint8_t *buf,
                                    size_t buf_len);

/**
 * @brief Deserialise an Entity ID TLV (§5.4.6).
 *
 * @param[in]  buf       Input buffer positioned at the type octet.
 * @param[in]  buf_len   Octets available in @p buf.
 * @param[out] entity_id Decoded entity ID.
 * @param[out] id_len    Octets the entity ID occupied on the wire.
 * @return Bytes consumed, or 0 on error (wrong type or bad length).
 */
size_t cfdp_entity_id_tlv_deserialize(const uint8_t *buf,
                                      size_t buf_len,
                                      uint64_t *entity_id,
                                      uint8_t *id_len);

/**
 * @brief Serialise a Fault Handler Override TLV (§5.4.4).
 *
 * @param[in]  condition_code Condition the override applies to.
 * @param[in]  handler_code   Handler to apply for that condition.
 * @param[out] buf            Output buffer.
 * @param[in]  buf_len        Buffer capacity in octets.
 * @return Bytes written, or 0 on error.
 */
size_t cfdp_fault_handler_tlv_serialize(cfdp_condition_code_t condition_code,
                                        cfdp_fault_handler_code_t handler_code,
                                        uint8_t *buf,
                                        size_t buf_len);

/**
 * @brief Deserialise a Fault Handler Override TLV (§5.4.4).
 *
 * @param[in]  buf            Input buffer positioned at the type octet.
 * @param[in]  buf_len        Octets available in @p buf.
 * @param[out] condition_code Decoded condition code.
 * @param[out] handler_code   Decoded handler code.
 * @return Bytes consumed, or 0 on error.
 */
size_t cfdp_fault_handler_tlv_deserialize(const uint8_t *buf,
                                          size_t buf_len,
                                          cfdp_condition_code_t *condition_code,
                                          cfdp_fault_handler_code_t *handler_code);

/**
 * @brief Serialise a Filestore Request TLV (§5.4.1).
 *
 * @param[in]  req     Request contents; the second file name is written only
 *                     for the action codes that carry one.
 * @param[out] buf     Output buffer.
 * @param[in]  buf_len Buffer capacity in octets.
 * @return Bytes written, or 0 on error.
 */
size_t cfdp_filestore_request_tlv_serialize(const cfdp_filestore_request_t *req,
                                            uint8_t *buf,
                                            size_t buf_len);

/**
 * @brief Deserialise a Filestore Request TLV (§5.4.1).
 *
 * @param[in]  buf     Input buffer positioned at the type octet.
 * @param[in]  buf_len Octets available in @p buf.
 * @param[out] req     Decoded request; file names index into @p buf.
 * @return Bytes consumed, or 0 on error.
 */
size_t cfdp_filestore_request_tlv_deserialize(const uint8_t *buf,
                                              size_t buf_len,
                                              cfdp_filestore_request_t *req);

/**
 * @brief Serialise a Filestore Response TLV (§5.4.2).
 *
 * @param[in]  resp    Response contents; the second file name is written only
 *                     for the action codes that carry one.
 * @param[out] buf     Output buffer.
 * @param[in]  buf_len Buffer capacity in octets.
 * @return Bytes written, or 0 on error.
 */
size_t cfdp_filestore_response_tlv_serialize(const cfdp_filestore_response_t *resp,
                                             uint8_t *buf,
                                             size_t buf_len);

/**
 * @brief Deserialise a Filestore Response TLV (§5.4.2).
 *
 * @param[in]  buf     Input buffer positioned at the type octet.
 * @param[in]  buf_len Octets available in @p buf.
 * @param[out] resp    Decoded response; file names and message index into @p buf.
 * @return Bytes consumed, or 0 on error.
 */
size_t cfdp_filestore_response_tlv_deserialize(const uint8_t *buf,
                                               size_t buf_len,
                                               cfdp_filestore_response_t *resp);

#endif /* CFDP_TLV_H */
