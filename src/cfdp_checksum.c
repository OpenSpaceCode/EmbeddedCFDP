/**
 * @file    cfdp_checksum.c
 * @brief   CFDP 32-bit file checksums
 *
 * Implements the modular checksum (CCSDS 727.0-B-5 §4.2.5) and the null
 * checksum (§4.2.2.4), the two algorithms §4.2.2.3-4.2.2.4 make mandatory.
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

bool cfdp_checksum_type_supported(cfdp_checksum_type_t type)
{
    return (type == CFDP_CHECKSUM_MODULAR) || (type == CFDP_CHECKSUM_NULL);
}

bool cfdp_checksum_update_by_type(cfdp_checksum_type_t type,
                                  uint32_t *checksum,
                                  uint64_t offset,
                                  const uint8_t *data,
                                  size_t len)
{
    if ((!checksum) || (!cfdp_checksum_type_supported(type)))
    {
        return false;
    }

    /* §4.2.2.4: the null checksum algorithm is to set the value to zero. */
    *checksum =
        (type == CFDP_CHECKSUM_NULL) ? 0U : cfdp_checksum_update(*checksum, offset, data, len);

    return true;
}
