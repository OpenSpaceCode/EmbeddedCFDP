/**
 * @file    cfdp_common.h
 * @brief   Common CFDP constants, enumerations and shared helpers
 *
 * Defines the protocol-level enumerations (PDU types, directive codes,
 * condition codes, etc.) shared by every CFDP module.
 * Implements the field encodings of CCSDS 727.0-B-5 (CCSDS File Delivery
 * Protocol), Section 5.
 * See also: docs/ccsds_cfdp.md
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#ifndef CFDP_COMMON_H
#define CFDP_COMMON_H

#include <stdbool.h>
#include <stdint.h>

/* -------------------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------------- */

/** @brief CFDP protocol version carried in the PDU header (CCSDS 727.0-B-5 §5.1.2). */
#define CFDP_PROTOCOL_VERSION 1U

/** @brief Minimum length in octets of an entity ID or transaction sequence number. */
#define CFDP_ID_LEN_MIN 1U

/** @brief Maximum length in octets of an entity ID or transaction sequence number. */
#define CFDP_ID_LEN_MAX 8U

/** @brief Size of the fixed part of the PDU header, before the variable-length IDs. */
#define CFDP_PDU_HEADER_FIXED_LEN 4U

/** @brief Largest possible PDU header: fixed part plus three 8-octet identifier fields. */
#define CFDP_PDU_HEADER_MAX_LEN (CFDP_PDU_HEADER_FIXED_LEN + 3U * CFDP_ID_LEN_MAX)

/* -------------------------------------------------------------------------
 * Types
 * ---------------------------------------------------------------------- */

/**
 * @brief PDU Type flag (CCSDS 727.0-B-5 §5.1.2).
 *
 * Enum values equal the 1-bit wire pattern directly.
 */
typedef enum
{
    CFDP_PDU_TYPE_DIRECTIVE = 0, /**< PDU carries a File Directive. */
    CFDP_PDU_TYPE_FILE_DATA = 1  /**< PDU carries File Data. */
} cfdp_pdu_type_t;

/**
 * @brief Direction flag (CCSDS 727.0-B-5 §5.1.2).
 *
 * Enum values equal the 1-bit wire pattern directly.
 */
typedef enum
{
    CFDP_DIRECTION_TOWARD_RECEIVER = 0, /**< PDU travels toward the file receiver. */
    CFDP_DIRECTION_TOWARD_SENDER = 1    /**< PDU travels toward the file sender. */
} cfdp_direction_t;

/**
 * @brief Transmission Mode flag (CCSDS 727.0-B-5 §5.1.2).
 *
 * Enum values equal the 1-bit wire pattern directly.
 */
typedef enum
{
    CFDP_TRANS_MODE_ACKNOWLEDGED = 0,  /**< Class 2: reliable, acknowledged transfer. */
    CFDP_TRANS_MODE_UNACKNOWLEDGED = 1 /**< Class 1: unreliable, unacknowledged transfer. */
} cfdp_transmission_mode_t;

/**
 * @brief CRC flag (CCSDS 727.0-B-5 §5.1.2).
 *
 * Enum values equal the 1-bit wire pattern directly.
 */
typedef enum
{
    CFDP_CRC_ABSENT = 0, /**< No 16-bit CRC trails the PDU data field. */
    CFDP_CRC_PRESENT = 1 /**< A 16-bit CRC trails the PDU data field. */
} cfdp_crc_flag_t;

/**
 * @brief Large File flag (CCSDS 727.0-B-5 §5.1.2).
 *
 * Selects the width of file offsets and sizes on the wire.
 */
typedef enum
{
    CFDP_FILE_SIZE_SMALL = 0, /**< Offsets and sizes are 32-bit (4 octets). */
    CFDP_FILE_SIZE_LARGE = 1  /**< Offsets and sizes are 64-bit (8 octets). */
} cfdp_large_file_flag_t;

/**
 * @brief Segmentation Control flag (CCSDS 727.0-B-5 §5.1.2).
 *
 * Enum values equal the 1-bit wire pattern directly.
 */
typedef enum
{
    CFDP_SEG_CTRL_BOUNDARIES_NOT_PRESERVED = 0, /**< Record boundaries are not preserved. */
    CFDP_SEG_CTRL_BOUNDARIES_PRESERVED = 1      /**< Record boundaries are preserved. */
} cfdp_seg_ctrl_t;

/**
 * @brief Segment Metadata flag (CCSDS 727.0-B-5 §5.1.2).
 *
 * Enum values equal the 1-bit wire pattern directly.
 */
typedef enum
{
    CFDP_SEG_METADATA_ABSENT = 0, /**< File Data PDUs carry no segment metadata. */
    CFDP_SEG_METADATA_PRESENT = 1 /**< File Data PDUs carry segment metadata. */
} cfdp_seg_metadata_flag_t;

/**
 * @brief File Directive codes (CCSDS 727.0-B-5 §5.4, Table 5-4).
 *
 * Enum values equal the 1-octet directive code on the wire.
 */
typedef enum
{
    CFDP_DIRECTIVE_EOF = 0x04,       /**< End-of-File PDU. */
    CFDP_DIRECTIVE_FINISHED = 0x05,  /**< Finished PDU. */
    CFDP_DIRECTIVE_ACK = 0x06,       /**< Acknowledgement PDU. */
    CFDP_DIRECTIVE_METADATA = 0x07,  /**< Metadata PDU. */
    CFDP_DIRECTIVE_NAK = 0x08,       /**< Negative Acknowledgement PDU. */
    CFDP_DIRECTIVE_PROMPT = 0x09,    /**< Prompt PDU. */
    CFDP_DIRECTIVE_KEEP_ALIVE = 0x0C /**< Keep Alive PDU. */
} cfdp_directive_code_t;

/**
 * @brief Condition codes (CCSDS 727.0-B-5 §5.5, Table 5-5).
 *
 * Enum values equal the 4-bit condition code on the wire.
 */
typedef enum
{
    CFDP_COND_NO_ERROR = 0x0,                    /**< No error. */
    CFDP_COND_POSITIVE_ACK_LIMIT_REACHED = 0x1,  /**< Positive ACK limit reached. */
    CFDP_COND_KEEP_ALIVE_LIMIT_REACHED = 0x2,    /**< Keep Alive limit reached. */
    CFDP_COND_INVALID_TRANSMISSION_MODE = 0x3,   /**< Invalid transmission mode. */
    CFDP_COND_FILESTORE_REJECTION = 0x4,         /**< Filestore rejection. */
    CFDP_COND_FILE_CHECKSUM_FAILURE = 0x5,       /**< File checksum failure. */
    CFDP_COND_FILE_SIZE_ERROR = 0x6,             /**< File size error. */
    CFDP_COND_NAK_LIMIT_REACHED = 0x7,           /**< NAK limit reached. */
    CFDP_COND_INACTIVITY_DETECTED = 0x8,         /**< Inactivity detected. */
    CFDP_COND_INVALID_FILE_STRUCTURE = 0x9,      /**< Invalid file structure. */
    CFDP_COND_CHECK_LIMIT_REACHED = 0xA,         /**< Check limit reached. */
    CFDP_COND_UNSUPPORTED_CHECKSUM_TYPE = 0xB,   /**< Unsupported checksum type. */
    CFDP_COND_SUSPEND_REQUEST_RECEIVED = 0xE,    /**< Suspend request received. */
    CFDP_COND_CANCEL_REQUEST_RECEIVED = 0xF      /**< Cancel request received. */
} cfdp_condition_code_t;

/**
 * @brief Delivery code carried in the Finished PDU (CCSDS 727.0-B-5 §5.4.2).
 *
 * Enum values equal the 1-bit wire pattern directly.
 */
typedef enum
{
    CFDP_DELIVERY_COMPLETE = 0,  /**< Data complete: whole file delivered. */
    CFDP_DELIVERY_INCOMPLETE = 1 /**< Data incomplete: file not fully delivered. */
} cfdp_delivery_code_t;

/**
 * @brief File status carried in the Finished PDU (CCSDS 727.0-B-5 §5.4.2).
 *
 * Enum values equal the 2-bit wire pattern directly.
 */
typedef enum
{
    CFDP_FILE_STATUS_DISCARDED = 0,                     /**< Deliberately discarded. */
    CFDP_FILE_STATUS_DISCARDED_FILESTORE_REJECTION = 1, /**< Discarded on filestore rejection. */
    CFDP_FILE_STATUS_RETAINED = 2,                      /**< Retained in the filestore. */
    CFDP_FILE_STATUS_UNREPORTED = 3                     /**< File status not reported. */
} cfdp_file_status_t;

/**
 * @brief Transaction status carried in the ACK PDU (CCSDS 727.0-B-5 §5.4.3).
 *
 * Enum values equal the 2-bit wire pattern directly.
 */
typedef enum
{
    CFDP_TXN_STATUS_UNDEFINED = 0,   /**< Transaction status undefined. */
    CFDP_TXN_STATUS_ACTIVE = 1,      /**< Transaction is active. */
    CFDP_TXN_STATUS_TERMINATED = 2,  /**< Transaction is terminated. */
    CFDP_TXN_STATUS_UNRECOGNIZED = 3 /**< Transaction is unrecognized. */
} cfdp_transaction_status_t;

/**
 * @brief Checksum algorithm identifier (CCSDS 727.0-B-5 §5.2.5; SANA registry).
 *
 * Enum values equal the 4-bit checksum type field in the Metadata PDU.
 */
typedef enum
{
    CFDP_CHECKSUM_MODULAR = 0, /**< Legacy 32-bit modular checksum. */
    CFDP_CHECKSUM_NULL = 15    /**< Null checksum: value is always zero. */
} cfdp_checksum_type_t;

/**
 * @brief Prompt PDU response type (CCSDS 727.0-B-5 §5.4.5).
 *
 * Enum values equal the 1-bit wire pattern directly.
 */
typedef enum
{
    CFDP_PROMPT_NAK = 0,       /**< Prompt the receiver to issue a NAK. */
    CFDP_PROMPT_KEEP_ALIVE = 1 /**< Prompt the receiver to issue a Keep Alive. */
} cfdp_prompt_response_t;

/* -------------------------------------------------------------------------
 * Inline Helpers
 * ---------------------------------------------------------------------- */

/**
 * @brief Number of octets used to encode file offsets and sizes.
 *
 * @param[in] large_file_flag Large File flag from the PDU header.
 * @return 8 for a large file, 4 for a small file.
 */
static inline uint8_t cfdp_file_size_octets(cfdp_large_file_flag_t large_file_flag)
{
    return (large_file_flag == CFDP_FILE_SIZE_LARGE) ? 8U : 4U;
}

#endif /* CFDP_COMMON_H */
