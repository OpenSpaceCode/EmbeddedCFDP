/**
 * @file    cfdp_endian.h
 * @brief   Big-endian (network order) integer serialisation helpers
 *
 * CFDP encodes every multi-octet field in big-endian order
 * (CCSDS 727.0-B-5 §5.1). These small inline helpers keep the module
 * serialisers free of hand-rolled shift/mask loops.
 * See also: docs/ccsds_cfdp.md
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#ifndef CFDP_ENDIAN_H
#define CFDP_ENDIAN_H

#include <stdint.h>

/**
 * @brief Write an unsigned integer to a buffer in big-endian order.
 *
 * @param[out] buf    Destination buffer, must hold at least @p nbytes octets.
 * @param[in]  value  Value to encode; only the low @p nbytes octets are used.
 * @param[in]  nbytes Number of octets to write (1..8).
 */
static inline void cfdp_write_uint(uint8_t *buf, uint64_t value, uint8_t nbytes)
{
    for (uint8_t i = 0; i < nbytes; i++)
    {
        buf[nbytes - 1U - i] = (uint8_t)(value & 0xFFU);
        value >>= 8;
    }
}

/**
 * @brief Read a big-endian unsigned integer from a buffer.
 *
 * @param[in] buf    Source buffer, must hold at least @p nbytes octets.
 * @param[in] nbytes Number of octets to read (1..8).
 * @return The decoded value.
 */
static inline uint64_t cfdp_read_uint(const uint8_t *buf, uint8_t nbytes)
{
    uint64_t value = 0;
    for (uint8_t i = 0; i < nbytes; i++)
    {
        value = (value << 8) | (uint64_t)buf[i];
    }
    return value;
}

#endif /* CFDP_ENDIAN_H */
