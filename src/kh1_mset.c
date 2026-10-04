#include "kh1vita/kh1_mset.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KH1_MSET_MMTN_MAGIC 0x4e544d4du /* "MMTN" little-endian */
#define KH1_MSET_MAX_SECTIONS 64u
#define KH1_MSET_MAX_MOTIONS 4096u
#define KH1_MSET_MOTION_HEADER_SIZE 0x50u
#define KH1_MSET_CHANNEL_SIZE 6u
#define KH1_MSET_KEY_SIZE 16u
#define KH1_MSET_AUX_JOINT_SIZE 40u
#define KH1_MSET_STATIC_SIZE 8u

static uint16_t load_u16le(const unsigned char *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

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

typedef struct ParsedMset {
    FILE *f;
    uint32_t file_size;
    uint32_t section_count;
    uint32_t mmtn_offset;
    uint32_t motion_table_abs;
    uint32_t motion_count;
    uint32_t motion_table_size;
    uint32_t *motion_offsets;
} ParsedMset;

static void parsed_close(ParsedMset *p) {
    if (!p) return;
    free(p->motion_offsets);
    if (p->f) fclose(p->f);
    memset(p, 0, sizeof(*p));
}

static int parsed_open(const char *path, ParsedMset *out) {
    unsigned char root[8];
    unsigned char mmtn_header[0x38];
    uint32_t motion_table_rel;
    uint32_t motion_table_size;
    uint32_t pos;
    uint32_t count = 0;
    if (!path || !out) return KH1_MSET_BAD_ARGUMENT;
    memset(out, 0, sizeof(*out));
    out->f = fopen(path, "rb");
    if (!out->f) return KH1_MSET_OPEN_FAILED;
    if (file_size_u32(out->f, &out->file_size) < 0 || out->file_size < 0x84u ||
        read_at(out->f, 0u, root, sizeof(root)) < 0) {
        parsed_close(out); return KH1_MSET_BAD_FILE;
    }
    out->section_count = load_u32le(root + 0u);
    out->mmtn_offset = load_u32le(root + 4u);
    if (out->section_count == 0u || out->section_count > KH1_MSET_MAX_SECTIONS ||
        out->mmtn_offset > out->file_size - sizeof(mmtn_header) ||
        read_at(out->f, out->mmtn_offset, mmtn_header, sizeof(mmtn_header)) < 0 ||
        load_u32le(mmtn_header) != KH1_MSET_MMTN_MAGIC) {
        parsed_close(out); return KH1_MSET_BAD_FILE;
    }

    /* Hypercrown and the supplied retail files agree that MMTN +0x30/+0x34
     * are the animation subsection offset/length. */
    motion_table_rel = load_u32le(mmtn_header + 0x30u);
    motion_table_size = load_u32le(mmtn_header + 0x34u);
    if (motion_table_rel < 0x40u || motion_table_size < 8u ||
        (uint64_t)out->mmtn_offset + motion_table_rel + motion_table_size > out->file_size) {
        parsed_close(out); return KH1_MSET_BAD_FILE;
    }
    out->motion_table_abs = out->mmtn_offset + motion_table_rel;
    out->motion_table_size = motion_table_size;
    out->motion_offsets = (uint32_t *)calloc(KH1_MSET_MAX_MOTIONS, sizeof(uint32_t));
    if (!out->motion_offsets) { parsed_close(out); return KH1_MSET_BAD_FILE; }

    pos = out->motion_table_abs;
    while (count < KH1_MSET_MAX_MOTIONS) {
        unsigned char word[4];
        uint32_t rel;
        if (pos > out->file_size - 4u || read_at(out->f, pos, word, 4u) < 0) {
            parsed_close(out); return KH1_MSET_BAD_FILE;
        }
        rel = load_u32le(word);
        pos += 4u;
        if (rel == 0xffffffffu) break;
        if (rel == 0u ||
            (uint64_t)out->motion_table_abs + rel + KH1_MSET_MOTION_HEADER_SIZE > out->file_size ||
            (count > 0u && rel <= out->motion_offsets[count - 1u])) {
            parsed_close(out); return KH1_MSET_BAD_FILE;
        }
        out->motion_offsets[count++] = rel;
    }
    if (count == 0u || count == KH1_MSET_MAX_MOTIONS ||
        (uint64_t)out->motion_table_abs + out->motion_offsets[0] < pos) {
        parsed_close(out); return KH1_MSET_BAD_FILE;
    }
    out->motion_count = count;
    return KH1_MSET_OK;
}

static int read_motion_info(ParsedMset *p, uint32_t index, Kh1MsetMotionInfo *out) {
    unsigned char h[KH1_MSET_MOTION_HEADER_SIZE];
    uint32_t abs;
    uint32_t raw_size;
    uint32_t i;
    uint32_t max_key = 0u;
    if (!p || !out || index >= p->motion_count) return KH1_MSET_BAD_ARGUMENT;
    abs = p->motion_table_abs + p->motion_offsets[index];
    raw_size = (index + 1u < p->motion_count)
             ? p->motion_offsets[index + 1u] - p->motion_offsets[index]
             : p->motion_table_size - p->motion_offsets[index];
    if (raw_size < KH1_MSET_MOTION_HEADER_SIZE || read_at(p->f, abs, h, sizeof(h)) < 0) {
        return KH1_MSET_BAD_MOTION;
    }
    memset(out, 0, sizeof(*out));
    out->motion_index = index;
    out->relative_offset = p->motion_offsets[index];
    out->raw_size = raw_size;
    out->frame_count = load_u32le(h + 0x04u);
    out->frames_per_second = load_f32le(h + 0x08u);
    out->flags = load_u32le(h + 0x0cu);
    out->joint_count = load_u32le(h + 0x10u);
    out->header_size = load_u32le(h + 0x14u);
    out->channel_count = load_u32le(h + 0x18u);
    out->channel_offset = load_u32le(h + 0x1cu);
    out->aux_channel_count = load_u32le(h + 0x20u);
    out->aux_channel_offset = load_u32le(h + 0x24u);
    out->curve_offset = load_u32le(h + 0x28u);
    out->aux_joint_count = load_u32le(h + 0x2cu);
    out->aux_joint_offset = load_u32le(h + 0x30u);
    out->static_transform_count = load_u32le(h + 0x34u);
    out->static_transform_offset = load_u32le(h + 0x38u);
    out->tail_offset = load_u32le(h + 0x3cu);

    if (out->frame_count > 100000u ||
        !(out->frames_per_second > 0.0f && out->frames_per_second <= 1000.0f) ||
        out->joint_count == 0u || out->joint_count > 4096u ||
        out->header_size < KH1_MSET_MOTION_HEADER_SIZE || out->header_size > raw_size) {
        return KH1_MSET_BAD_MOTION;
    }

    /* Several small KH1 MSETs omit whole motion subsections and use
     * 0xffffffff as the corresponding offset. Treat those as valid when the
     * count is zero instead of forcing the full Sora-style layout. */
    if (out->aux_joint_count > 0u &&
        (out->aux_joint_offset == 0xffffffffu || out->aux_joint_offset < out->header_size ||
         (uint64_t)out->aux_joint_offset + (uint64_t)out->aux_joint_count * KH1_MSET_AUX_JOINT_SIZE > raw_size)) {
        return KH1_MSET_BAD_MOTION;
    }
    if (out->channel_count > 0u &&
        (out->channel_offset == 0xffffffffu || out->channel_offset < out->header_size ||
         (uint64_t)out->channel_offset + (uint64_t)out->channel_count * KH1_MSET_CHANNEL_SIZE > raw_size)) {
        return KH1_MSET_BAD_MOTION;
    }
    if (out->aux_channel_count > 0u &&
        (out->aux_channel_offset == 0xffffffffu || out->aux_channel_offset < out->header_size ||
         (uint64_t)out->aux_channel_offset + (uint64_t)out->aux_channel_count * KH1_MSET_CHANNEL_SIZE > raw_size)) {
        return KH1_MSET_BAD_MOTION;
    }
    if ((out->channel_count > 0u || out->aux_channel_count > 0u) &&
        (out->curve_offset == 0xffffffffu || out->curve_offset < out->header_size || out->curve_offset > raw_size)) {
        return KH1_MSET_BAD_MOTION;
    }
    if (out->static_transform_count > 0u &&
        (out->static_transform_offset == 0xffffffffu || out->static_transform_offset < out->header_size ||
         (uint64_t)out->static_transform_offset + (uint64_t)out->static_transform_count * KH1_MSET_STATIC_SIZE > raw_size)) {
        return KH1_MSET_BAD_MOTION;
    }

    /* Curve indices are shared between main and auxiliary channels. */
    for (i = 0; i < out->channel_count + out->aux_channel_count; ++i) {
        unsigned char b[KH1_MSET_CHANNEL_SIZE];
        uint32_t off = (i < out->channel_count)
                     ? out->channel_offset + i * KH1_MSET_CHANNEL_SIZE
                     : out->aux_channel_offset + (i - out->channel_count) * KH1_MSET_CHANNEL_SIZE;
        uint32_t key_count;
        uint32_t key_index;
        if (read_at(p->f, abs + off, b, sizeof(b)) < 0) return KH1_MSET_BAD_MOTION;
        key_count = b[3];
        key_index = load_u16le(b + 4u);
        if (key_index + key_count > max_key) max_key = key_index + key_count;
    }
    if (max_key > 0u) {
        uint32_t curve_limit = raw_size;
        if (out->static_transform_count > 0u && out->static_transform_offset < curve_limit) curve_limit = out->static_transform_offset;
        if (out->tail_offset != 0xffffffffu && out->tail_offset < curve_limit) curve_limit = out->tail_offset;
        if (out->curve_offset == 0xffffffffu ||
            (uint64_t)out->curve_offset + (uint64_t)max_key * KH1_MSET_KEY_SIZE > curve_limit) {
            return KH1_MSET_BAD_MOTION;
        }
    }
    out->key_count = max_key;
    return KH1_MSET_OK;
}

int kh1_mset_scan(const char *path,
                  Kh1MsetInfo *out_info,
                  Kh1MsetMotionCallback callback,
                  void *user) {
    ParsedMset p;
    uint32_t i;
    int rc = parsed_open(path, &p);
    if (rc != KH1_MSET_OK) return rc;
    if (out_info) {
        out_info->section_count = p.section_count;
        out_info->motion_count = p.motion_count;
        out_info->mmtn_offset = p.mmtn_offset;
        out_info->motion_table_offset = p.motion_table_abs;
    }
    for (i = 0; i < p.motion_count; ++i) {
        Kh1MsetMotionInfo info;
        rc = read_motion_info(&p, i, &info);
        if (rc != KH1_MSET_OK) break;
        if (callback && callback(&info, user) != 0) { rc = -13; break; }
    }
    parsed_close(&p);
    return rc;
}

int kh1_mset_get_motion_info(const char *path,
                             uint32_t motion_index,
                             Kh1MsetMotionInfo *out_motion) {
    ParsedMset p;
    int rc;
    if (!out_motion) return KH1_MSET_BAD_ARGUMENT;
    rc = parsed_open(path, &p);
    if (rc != KH1_MSET_OK) return rc;
    rc = read_motion_info(&p, motion_index, out_motion);
    parsed_close(&p);
    return rc;
}

static void apply_transform(Kh1MdlsJointInfo *j, uint16_t type, float value) {
    if (!j) return;
    switch (type) {
        case 1u: j->scale_x = value; break;
        case 2u: j->scale_y = value; break;
        case 3u: j->scale_z = value; break;
        case 4u: j->rotate_x = value; break;
        case 5u: j->rotate_y = value; break;
        case 6u: j->rotate_z = value; break;
        case 7u: j->translate_x = value; break;
        case 8u: j->translate_y = value; break;
        case 9u: j->translate_z = value; break;
        default: break;
    }
}

static int read_key(FILE *f, uint32_t abs, Kh1MsetKeyframe *out) {
    unsigned char b[KH1_MSET_KEY_SIZE];
    if (!out || read_at(f, abs, b, sizeof(b)) < 0) return -1;
    out->raw_interpolation_type = load_u16le(b + 0u);
    out->frame = load_u16le(b + 2u);
    out->value = load_f32le(b + 4u);
    out->tangent_in = load_f32le(b + 8u);
    out->tangent_out = load_f32le(b + 12u);
    return 0;
}

static float hermite(float p0, float m0, float p1, float m1, float t, float dt) {
    float t2 = t * t;
    float t3 = t2 * t;
    float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
    float h10 = t3 - 2.0f * t2 + t;
    float h01 = -2.0f * t3 + 3.0f * t2;
    float h11 = t3 - t2;
    return h00 * p0 + h10 * (m0 * dt) + h01 * p1 + h11 * (m1 * dt);
}

static float eval_key_pair(const Kh1MsetKeyframe *a, const Kh1MsetKeyframe *b, float frame, int *ok) {
    float dt;
    uint16_t mode_a, mode_b;
    if (!a || !b || !ok) return 0.0f;
    dt = (float)b->frame - (float)a->frame;
    if (dt <= 0.0f) { *ok = 1; return b->value; }
    mode_a = (uint16_t)(a->raw_interpolation_type & 0x00ffu);
    mode_b = (uint16_t)(b->raw_interpolation_type & 0x00ffu);
    if (mode_a == 2u && mode_b == 2u &&
        (a->raw_interpolation_type & 0xff00u) == 0u &&
        (b->raw_interpolation_type & 0xff00u) == 0u) {
        float t = (frame - (float)a->frame) / dt;
        *ok = 1;
        return hermite(a->value, a->tangent_out, b->value, b->tangent_in, t, dt);
    }
    *ok = 0;
    return 0.0f;
}

static int eval_clip_channel(const Kh1MsetClip *clip, const Kh1MsetChannelInfo *ch,
                             float frame, float *out_value) {
    uint32_t i;
    const Kh1MsetKeyframe *a;
    if (!clip || !ch || !out_value || ch->key_count == 0u ||
        (uint32_t)ch->key_index + ch->key_count > clip->info.key_count) return KH1_MSET_BAD_MOTION;
    a = &clip->keys[ch->key_index];
    if (frame <= (float)a->frame || ch->key_count == 1u) { *out_value = a->value; return KH1_MSET_OK; }
    for (i = 1u; i < ch->key_count; ++i) {
        const Kh1MsetKeyframe *b = &clip->keys[(uint32_t)ch->key_index + i];
        if (frame <= (float)b->frame) {
            int ok = 0;
            *out_value = eval_key_pair(a, b, frame, &ok);
            return ok ? KH1_MSET_OK : KH1_MSET_UNSUPPORTED_CURVE;
        }
        a = b;
    }
    *out_value = a->value;
    return KH1_MSET_OK;
}

void kh1_mset_free_clip(Kh1MsetClip *clip) {
    if (!clip) return;
    free(clip->channels);
    free(clip->keys);
    free(clip->static_transforms);
    memset(clip, 0, sizeof(*clip));
}

int kh1_mset_load_clip(const char *path, uint32_t motion_index, Kh1MsetClip *out_clip) {
    ParsedMset p;
    Kh1MsetMotionInfo m;
    uint32_t motion_abs;
    uint32_t i;
    int rc;
    if (!path || !out_clip) return KH1_MSET_BAD_ARGUMENT;
    memset(out_clip, 0, sizeof(*out_clip));
    rc = parsed_open(path, &p);
    if (rc != KH1_MSET_OK) return rc;
    rc = read_motion_info(&p, motion_index, &m);
    if (rc != KH1_MSET_OK) { parsed_close(&p); return rc; }
    motion_abs = p.motion_table_abs + m.relative_offset;
    out_clip->info = m;
    out_clip->channels = (Kh1MsetChannelInfo *)calloc(m.channel_count, sizeof(*out_clip->channels));
    out_clip->keys = (Kh1MsetKeyframe *)calloc(m.key_count, sizeof(*out_clip->keys));
    out_clip->static_transforms = (Kh1MsetStaticTransform *)calloc(m.static_transform_count, sizeof(*out_clip->static_transforms));
    if ((m.channel_count && !out_clip->channels) || (m.key_count && !out_clip->keys) ||
        (m.static_transform_count && !out_clip->static_transforms)) {
        kh1_mset_free_clip(out_clip); parsed_close(&p); return KH1_MSET_BAD_FILE;
    }
    for (i = 0u; i < m.channel_count; ++i) {
        unsigned char b[KH1_MSET_CHANNEL_SIZE];
        Kh1MsetChannelInfo *ch = &out_clip->channels[i];
        if (read_at(p.f, motion_abs + m.channel_offset + i * KH1_MSET_CHANNEL_SIZE, b, sizeof(b)) < 0) {
            kh1_mset_free_clip(out_clip); parsed_close(&p); return KH1_MSET_BAD_MOTION;
        }
        ch->joint_id = load_u16le(b + 0u);
        ch->raw_transform_type = b[2];
        ch->key_count = b[3];
        ch->key_index = load_u16le(b + 4u);
    }
    for (i = 0u; i < m.key_count; ++i) {
        if (read_key(p.f, motion_abs + m.curve_offset + i * KH1_MSET_KEY_SIZE, &out_clip->keys[i]) < 0) {
            kh1_mset_free_clip(out_clip); parsed_close(&p); return KH1_MSET_BAD_MOTION;
        }
    }
    for (i = 0u; i < m.static_transform_count; ++i) {
        unsigned char b[KH1_MSET_STATIC_SIZE];
        Kh1MsetStaticTransform *st = &out_clip->static_transforms[i];
        if (read_at(p.f, motion_abs + m.static_transform_offset + i * KH1_MSET_STATIC_SIZE, b, sizeof(b)) < 0) {
            kh1_mset_free_clip(out_clip); parsed_close(&p); return KH1_MSET_BAD_MOTION;
        }
        st->joint_id = load_u16le(b + 0u);
        st->transform_type = load_u16le(b + 2u);
        st->value = load_f32le(b + 4u);
    }
    parsed_close(&p);
    return KH1_MSET_OK;
}

int kh1_mset_evaluate_clip(const Kh1MsetClip *clip,
                           float frame,
                           const Kh1MdlsJointInfo *base_joints,
                           uint32_t joint_count,
                           Kh1MdlsJointInfo *out_joints) {
    uint32_t i;
    if (!clip || !base_joints || !out_joints || joint_count == 0u || clip->info.joint_count != joint_count) {
        return KH1_MSET_BAD_ARGUMENT;
    }
    memcpy(out_joints, base_joints, (size_t)joint_count * sizeof(*out_joints));
    if (!isfinite(frame)) frame = 0.0f;
    if (frame < 0.0f) frame = 0.0f;
    if (frame > (float)clip->info.frame_count) frame = (float)clip->info.frame_count;
    for (i = 0u; i < clip->info.static_transform_count; ++i) {
        const Kh1MsetStaticTransform *st = &clip->static_transforms[i];
        if (st->joint_id >= joint_count || st->transform_type < 1u || st->transform_type > 9u) return KH1_MSET_BAD_MOTION;
        apply_transform(&out_joints[st->joint_id], st->transform_type, st->value);
    }
    for (i = 0u; i < clip->info.channel_count; ++i) {
        const Kh1MsetChannelInfo *ch = &clip->channels[i];
        float value;
        int rc;
        if (ch->joint_id >= joint_count || ch->raw_transform_type < 1u || ch->raw_transform_type > 9u) {
            return (ch->raw_transform_type > 9u) ? KH1_MSET_UNSUPPORTED_CURVE : KH1_MSET_BAD_MOTION;
        }
        rc = eval_clip_channel(clip, ch, frame, &value);
        if (rc != KH1_MSET_OK) return rc;
        apply_transform(&out_joints[ch->joint_id], ch->raw_transform_type, value);
    }
    return KH1_MSET_OK;
}

int kh1_mset_evaluate_motion(const char *path,
                             uint32_t motion_index,
                             float frame,
                             const Kh1MdlsJointInfo *base_joints,
                             uint32_t joint_count,
                             Kh1MdlsJointInfo *out_joints,
                             Kh1MsetMotionInfo *out_motion) {
    Kh1MsetClip clip;
    int rc;
    rc = kh1_mset_load_clip(path, motion_index, &clip);
    if (rc != KH1_MSET_OK) return rc;
    rc = kh1_mset_evaluate_clip(&clip, frame, base_joints, joint_count, out_joints);
    if (out_motion) *out_motion = clip.info;
    kh1_mset_free_clip(&clip);
    return rc;
}
