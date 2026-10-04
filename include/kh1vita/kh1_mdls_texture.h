#pragma once

#include <stdint.h>

typedef struct Kh1MdlsDecodedTextureInfo {
    uint32_t width;
    uint32_t height;
    uint32_t rgba_size;
} Kh1MdlsDecodedTextureInfo;

/* Query texture size and optionally decode to tightly packed RGBA8888. */
int kh1_mdls_decode_texture_rgba(const char *path,
                                 uint32_t texture_index,
                                 unsigned char *rgba,
                                 uint32_t rgba_capacity,
                                 Kh1MdlsDecodedTextureInfo *out_info);
