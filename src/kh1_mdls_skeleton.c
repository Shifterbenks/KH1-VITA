#include "kh1vita/kh1_mdls_skeleton.h"
#include "kh1vita/kh1_mdls.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define KH1_MDLS_HEADER_U32_COUNT 14u
#define KH1_MDLS_HEADER_SIZE (8u + KH1_MDLS_HEADER_U32_COUNT * 4u)
#define KH1_MDLS_MODEL_HEADER_SIZE 16u
#define KH1_MDLS_JOINT_SIZE 48u
#define KH1_MDLS_NO_PARENT 0x3ffu

static uint32_t load_u32le(const unsigned char *p) {
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static float load_f32le(const unsigned char *p) {
    uint32_t bits = load_u32le(p);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
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

static Kh1MdlsMat4 mat_identity(void) {
    Kh1MdlsMat4 r = {{
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    }};
    return r;
}

static Kh1MdlsMat4 mat_mul(const Kh1MdlsMat4 *a, const Kh1MdlsMat4 *b) {
    Kh1MdlsMat4 r;
    unsigned row, col, k;
    for (row = 0; row < 4u; ++row) {
        for (col = 0; col < 4u; ++col) {
            float sum = 0.0f;
            for (k = 0; k < 4u; ++k) {
                sum += a->m[row * 4u + k] * b->m[k * 4u + col];
            }
            r.m[row * 4u + col] = sum;
        }
    }
    return r;
}

static Kh1MdlsMat4 mat_translate(float x, float y, float z) {
    Kh1MdlsMat4 r = mat_identity();
    r.m[3] = x;
    r.m[7] = y;
    r.m[11] = z;
    return r;
}

static Kh1MdlsMat4 mat_scale(float x, float y, float z) {
    Kh1MdlsMat4 r = mat_identity();
    r.m[0] = x;
    r.m[5] = y;
    r.m[10] = z;
    return r;
}

static Kh1MdlsMat4 mat_rx(float a) {
    Kh1MdlsMat4 r = mat_identity();
    float c = cosf(a), s = sinf(a);
    r.m[5] = c; r.m[6] = -s;
    r.m[9] = s; r.m[10] = c;
    return r;
}

static Kh1MdlsMat4 mat_ry(float a) {
    Kh1MdlsMat4 r = mat_identity();
    float c = cosf(a), s = sinf(a);
    r.m[0] = c; r.m[2] = s;
    r.m[8] = -s; r.m[10] = c;
    return r;
}

static Kh1MdlsMat4 mat_rz(float a) {
    Kh1MdlsMat4 r = mat_identity();
    float c = cosf(a), s = sinf(a);
    r.m[0] = c; r.m[1] = -s;
    r.m[4] = s; r.m[5] = c;
    return r;
}

static Kh1MdlsMat4 joint_local(const Kh1MdlsJointInfo *j) {
    Kh1MdlsMat4 t = mat_translate(j->translate_x, j->translate_y, j->translate_z);
    Kh1MdlsMat4 rz = mat_rz(j->rotate_z);
    Kh1MdlsMat4 ry = mat_ry(j->rotate_y);
    Kh1MdlsMat4 rx = mat_rx(j->rotate_x);
    Kh1MdlsMat4 s = mat_scale(j->scale_x, j->scale_y, j->scale_z);
    Kh1MdlsMat4 r = mat_mul(&t, &rz);
    r = mat_mul(&r, &ry);
    r = mat_mul(&r, &rx);
    r = mat_mul(&r, &s);
    return r;
}

int kh1_mdls_build_pose(const Kh1MdlsJointInfo *joints,
                        uint32_t joint_count,
                        Kh1MdlsMat4 *out_pose,
                        uint32_t capacity) {
    uint32_t i;
    if (!joints || !out_pose || capacity < joint_count || joint_count == 0u) return -1;
    for (i = 0; i < joint_count; ++i) {
        const Kh1MdlsJointInfo *j = &joints[i];
        Kh1MdlsMat4 local;
        if (j->index != i) return -2;
        local = joint_local(j);
        if (j->parent_id == KH1_MDLS_NO_PARENT) {
            out_pose[i] = local;
        } else {
            if (j->parent_id >= i || j->parent_id >= joint_count) return -3;
            out_pose[i] = mat_mul(&out_pose[j->parent_id], &local);
        }
    }
    return 0;
}

int kh1_mdls_read_bind_pose(const char *path,
                            Kh1MdlsJointInfo *out_joints,
                            Kh1MdlsMat4 *out_bind_pose,
                            uint32_t capacity,
                            uint32_t *out_count) {
    FILE *f;
    uint32_t file_size;
    unsigned char header[KH1_MDLS_HEADER_SIZE];
    uint32_t model_offset;
    uint32_t model_abs;
    unsigned char model_header[KH1_MDLS_MODEL_HEADER_SIZE];
    uint32_t joint_count;
    uint32_t joint_info_offset;
    uint32_t joint_abs;
    uint32_t i;

    if (!path || !out_count) return -1;
    *out_count = 0;
    f = fopen(path, "rb");
    if (!f) return -2;

    if (file_size_u32(f, &file_size) < 0 ||
        file_size < KH1_MDLS_BASE_OFFSET + KH1_MDLS_HEADER_SIZE ||
        read_at(f, KH1_MDLS_BASE_OFFSET, header, sizeof(header)) < 0 ||
        load_u32le(header) != KH1_MDLS_MAGIC) {
        fclose(f);
        return -3;
    }

    model_offset = load_u32le(header + 8u + 6u * 4u);
    if ((uint64_t)KH1_MDLS_BASE_OFFSET + model_offset + KH1_MDLS_MODEL_HEADER_SIZE > file_size) {
        fclose(f);
        return -4;
    }
    model_abs = KH1_MDLS_BASE_OFFSET + model_offset;
    if (read_at(f, model_abs, model_header, sizeof(model_header)) < 0) {
        fclose(f);
        return -5;
    }
    joint_count = load_u32le(model_header + 0u);
    joint_info_offset = load_u32le(model_header + 4u);
    joint_abs = model_abs + joint_info_offset;
    if (joint_count == 0u || joint_count > 4096u ||
        (uint64_t)joint_abs + (uint64_t)joint_count * KH1_MDLS_JOINT_SIZE > file_size) {
        fclose(f);
        return -6;
    }

    *out_count = joint_count;
    if (!out_joints && !out_bind_pose) {
        fclose(f);
        return 0;
    }
    if (capacity < joint_count || !out_joints || !out_bind_pose) {
        fclose(f);
        return -7;
    }

    for (i = 0; i < joint_count; ++i) {
        unsigned char b[KH1_MDLS_JOINT_SIZE];
        Kh1MdlsJointInfo *j = &out_joints[i];
        Kh1MdlsMat4 local;
        if (read_at(f, joint_abs + i * KH1_MDLS_JOINT_SIZE, b, sizeof(b)) < 0) {
            fclose(f);
            return -8;
        }
        memset(j, 0, sizeof(*j));
        j->scale_x = load_f32le(b + 0u);
        j->scale_y = load_f32le(b + 4u);
        j->scale_z = load_f32le(b + 8u);
        j->index = load_u32le(b + 12u);
        j->rotate_x = load_f32le(b + 16u);
        j->rotate_y = load_f32le(b + 20u);
        j->rotate_z = load_f32le(b + 24u);
        j->translate_x = load_f32le(b + 32u);
        j->translate_y = load_f32le(b + 36u);
        j->translate_z = load_f32le(b + 40u);
        j->hierarchy_bits = load_u32le(b + 44u);
        j->parent_id = j->hierarchy_bits & KH1_MDLS_NO_PARENT;

        if (j->index != i) {
            fclose(f);
            return -9;
        }
        local = joint_local(j);
        if (j->parent_id == KH1_MDLS_NO_PARENT) {
            out_bind_pose[i] = local;
        } else {
            if (j->parent_id >= i) {
                fclose(f);
                return -10;
            }
            out_bind_pose[i] = mat_mul(&out_bind_pose[j->parent_id], &local);
        }
    }

    fclose(f);
    return 0;
}

void kh1_mdls_transform_point(const Kh1MdlsMat4 *matrix,
                              float x, float y, float z,
                              float *out_x, float *out_y, float *out_z) {
    if (!matrix) return;
    if (out_x) *out_x = matrix->m[0] * x + matrix->m[1] * y + matrix->m[2] * z + matrix->m[3];
    if (out_y) *out_y = matrix->m[4] * x + matrix->m[5] * y + matrix->m[6] * z + matrix->m[7];
    if (out_z) *out_z = matrix->m[8] * x + matrix->m[9] * y + matrix->m[10] * z + matrix->m[11];
}
