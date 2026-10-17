/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "adf.h"
#include "bytes.h"
#include "file.h"
#include "os.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Layout of the Amiga Old File System on a double-density floppy. Offsets are bytes in a block. */
#define ADF_BLOCK_SIZE        512
#define ADF_BLOCKS            1760 /* 80 cylinders, 2 heads, 11 sectors */
#define ADF_SIZE              (ADF_BLOCK_SIZE * ADF_BLOCKS)
#define ADF_ROOT_BLOCK        880 /* the middle of the disk */
#define ADF_DOS_TAG_SIZE      3   /* "DOS" in the boot block, then the flags byte */
#define ADF_FLAGS_OFFSET      3
#define ADF_TYPE_HEADER       2  /* root, folder, or file header block */
#define ADF_TYPE_DATA         8  /* OFS data block */
#define ADF_SEC_FOLDER        2  /* secondary type of a folder header */
#define ADF_LONG_SIZE         4  /* a block is an array of big-endian longs */
#define ADF_HASH_OFFSET       24 /* the folder hash table, or the data block list of a file */
#define ADF_HASH_SIZE         72 /* (512 / 4) - 56 entries */
#define ADF_FIRST_DATA_OFFSET 16
#define ADF_BYTE_SIZE_OFFSET  324 /* file size in bytes */
#define ADF_NAME_OFFSET       432 /* BCPL string: length byte, then the characters */
#define ADF_NAME_MAX          30
#define ADF_CHAIN_OFFSET      496 /* next header with the same hash */
#define ADF_SEC_TYPE_OFFSET   508
#define ADF_DATA_SIZE_OFFSET  12
#define ADF_NEXT_DATA_OFFSET  16
#define ADF_DATA_OFFSET       24
#define ADF_DATA_MAX          (ADF_BLOCK_SIZE - ADF_DATA_OFFSET)
#define ADF_FOLDER_DEPTH      8             /* deeper folders mean a broken image */
#define ADF_UNSAFE_NAME_CHARS "/\\:*?\"<>|" /* characters that macOS or Windows do not accept in a file name */

/* Bit of the flags byte of the boot block: set for the Fast File System, whose data blocks have no header. */
#define ADF_FFS_FLAG 0x01

typedef enum { ADF_VISIT_CONTINUE, ADF_VISIT_STOP, ADF_VISIT_BROKEN } AdfVisit;

typedef AdfVisit (*EntryVisit)(const uint8_t *img, const uint8_t *entry, const char *name, void *ctx);

/* Block n of the image, or NULL outside the disk. Block 0 is the boot block and never a link. */
static const uint8_t *block(const uint8_t *img, uint32_t n)
{
    return n && n < ADF_BLOCKS ? img + (size_t)n * ADF_BLOCK_SIZE : NULL;
}

/* The image in memory, or NULL when the file is not an 880 KB OFS floppy. */
static uint8_t *load(const char *path)
{
    size_t len;
    uint8_t *img = file_read(path, &len);
    if (img && (len != ADF_SIZE || memcmp(img, "DOS", ADF_DOS_TAG_SIZE) || img[ADF_FLAGS_OFFSET] & ADF_FFS_FLAG)) {
        free(img);
        img = NULL;
    }
    return img;
}

/* Calls visit for each entry of a folder header block; stops at the first entry that does not continue. BROKEN for a broken chain. */
static AdfVisit each_entry(const uint8_t *img, const uint8_t *folder, EntryVisit visit, void *ctx)
{
    for (int i = 0; i < ADF_HASH_SIZE; i++) {
        uint32_t n = be32(folder + ADF_HASH_OFFSET + ADF_LONG_SIZE * i);
        for (int guard = 0; n; guard++) {
            const uint8_t *entry = block(img, n);
            if (!entry || guard >= ADF_BLOCKS || be32(entry) != ADF_TYPE_HEADER)
                return ADF_VISIT_BROKEN;
            char name[ADF_NAME_MAX + 1];
            int len = entry[ADF_NAME_OFFSET] < ADF_NAME_MAX ? entry[ADF_NAME_OFFSET] : ADF_NAME_MAX;
            memcpy(name, entry + ADF_NAME_OFFSET + 1, len);
            name[len] = '\0';
            AdfVisit r = visit(img, entry, name, ctx);
            if (r != ADF_VISIT_CONTINUE)
                return r;
            n = be32(entry + ADF_CHAIN_OFFSET);
        }
    }
    return ADF_VISIT_CONTINUE;
}

/* Stops at a file (not a folder) of the disk with the name in ctx, without case. */
static AdfVisit match_name(const uint8_t *img, const uint8_t *entry, const char *name, void *ctx)
{
    (void)img;
    bool match = (int32_t)be32(entry + ADF_SEC_TYPE_OFFSET) != ADF_SEC_FOLDER && !file_compare_names(name, ctx);
    return match ? ADF_VISIT_STOP : ADF_VISIT_CONTINUE;
}

bool adf_has_file(const char *adf_path, const char *name)
{
    uint8_t *img = load(adf_path);
    bool found =
        img && each_entry(img, img + ADF_ROOT_BLOCK * ADF_BLOCK_SIZE, match_name, (void *)name) == ADF_VISIT_STOP;
    free(img);
    return found;
}

/* Makes the folder and its parents. */
static bool make_folders(const char *path)
{
    char p[PATH_SIZE];
    snprintf(p, sizeof p, "%s", path);
    for (char *s = p + 1; *s; s++) {
        if (*s != '/')
            continue;
        *s = '\0';
        if (!os_make_folder(p))
            return false;
        *s = '/';
    }
    return os_make_folder(p);
}

/* Writes the file of a file header block: its data blocks in their chain. False when it cannot be written. */
static bool write_file(const uint8_t *img, const uint8_t *header, const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        return false;
    uint32_t left = be32(header + ADF_BYTE_SIZE_OFFSET);
    uint32_t n = be32(header + ADF_FIRST_DATA_OFFSET);
    for (int guard = 0; left; guard++) {
        const uint8_t *data = block(img, n);
        uint32_t size = data ? be32(data + ADF_DATA_SIZE_OFFSET) : 0;
        if (!data || guard >= ADF_BLOCKS || be32(data) != ADF_TYPE_DATA || !size || size > ADF_DATA_MAX ||
            size > left || fwrite(data + ADF_DATA_OFFSET, 1, size, f) != size)
            break;
        left -= size;
        n = be32(data + ADF_NEXT_DATA_OFFSET);
    }
    return !fclose(f) && !left;
}

typedef struct {
    const char *dir;
    int depth;
} ExtractTarget;

bool adf_is_safe_name(const char *name)
{
    if (!*name || !strcmp(name, ".") || !strcmp(name, ".."))
        return false;
    for (const unsigned char *p = (const unsigned char *)name; *p; p++)
        if (*p < ' ' || strchr(ADF_UNSAFE_NAME_CHARS, *p))
            return false;
    return true;
}

/* Writes one file of the disk into the target folder, or makes a folder and extracts its entries. */
static AdfVisit extract_entry(const uint8_t *img, const uint8_t *entry, const char *name, void *ctx)
{
    const ExtractTarget *at = ctx;
    char path[PATH_SIZE];
    if (!adf_is_safe_name(name))
        return ADF_VISIT_BROKEN;
    snprintf(path, sizeof path, "%s/%s", at->dir, name);
    if ((int32_t)be32(entry + ADF_SEC_TYPE_OFFSET) != ADF_SEC_FOLDER)
        return write_file(img, entry, path) ? ADF_VISIT_CONTINUE : ADF_VISIT_BROKEN;
    ExtractTarget sub = {path, at->depth + 1};
    if (sub.depth > ADF_FOLDER_DEPTH || !make_folders(path))
        return ADF_VISIT_BROKEN;
    return each_entry(img, entry, extract_entry, &sub);
}

bool adf_extract(const char *adf_path, const char *out_dir)
{
    uint8_t *img = load(adf_path);
    ExtractTarget root = {out_dir, 0};
    bool ok = img && make_folders(out_dir) &&
              each_entry(img, img + ADF_ROOT_BLOCK * ADF_BLOCK_SIZE, extract_entry, &root) == ADF_VISIT_CONTINUE;
    free(img);
    return ok;
}
