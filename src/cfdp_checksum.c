/**
 * @file    cfdp_checksum.c
 * @brief   CFDP 32-bit modular file checksum
 *
 * Implements the legacy modular checksum as per CCSDS 727.0-B-5 §4.2.2.
 * See also: docs/727x0b5e1.pdf
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#include "cfdp_checksum.h"

uint32_t cfdp_checksum_update(uint32_t checksum, uint64_t offset, const uint8_t *data, size_t len)
{
    if ((!data) || (len == 0))
    {
        return checksum;
    }

    for (size_t i = 0; i < len; i++)
    {
        /* Each octet lands in one of the four byte lanes of a 4-octet word
         * according to its absolute position in the file, so the running sum
         * is independent of how the file was split into segments. */
        uint8_t lane = (uint8_t)((offset + i) & 0x3U);
        uint8_t shift = (uint8_t)(8U * (3U - lane));
        checksum += ((uint32_t)data[i]) << shift;
    }

    return checksum;
}

uint32_t cfdp_checksum_compute(const uint8_t *data, size_t len)
{
    return cfdp_checksum_update(0, 0, data, len);
}
