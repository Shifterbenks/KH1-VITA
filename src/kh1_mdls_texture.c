#include "kh1vita/kh1_mdls_texture.h"
#include "kh1vita/kh1_mdls.h"

#include <stdio.h>
#include <string.h>

#define KH1_MDLS_HEADER_U32_COUNT 14u
#define KH1_MDLS_HEADER_SIZE (8u + KH1_MDLS_HEADER_U32_COUNT * 4u)
#define KH1_MDLS_TEXTURE_INFO_SIZE 16u
#define KH1_MDLS_CLUT_COLORS 256u
#define KH1_MDLS_CLUT_BYTES (KH1_MDLS_CLUT_COLORS * 4u)

static uint16_t load_u16le(const unsigned char *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t load_u32le(const unsigned char *p) {
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static int file_size_u32(FILE *f, uint32_t *out) {
    long end;
    if (!f || !out) return -1;
    if (fseek(f, 0, SEEK_END) != 0) return -1;
    end = ftell(f);
    if (end < 0 || (unsigned long)end > 0xffffffffUL) return -1;
    *out = (uint32_t)end;
    return fseek(f, 0, SEEK_SET) == 0 ? 0 : -1;
}

static int read_at(FILE *f, uint32_t offset, void *dst, size_t size) {
    if (!f || !dst) return -1;
    if (fseek(f, (long)offset, SEEK_SET) != 0) return -1;
    return fread(dst, 1, size, f) == size ? 0 : -1;
}

static unsigned char clut_index_fix(unsigned char index) {
    unsigned low = (unsigned)index & 31u;
    if (low >= 8u && low < 16u) return (unsigned char)(index + 8u);
    if (low >= 16u && low < 24u) return (unsigned char)(index - 8u);
    return index;
}

int kh1_mdls_decode_texture_rgba(const char *path,
                                 uint32_t texture_index,
                                 unsigned char *rgba,
                                 uint32_t rgba_capacity,
                                 Kh1MdlsDecodedTextureInfo *out_info) {
    FILE *f;
    uint32_t file_size;
    unsigned char header[KH1_MDLS_HEADER_SIZE];
    uint32_t texture_info_offset;
    uint32_t texture_info_size;
    uint32_t texture_data_offset;
    uint32_t texture_data_size;
    uint32_t clut_offset;
    uint32_t clut_size;
    uint32_t texture_count;
    unsigned char info[KH1_MDLS_TEXTURE_INFO_SIZE];
    uint32_t width, height;
    uint32_t pixel_count;
    uint32_t rgba_size;
    uint32_t i;
    uint32_t pixel_offset = 0;
    unsigned char clut[KH1_MDLS_CLUT_BYTES];

    if (!path) return -1;
    f = fopen(path, "rb");
    if (!f) return -2;
    if (file_size_u32(f, &file_size) < 0 ||
        file_size < KH1_MDLS_BASE_OFFSET + KH1_MDLS_HEADER_SIZE ||
        read_at(f, KH1_MDLS_BASE_OFFSET, header, sizeof(header)) < 0 ||
        load_u32le(header) != KH1_MDLS_MAGIC) {
        fclose(f);
        return -3;
    }

    texture_info_offset = load_u32le(header + 8u + 0u * 4u);
    texture_info_size = load_u32le(header + 8u + 1u * 4u);
    texture_data_offset = load_u32le(header + 8u + 2u * 4u);
    texture_data_size = load_u32le(header + 8u + 3u * 4u);
    clut_offset = load_u32le(header + 8u + 4u * 4u);
    clut_size = load_u32le(header + 8u + 5u * 4u);

    if ((texture_info_size % KH1_MDLS_TEXTURE_INFO_SIZE) != 0u) {
        fclose(f);
        return -4;
    }
    texture_count = texture_info_size / KH1_MDLS_TEXTURE_INFO_SIZE;
    if (texture_index >= texture_count || clut_size < texture_count * KH1_MDLS_CLUT_BYTES) {
        fclose(f);
        return -5;
    }

    if ((uint64_t)KH1_MDLS_BASE_OFFSET + texture_info_offset + texture_info_size > file_size ||
        (uint64_t)KH1_MDLS_BASE_OFFSET + texture_data_offset + texture_data_size > file_size ||
        (uint64_t)KH1_MDLS_BASE_OFFSET + clut_offset + clut_size > file_size) {
        fclose(f);
        return -6;
    }

    /* Texture images are stored sequentially. Walk earlier image info entries
       to find this image's byte offset. KH1's indexed textures use one byte per pixel. */
    for (i = 0; i <= texture_index; ++i) {
        uint32_t iw, ih;
        if (read_at(f,
                    KH1_MDLS_BASE_OFFSET + texture_info_offset + i * KH1_MDLS_TEXTURE_INFO_SIZE,
                    info, sizeof(info)) < 0) {
            fclose(f);
            return -7;
        }
        iw = load_u16le(info + 4u);
        ih = load_u16le(info + 6u);
        if (iw == 0u || ih == 0u || (uint64_t)iw * ih > 0xffffffffu) {
            fclose(f);
            return -8;
        }
        if (i < texture_index) pixel_offset += iw * ih;
    }

    width = load_u16le(info + 4u);
    height = load_u16le(info + 6u);
    pixel_count = width * height;
    rgba_size = pixel_count * 4u;
    if ((uint64_t)pixel_offset + pixel_count > texture_data_size) {
        fclose(f);
        return -9;
    }

    if (out_info) {
        out_info->width = width;
        out_info->height = height;
        out_info->rgba_size = rgba_size;
    }
    if (!rgba) {
        fclose(f);
        return 0;
    }
    if (rgba_capacity < rgba_size) {
        fclose(f);
        return -10;
    }

    if (read_at(f,
                KH1_MDLS_BASE_OFFSET + clut_offset + texture_index * KH1_MDLS_CLUT_BYTES,
                clut, sizeof(clut)) < 0) {
        fclose(f);
        return -11;
    }
    if (fseek(f, (long)(KH1_MDLS_BASE_OFFSET + texture_data_offset + pixel_offset), SEEK_SET) != 0) {
        fclose(f);
        return -12;
    }

    for (i = 0; i < pixel_count; ++i) {
        int raw_index = fgetc(f);
        unsigned char index;
        const unsigned char *c;
        unsigned alpha;
        if (raw_index == EOF) {
            fclose(f);
            return -13;
        }
        index = clut_index_fix((unsigned char)raw_index);
        c = &clut[(uint32_t)index * 4u];
        alpha = ((unsigned)c[3] * 255u) >> 7;
        if (alpha > 255u) alpha = 255u;
        rgba[i * 4u + 0u] = c[0];
        rgba[i * 4u + 1u] = c[1];
        rgba[i * 4u + 2u] = c[2];
        rgba[i * 4u + 3u] = (unsigned char)alpha;
    }

    fclose(f);
    return 0;
}
