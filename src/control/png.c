/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "png.h"
#include "bytes.h"
#include "crc32.h"
#include "video.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* The zlib header bytes. */
#define ZLIB_METHOD 0x78 /* deflate, 32 KB window */
#define ZLIB_FLAGS  0x01 /* no dictionary; header check bits */

/* The zlib and PNG formats. */
#define ZLIB_HEADER_BYTES    2
#define ZLIB_BLOCK_HEAD      5     /* final flag, length, inverted length */
#define ADLER32_MODULUS      65521 /* the largest prime below 65536 */
#define ADLER32_HIGH_SHIFT   16
#define ADLER32_BYTES        4
#define PNG_IHDR_SIZE        13
#define PNG_BITS_PER_CHANNEL 8
#define PNG_COLOR_TYPE_RGB   2
#define PNG_SIGNATURE_BYTES  8
#define PNG_LENGTH_BYTES     4
#define PNG_TYPE_BYTES       4
#define PNG_CHUNK_HEAD_BYTES (PNG_LENGTH_BYTES + PNG_TYPE_BYTES)
#define PNG_CHUNK_CRC_BYTES  4
#define PNG_HEIGHT_OFFSET    4
#define PNG_DEPTH_OFFSET     8
#define PNG_COLOR_OFFSET     9
#define PNG_CHANNELS         3
#define PNG_FILTER_NONE      0

/* Sizes of the pixel data. */
#define PNG_ROW_BYTES        (1 + PNG_CHANNELS * SCREEN_W) /* filter byte and the pixels */
#define PNG_RAW_BYTES        (PNG_ROW_BYTES * SCREEN_H)
#define PNG_STORED_BLOCK_MAX 65535
#define PNG_BLOCKS           ((PNG_RAW_BYTES + PNG_STORED_BLOCK_MAX - 1) / PNG_STORED_BLOCK_MAX)
#define PNG_ZLIB_BYTES       (ZLIB_HEADER_BYTES + PNG_RAW_BYTES + ZLIB_BLOCK_HEAD * PNG_BLOCKS + ADLER32_BYTES)

/* Writes a chunk: length, type, data, CRC; false when a write fails. */
static bool write_chunk(FILE *file, const char *type, const uint8_t *data, uint32_t length)
{
    uint8_t head[PNG_CHUNK_HEAD_BYTES];
    uint8_t tail[PNG_CHUNK_CRC_BYTES];
    wr32(head, length);
    memcpy(head + PNG_LENGTH_BYTES, type, PNG_TYPE_BYTES);
    wr32(tail, crc32_update(crc32_update(0, head + PNG_LENGTH_BYTES, PNG_TYPE_BYTES), data, length));
    return fwrite(head, 1, sizeof head, file) == sizeof head && fwrite(data, 1, length, file) == length &&
           fwrite(tail, 1, sizeof tail, file) == sizeof tail;
}

bool png_write(const char *path)
{
    static uint8_t raw[PNG_RAW_BYTES], compressed[PNG_ZLIB_BYTES];
    for (int16_t y = 0; y < SCREEN_H; y++) {
        uint8_t *pixel = raw + y * PNG_ROW_BYTES;
        *pixel++ = PNG_FILTER_NONE;
        for (int x = 0; x < SCREEN_W; x++) {
            uint32_t argb = video_convert_argb(video_get_color(y, g_screen.px[y][x]));
            *pixel++ = video_argb_red(argb);
            *pixel++ = video_argb_green(argb);
            *pixel++ = video_argb_blue(argb);
        }
    }
    uint8_t *out = compressed;
    *out++ = ZLIB_METHOD;
    *out++ = ZLIB_FLAGS;
    uint32_t byte_sum = 1;
    uint32_t sum_of_sums = 0;
    for (size_t done = 0; done < PNG_RAW_BYTES;) {
        size_t n = PNG_RAW_BYTES - done < PNG_STORED_BLOCK_MAX ? PNG_RAW_BYTES - done : PNG_STORED_BLOCK_MAX;
        *out++ = done + n == PNG_RAW_BYTES;
        *out++ = (uint8_t)n;
        *out++ = (uint8_t)(n >> BITS_PER_BYTE);
        *out++ = (uint8_t)~n;
        *out++ = (uint8_t)(~n >> BITS_PER_BYTE);
        memcpy(out, raw + done, n);
        for (size_t i = 0; i < n; i++) {
            byte_sum = (byte_sum + raw[done + i]) % ADLER32_MODULUS;
            sum_of_sums = (sum_of_sums + byte_sum) % ADLER32_MODULUS;
        }
        out += n;
        done += n;
    }
    wr32(out, sum_of_sums << ADLER32_HIGH_SHIFT | byte_sum);
    out += ADLER32_BYTES;
    uint8_t ihdr[PNG_IHDR_SIZE] = {0};
    wr32(ihdr, SCREEN_W);
    wr32(ihdr + PNG_HEIGHT_OFFSET, SCREEN_H);
    ihdr[PNG_DEPTH_OFFSET] = PNG_BITS_PER_CHANNEL;
    ihdr[PNG_COLOR_OFFSET] = PNG_COLOR_TYPE_RGB;
    FILE *file = fopen(path, "wb");
    if (!file)
        return false;
    bool ok = fwrite("\x89PNG\r\n\x1a\n", 1, PNG_SIGNATURE_BYTES, file) == PNG_SIGNATURE_BYTES &&
              write_chunk(file, "IHDR", ihdr, sizeof ihdr) &&
              write_chunk(file, "IDAT", compressed, (uint32_t)(out - compressed)) && write_chunk(file, "IEND", NULL, 0);
    return fclose(file) == 0 && ok;
}
