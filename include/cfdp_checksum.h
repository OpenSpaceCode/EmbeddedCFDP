/**
 * @file    cfdp_checksum.h
 * @brief   CFDP 32-bit modular file checksum
 *
 * Implements the legacy modular checksum as per CCSDS 727.0-B-5 §4.2.2.
 * The checksum is the arithmetic sum, modulo 2^32, of the 4-octet words
 * formed by the file contents aligned to their absolute offset within the
 * file. Because each octet contributes independently of the others, the
 * checksum can be accumulated segment-by-segment and in any order.
 * See also: docs/727x0b5e1.pdf
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#ifndef CFDP_CHECKSUM_H
#define CFDP_CHECKSUM_H

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

#endif /* CFDP_CHECKSUM_H */
