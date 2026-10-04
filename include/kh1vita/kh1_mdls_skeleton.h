#pragma once

#include <stdint.h>

typedef struct Kh1MdlsMat4 {
    float m[16];
} Kh1MdlsMat4;

typedef struct Kh1MdlsJointInfo {
    float scale_x;
    float scale_y;
    float scale_z;
    uint32_t index;
    float rotate_x;
    float rotate_y;
    float rotate_z;
    float translate_x;
    float translate_y;
    float translate_z;
    uint32_t hierarchy_bits;
    uint32_t parent_id;
} Kh1MdlsJointInfo;

/* Returns the number of joints through out_count. If out_joints/out_bind_pose
 * are NULL, the function can be used as a sizing query. */
int kh1_mdls_read_bind_pose(const char *path,
                            Kh1MdlsJointInfo *out_joints,
                            Kh1MdlsMat4 *out_bind_pose,
                            uint32_t capacity,
                            uint32_t *out_count);

int kh1_mdls_build_pose(const Kh1MdlsJointInfo *joints,
                        uint32_t joint_count,
                        Kh1MdlsMat4 *out_pose,
                        uint32_t capacity);

void kh1_mdls_transform_point(const Kh1MdlsMat4 *matrix,
                              float x, float y, float z,
                              float *out_x, float *out_y, float *out_z);
