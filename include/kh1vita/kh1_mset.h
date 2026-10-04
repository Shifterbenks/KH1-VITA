#pragma once

#include <stdint.h>
#include "kh1vita/kh1_mdls_skeleton.h"

typedef struct Kh1MsetInfo {
    uint32_t section_count;
    uint32_t motion_count;
    uint32_t mmtn_offset;
    uint32_t motion_table_offset;
} Kh1MsetInfo;

typedef struct Kh1MsetMotionInfo {
    uint32_t motion_index;
    uint32_t relative_offset;
    uint32_t raw_size;
    uint32_t frame_count;
    float frames_per_second;
    uint32_t flags;
    uint32_t joint_count;
    uint32_t header_size;

    /* Motion-local sections confirmed against KH1 Final Mix data. */
    uint32_t channel_count;
    uint32_t channel_offset;
    uint32_t aux_channel_count;
    uint32_t aux_channel_offset;
    uint32_t curve_offset;
    uint32_t aux_joint_count;
    uint32_t aux_joint_offset;
    uint32_t static_transform_count;
    uint32_t static_transform_offset;
    uint32_t tail_offset;
    uint32_t key_count;
} Kh1MsetMotionInfo;

typedef struct Kh1MsetChannelInfo {
    uint16_t joint_id;
    uint8_t raw_transform_type;
    uint8_t key_count;
    uint16_t key_index;
} Kh1MsetChannelInfo;

typedef struct Kh1MsetKeyframe {
    uint16_t raw_interpolation_type;
    uint16_t frame;
    float value;
    float tangent_in;
    float tangent_out;
} Kh1MsetKeyframe;

typedef struct Kh1MsetStaticTransform {
    uint16_t joint_id;
    uint16_t transform_type;
    float value;
} Kh1MsetStaticTransform;


typedef struct Kh1MsetClip {
    Kh1MsetMotionInfo info;
    Kh1MsetChannelInfo *channels;
    Kh1MsetKeyframe *keys;
    Kh1MsetStaticTransform *static_transforms;
} Kh1MsetClip;

typedef int (*Kh1MsetMotionCallback)(const Kh1MsetMotionInfo *motion, void *user);

enum {
    KH1_MSET_OK = 0,
    KH1_MSET_BAD_ARGUMENT = -1,
    KH1_MSET_OPEN_FAILED = -2,
    KH1_MSET_BAD_FILE = -3,
    KH1_MSET_BAD_MOTION = -4,
    KH1_MSET_UNSUPPORTED_CURVE = -5
};

/* Parse the KH1 PS2 MSET/MMTN motion table. */
int kh1_mset_scan(const char *path,
                  Kh1MsetInfo *out_info,
                  Kh1MsetMotionCallback callback,
                  void *user);

/* Read one motion's validated layout and section counts. */
int kh1_mset_get_motion_info(const char *path,
                             uint32_t motion_index,
                             Kh1MsetMotionInfo *out_motion);

/*
 * Evaluate the main skeleton channels of one KH1 motion at a frame.
 *
 * base_joints comes from kh1_mdls_read_bind_pose(). out_joints receives a
 * copy with static and animated transforms applied. The auxiliary 40-byte
 * control-rig section is parsed/validated but intentionally not applied yet.
 *
 * This milestone implements the unflagged transform channels and interpolation
 * type 2 (cubic Hermite), which fully covers xa_ex_0010 motion 0.
 */
int kh1_mset_evaluate_motion(const char *path,
                             uint32_t motion_index,
                             float frame,
                             const Kh1MdlsJointInfo *base_joints,
                             uint32_t joint_count,
                             Kh1MdlsJointInfo *out_joints,
                             Kh1MsetMotionInfo *out_motion);

/* Load one motion into RAM for real-time playback. */
int kh1_mset_load_clip(const char *path, uint32_t motion_index, Kh1MsetClip *out_clip);
void kh1_mset_free_clip(Kh1MsetClip *clip);
int kh1_mset_evaluate_clip(const Kh1MsetClip *clip,
                           float frame,
                           const Kh1MdlsJointInfo *base_joints,
                           uint32_t joint_count,
                           Kh1MdlsJointInfo *out_joints);
