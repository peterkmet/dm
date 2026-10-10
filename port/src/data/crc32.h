#ifndef DM_CRC32_H
#define DM_CRC32_H
/* Prefixes: crc32_ */
#include <stddef.h>
#include <stdint.h>

/* The CRC-32 of PNG and zip: start with 0, then feed the bytes in parts. */
uint32_t crc32_update(uint32_t crc, const uint8_t *p, size_t n);

#endif
