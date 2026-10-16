#include "crc32.h"
#include "bytes.h"

#define CRC32_POLYNOMIAL 0xedb88320u /* reflected CRC-32 of PNG */
#define CRC_TABLE_SIZE   256         /* one entry for each byte value */
#define BYTE_MASK        0xff

static uint32_t crc_table[CRC_TABLE_SIZE];

uint32_t crc32_update(uint32_t crc, const uint8_t *p, size_t n)
{
    if (!crc_table[1])
        for (uint32_t i = 0; i < CRC_TABLE_SIZE; i++) {
            uint32_t c = i;
            for (int k = 0; k < BYTE_BITS; k++)
                c = c & 1 ? CRC32_POLYNOMIAL ^ c >> 1 : c >> 1;
            crc_table[i] = c;
        }
    crc = ~crc;
    while (n--)
        crc = crc_table[(crc ^ *p++) & BYTE_MASK] ^ crc >> BYTE_BITS;
    return ~crc;
}
