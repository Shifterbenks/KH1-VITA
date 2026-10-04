#include "kh1vita/kh1_mset.h"
#include <math.h>
#include <stdio.h>

typedef struct TestCtx {
    uint32_t seen;
    uint32_t expected_joints;
    uint32_t min_frames;
    uint32_t max_frames;
} TestCtx;

static int on_motion(const Kh1MsetMotionInfo *m, void *user) {
    TestCtx *ctx = (TestCtx *)user;
    if (!m || !ctx) return 1;
    if (m->joint_count != ctx->expected_joints) return 1;
    if (fabsf(m->frames_per_second - 60.0f) > 0.001f) return 1;
    if (ctx->seen == 0u || m->frame_count < ctx->min_frames) ctx->min_frames = m->frame_count;
    if (m->frame_count > ctx->max_frames) ctx->max_frames = m->frame_count;
    ctx->seen++;
    return 0;
}

int main(int argc, char **argv) {
    Kh1MsetInfo info;
    TestCtx ctx = {0u, 293u, 0u, 0u};
    int rc;
    if (argc != 2) return 2;
    rc = kh1_mset_scan(argv[1], &info, on_motion, &ctx);
    printf("MSET rc=%d motions=%u seen=%u min_frames=%u max_frames=%u mmtn=0x%x table=0x%x\n",
           rc, (unsigned)info.motion_count, (unsigned)ctx.seen,
           (unsigned)ctx.min_frames, (unsigned)ctx.max_frames,
           (unsigned)info.mmtn_offset, (unsigned)info.motion_table_offset);
    if (rc != 0) return 3;
    if (info.motion_count != 118u || ctx.seen != 118u) return 4;
    return 0;
}
