#ifndef FTL_H
#define FTL_H
#include <stddef.h>
#include <stdint.h>

#define KAOS_BASE 0x100000

typedef struct {
    uint8_t *data; /* malloc'd memory image, loaded at base */
    uint32_t size, base, data_base, code_base;
} FtlImage;

int ftl_load(const uint8_t *raw, size_t len, uint32_t base, FtlImage *img);
void ftl_free(FtlImage *img);
uint8_t *read_file(const char *path, size_t *len);

extern FtlImage kaos;
int kaos_load(const char *path);
uint8_t *kaos_ptr(uint32_t addr);
uint8_t kaos_u8(uint32_t addr);
uint16_t kaos_u16(uint32_t addr);
int16_t kaos_s16(uint32_t addr);
uint32_t kaos_u32(uint32_t addr);

#endif
