#include "kh1vita/kh1_mdls.h"

#include <stdio.h>
#include <string.h>

#define KH1_MDLS_HEADER_U32_COUNT 14u
#define KH1_MDLS_HEADER_SIZE (8u + KH1_MDLS_HEADER_U32_COUNT * 4u)
#define KH1_MDLS_MODEL_HEADER_SIZE 16u
#define KH1_MDLS_TEXTURE_INFO_SIZE 16u

static int read_bytes(FILE *f, void *dst, size_t size) {
    return (f && dst && fread(dst, 1, size, f) == size) ? 0 : -1;
}

static int read_u16le(FILE *f, uint16_t *out) {
    unsigned char b[2];
    if (!out || read_bytes(f, b, sizeof(b)) < 0) return -1;
    *out = (uint16_t)((uint16_t)b[0] | ((uint16_t)b[1] << 8));
    return 0;
}

static int read_u32le(FILE *f, uint32_t *out) {
    unsigned char b[4];
    if (!out || read_bytes(f, b, sizeof(b)) < 0) return -1;
    *out = (uint32_t)b[0]
         | ((uint32_t)b[1] << 8)
         | ((uint32_t)b[2] << 16)
         | ((uint32_t)b[3] << 24);
    return 0;
}

static int get_file_size(FILE *f, uint32_t *out) {
    long end;
    if (!f || !out) return -1;
    if (fseek(f, 0, SEEK_END) != 0) return -1;
    end = ftell(f);
    if (end < 0 || (unsigned long)end > 0xffffffffUL) return -1;
    *out = (uint32_t)end;
    return fseek(f, 0, SEEK_SET) == 0 ? 0 : -1;
}

static int range_valid(uint32_t file_size, uint32_t relative_offset, uint32_t size) {
    uint64_t start = (uint64_t)KH1_MDLS_BASE_OFFSET + relative_offset;
    uint64_t end = start + size;
    return start <= file_size && end <= file_size;
}

int kh1_mdls_scan(const char *path,
                  Kh1MdlsInfo *info,
                  Kh1MdlsTextureVisitor texture_visitor,
                  void *user) {
    FILE *f;
    uint32_t file_size;
    uint32_t magic;
    uint32_t data_size;
    uint32_t h[KH1_MDLS_HEADER_U32_COUNT];
    uint32_t joint_count;
    uint32_t joint_info_offset;
    uint32_t bone_data_offset;
    uint32_t mesh_count;
    uint32_t texture_count;
    uint32_t i;

    (void)joint_info_offset;
    (void)bone_data_offset;

    if (!path) return -1;
    f = fopen(path, "rb");
    if (!f) return -2;

    if (get_file_size(f, &file_size) < 0 ||
        file_size < KH1_MDLS_BASE_OFFSET + KH1_MDLS_HEADER_SIZE) {
        fclose(f);
        return -3;
    }

    if (fseek(f, (long)KH1_MDLS_BASE_OFFSET, SEEK_SET) != 0 ||
        read_u32le(f, &magic) < 0 || magic != KH1_MDLS_MAGIC ||
        read_u32le(f, &data_size) < 0) {
        fclose(f);
        return -4;
    }

    for (i = 0; i < KH1_MDLS_HEADER_U32_COUNT; ++i) {
        if (read_u32le(f, &h[i]) < 0) {
            fclose(f);
            return -5;
        }
    }

    /* Header layout:
       0/1 texture-info offset+size
       2/3 texture-data offset+size
       4/5 CLUT offset+size
       6/7 model offset+size
       8/9 auxiliary offset+size
       10..13 unknown */
    if (!range_valid(file_size, h[0], h[1]) ||
        !range_valid(file_size, h[2], h[3]) ||
        !range_valid(file_size, h[4], h[5]) ||
        !range_valid(file_size, h[6], h[7])) {
        fclose(f);
        return -6;
    }

    if ((h[1] % KH1_MDLS_TEXTURE_INFO_SIZE) != 0) {
        fclose(f);
        return -7;
    }
    texture_count = h[1] / KH1_MDLS_TEXTURE_INFO_SIZE;

    if (fseek(f, (long)(KH1_MDLS_BASE_OFFSET + h[6]), SEEK_SET) != 0 ||
        read_u32le(f, &joint_count) < 0 ||
        read_u32le(f, &joint_info_offset) < 0 ||
        read_u32le(f, &bone_data_offset) < 0 ||
        read_u32le(f, &mesh_count) < 0) {
        fclose(f);
        return -8;
    }

    if ((uint64_t)KH1_MDLS_BASE_OFFSET + h[6] + KH1_MDLS_MODEL_HEADER_SIZE +
        (uint64_t)mesh_count * 16u > file_size) {
        fclose(f);
        return -9;
    }

    if (texture_visitor && texture_count != 0) {
        if (fseek(f, (long)(KH1_MDLS_BASE_OFFSET + h[0]), SEEK_SET) != 0) {
            fclose(f);
            return -10;
        }

        for (i = 0; i < texture_count; ++i) {
            Kh1MdlsTextureInfo t;
            uint32_t ignored;
            unsigned char exponents[2];
            memset(&t, 0, sizeof(t));

            if (read_u16le(f, &t.size_qwc) < 0 ||
                read_bytes(f, exponents, sizeof(exponents)) < 0 ||
                read_u16le(f, &t.width) < 0 ||
                read_u16le(f, &t.height) < 0 ||
                read_u32le(f, &ignored) < 0 ||
                read_u32le(f, &ignored) < 0) {
                fclose(f);
                return -11;
            }
            t.width_exp = exponents[0];
            t.height_exp = exponents[1];

            if (t.width == 0 || t.height == 0) {
                fclose(f);
                return -12;
            }

            if (texture_visitor(i, &t, user) != 0) break;
        }
    }

    if (info) {
        memset(info, 0, sizeof(*info));
        info->file_size = file_size;
        info->data_size = data_size;
        info->texture_count = texture_count;
        info->joint_count = joint_count;
        info->mesh_count = mesh_count;
        info->texture_info_offset = h[0];
        info->texture_info_size = h[1];
        info->texture_data_offset = h[2];
        info->texture_data_size = h[3];
        info->clut_offset = h[4];
        info->clut_size = h[5];
        info->model_offset = h[6];
        info->model_size = h[7];
    }

    fclose(f);
    return 0;
}
