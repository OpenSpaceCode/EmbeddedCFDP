/**
 * @file    cfdp_crc.c
 * @brief   CFDP 16-bit PDU CRC
 *
 * Implements the PDU CRC of CCSDS 727.0-B-5 §4.1 using the TC Space Data Link
 * Protocol algorithm (CCSDS 232.0-B-3 §4.2.1.3).
 * See also: docs/727x0b5e1.pdf
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#include "cfdp_crc.h"

#include <stdbool.h>

uint16_t cfdp_crc_update(uint16_t crc, const uint8_t *data, size_t len)
{
    if ((!data) || (len == 0))
    {
        return crc;
    }

    /* Bit-serial form of the CCSDS 232.0-B-3 shift register: no lookup table,
     * so the code stays small and the correspondence to the standard obvious. */
    for (size_t i = 0; i < len; i++)
    {
        crc ^= (uint16_t)((uint16_t)data[i] << 8);
        for (uint8_t bit = 0; bit < 8; bit++)
        {
            bool msb_set = (crc & 0x8000U) != 0;
            crc = (uint16_t)(crc << 1);
            if (msb_set)
            {
                crc ^= (uint16_t)CFDP_CRC_POLYNOMIAL;
            }
        }
    }

    return crc;
}

uint16_t cfdp_crc_compute(const uint8_t *data, size_t len)
{
    return cfdp_crc_update((uint16_t)CFDP_CRC_INIT, data, len);
}
