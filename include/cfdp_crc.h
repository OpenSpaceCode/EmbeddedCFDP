/**
 * @file    cfdp_crc.h
 * @brief   CFDP 16-bit PDU CRC
 *
 * Implements the CRC that protects a whole PDU when the CRC option is active
 * (CCSDS 727.0-B-5 §4.1). §4.1.3.1 delegates the algorithm to §4.2.1.3 of the
 * TC Space Data Link Protocol (CCSDS 232.0-B-3): generator polynomial
 * x^16 + x^12 + x^5 + 1, shift register preset to all ones, most significant
 * bit first, no final inversion. This is the CRC-16/CCITT-FALSE of the common
 * catalogues; its check value for the ASCII string "123456789" is 0x29B1.
 * See also: docs/727x0b5e1.pdf
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#ifndef CFDP_CRC_H
#define CFDP_CRC_H

#include <stddef.h>
#include <stdint.h>

/* -------------------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------------- */

/** @brief Generator polynomial x^16 + x^12 + x^5 + 1, without the x^16 term. */
#define CFDP_CRC_POLYNOMIAL 0x1021U

/** @brief Initial shift register state: all ones (CCSDS 232.0-B-3 §4.2.1.3). */
#define CFDP_CRC_INIT 0xFFFFU

/* -------------------------------------------------------------------------
 * Function Declarations
 * ---------------------------------------------------------------------- */

/**
 * @brief Fold octets into a running CRC.
 *
 * Suitable for streaming: start from ::CFDP_CRC_INIT and pass the octets of
 * the PDU in wire order, in as many calls as convenient.
 *
 * @param[in] crc  Running CRC so far (::CFDP_CRC_INIT for the first call).
 * @param[in] data Octets to fold in; may be NULL only when @p len is 0.
 * @param[in] len  Number of octets in @p data.
 * @return The updated CRC.
 */
uint16_t cfdp_crc_update(uint16_t crc, const uint8_t *data, size_t len);

/**
 * @brief Compute the CRC of a contiguous octet sequence.
 *
 * Convenience wrapper equivalent to cfdp_crc_update(CFDP_CRC_INIT, data, len).
 *
 * @param[in] data Octets to protect; may be NULL only when @p len is 0.
 * @param[in] len  Number of octets in @p data.
 * @return The CRC.
 */
uint16_t cfdp_crc_compute(const uint8_t *data, size_t len);

#endif /* CFDP_CRC_H */
