/**
 * @file    cfdp_checksum.h
 * @brief   CFDP 32-bit file checksums
 *
 * Implements the two checksum algorithms every CFDP entity must provide
 * (CCSDS 727.0-B-5 §4.2.2.3 and §4.2.2.4): the legacy modular checksum
 * (type 0, §4.2.5) and the null checksum (type 15, §4.2.2.4).
 * The modular checksum is the arithmetic sum, modulo 2^32, of the 4-octet
 * words formed by the file contents aligned to their absolute offset within
 * the file. Because each octet contributes independently of the others, the
 * checksum can be accumulated segment-by-segment and in any order. The null
 * checksum is always zero and so does not protect the file (§4.2.1.1 note).
 * See also: docs/727x0b5e1.pdf
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#ifndef CFDP_CHECKSUM_H
#define CFDP_CHECKSUM_H

#include "cfdp_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* -------------------------------------------------------------------------
 * Function Declarations
 * ---------------------------------------------------------------------- */

/**
 * @brief Fold one file segment into a running modular checksum.
 *
 * Suitable for streaming: call once per received or transmitted File Data
 * segment, passing the checksum returned by the previous call. Segments may
 * be supplied in any order and need not be aligned to a 4-octet boundary.
 *
 * @param[in] checksum Running checksum so far (0 for the first segment).
 * @param[in] offset   Absolute offset of @p data within the file, in octets.
 * @param[in] data     Segment octets; may be NULL only when @p len is 0.
 * @param[in] len      Number of octets in @p data.
 * @return The updated checksum.
 */
uint32_t cfdp_checksum_update(uint32_t checksum, uint64_t offset, const uint8_t *data, size_t len);

/**
 * @brief Compute the modular checksum of a whole in-memory file.
 *
 * Convenience wrapper equivalent to cfdp_checksum_update(0, 0, data, len).
 *
 * @param[in] data File octets; may be NULL only when @p len is 0.
 * @param[in] len  File length in octets.
 * @return The file checksum.
 */
uint32_t cfdp_checksum_compute(const uint8_t *data, size_t len);

/**
 * @brief Whether this library implements a checksum type (§4.2.2.2).
 *
 * Use it to decide between the preferred algorithm and the §4.2.2.8 fallback:
 * an unsupported type raises an Unsupported Checksum Type fault, and the
 * sending entity then uses ::CFDP_CHECKSUM_MODULAR while the receiving entity
 * uses ::CFDP_CHECKSUM_NULL.
 *
 * @param[in] type Checksum type, e.g. from a Metadata PDU.
 * @return true for ::CFDP_CHECKSUM_MODULAR and ::CFDP_CHECKSUM_NULL.
 */
bool cfdp_checksum_type_supported(cfdp_checksum_type_t type);

/**
 * @brief Fold one file segment into a running checksum of the given type.
 *
 * Streams exactly like cfdp_checksum_update(): call once per segment, in any
 * order, starting from 0. For ::CFDP_CHECKSUM_NULL the result is always 0.
 *
 * @param[in]     type     Checksum algorithm to apply.
 * @param[in,out] checksum Running checksum; updated only on success.
 * @param[in]     offset   Absolute offset of @p data within the file, in octets.
 * @param[in]     data     Segment octets; may be NULL only when @p len is 0.
 * @param[in]     len      Number of octets in @p data.
 * @return true on success, false if @p checksum is NULL or @p type is not
 *         supported (see cfdp_checksum_type_supported()).
 */
bool cfdp_checksum_update_by_type(cfdp_checksum_type_t type,
                                  uint32_t *checksum,
                                  uint64_t offset,
                                  const uint8_t *data,
                                  size_t len);

#endif /* CFDP_CHECKSUM_H */
