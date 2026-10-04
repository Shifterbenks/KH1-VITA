#pragma once

#include <stdint.h>

#define KH1_MDLS_BASE_OFFSET 0x80u
#define KH1_MDLS_MAGIC 0x4A424F4Du /* 'MOBJ' as little-endian uint32 */

typedef struct Kh1MdlsInfo {
    uint32_t file_size;
    uint32_t data_size;
    uint32_t texture_count;
    uint32_t joint_count;
    uint32_t mesh_count;
    uint32_t texture_info_offset;
    uint32_t texture_info_size;
    uint32_t texture_data_offset;
    uint32_t texture_data_size;
    uint32_t clut_offset;
    uint32_t clut_size;
    uint32_t model_offset;
    uint32_t model_size;
} Kh1MdlsInfo;

typedef struct Kh1MdlsTextureInfo {
    uint16_t size_qwc;
    uint8_t width_exp;
    uint8_t height_exp;
    uint16_t width;
    uint16_t height;
} Kh1MdlsTextureInfo;

typedef int (*Kh1MdlsTextureVisitor)(uint32_t index,
                                     const Kh1MdlsTextureInfo *texture,
                                     void *user);

/* Portable metadata parser for KH1 PS2 .mdls files. */
int kh1_mdls_scan(const char *path,
                  Kh1MdlsInfo *info,
                  Kh1MdlsTextureVisitor texture_visitor,
                  void *user);
