#include "kh1vita/game_bridge.h"
#include "kh1vita/config.h"
#include "kh1vita/kh1_ard.h"
#include "kh1vita/kh1_mdls.h"
#include "kh1vita/kh1_mdls_geometry.h"
#include "kh1vita/kh1_mdls_skeleton.h"
#include "kh1vita/kh1_mdls_texture.h"
#include "kh1vita/kh1_mset.h"
#include "kh1vita/platform.h"
#include "kh1vita/renderer.h"

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_booted = 0;
static char g_first_mdls[64];
static Kh1VitaRenderVertex *g_vertices = NULL;
static Kh1VitaRenderTriangle *g_triangles = NULL;
static uint32_t g_vertex_count = 0;
static uint32_t g_triangle_count = 0;
static Kh1VitaRenderTexture *g_textures = NULL;
static uint32_t g_texture_count = 0;
static float g_yaw = 0.0f;

typedef struct AnimatedVertexSource {
    float x, y, z;
    float u, v;
    uint32_t joint_id;
} AnimatedVertexSource;

static AnimatedVertexSource *g_vertex_sources = NULL;
static Kh1MdlsJointInfo *g_base_joints = NULL;
static Kh1MdlsJointInfo *g_animated_joints = NULL;
static Kh1MdlsMat4 *g_pose = NULL;
static uint32_t g_joint_count = 0u;
static Kh1MsetClip g_motion_clip;
static int g_motion_loaded = 0;
static float g_motion_frame = 0.0f;

typedef struct MeshLayout {
    uint32_t vertex_count;
    uint32_t triangle_count;
    uint32_t vertex_base;
    uint32_t triangle_base;
    uint32_t texture_index;
} MeshLayout;

typedef struct SceneBuildContext {
    MeshLayout *layout;
    uint32_t mesh_count;
    Kh1MdlsMat4 *pose;
    AnimatedVertexSource *sources;
    uint32_t joint_count;
    Kh1VitaRenderVertex *vertices;
    Kh1VitaRenderTriangle *triangles;
    float min_v[3];
    float max_v[3];
} SceneBuildContext;

static int log_resource(uint32_t slot, const char *name, void *user) {
    const char *ext;
    (void)user;
    kh1vita_log("ARD resource[%u] = %s\n", (unsigned)slot, name);

    if (g_first_mdls[0] == '\0') {
        ext = strrchr(name, '.');
        if (ext && strcmp(ext, ".mdls") == 0) {
            snprintf(g_first_mdls, sizeof(g_first_mdls), "%s", name);
        }
    }
    return 0;
}

static int log_texture(uint32_t index, const Kh1MdlsTextureInfo *texture, void *user) {
    (void)user;
    if (!texture) return 0;
    kh1vita_log("MDLS texture[%u] = %ux%u, qwc=%u\n",
                (unsigned)index,
                (unsigned)texture->width,
                (unsigned)texture->height,
                (unsigned)texture->size_qwc);
    return 0;
}

static int scan_room(const char *room_name) {
    char path[256];
    Kh1ArdInfo info;
    int rc;

    if (!room_name) return -1;
    if (snprintf(path, sizeof(path), "%s/%s", KH1VITA_KINGDOM_ROOT, room_name) >= (int)sizeof(path)) {
        return -2;
    }

    g_first_mdls[0] = '\0';
    rc = kh1_ard_scan(path, &info, log_resource, 0);
    if (rc < 0) {
        kh1vita_log("game_bridge: impossible de parser %s (rc=%d)\n", path, rc);
        return rc;
    }

    kh1vita_log("game_bridge: %s valide, %u slots, %u ressources non vides, %u octets\n",
                room_name,
                (unsigned)info.resource_slot_count,
                (unsigned)info.resource_nonempty_count,
                (unsigned)info.file_size);
    return 0;
}

static int collect_mesh_layout(const Kh1MdlsMeshInfo *mesh, void *user) {
    SceneBuildContext *ctx = (SceneBuildContext *)user;
    MeshLayout *dst;
    if (!ctx || !mesh || mesh->mesh_index >= ctx->mesh_count) return 1;
    dst = &ctx->layout[mesh->mesh_index];
    dst->vertex_count = mesh->vertex_count;
    dst->triangle_count = mesh->triangle_count;
    dst->texture_index = mesh->texture_index;
    return 0;
}

static uint32_t mesh_color(uint32_t texture_index) {
    static const uint32_t colors[] = {
        0xFFFFFFFFu,
        0xFF70D0FFu,
        0xFFFFA060u,
        0xFF80FF90u,
        0xFFFF80D0u,
        0xFFD0A0FFu,
        0xFFFFFF70u,
    };
    return colors[texture_index % (sizeof(colors) / sizeof(colors[0]))];
}

static int build_vertex(uint32_t mesh_index,
                        uint32_t vertex_index,
                        const Kh1MdlsVertex *vertex,
                        void *user) {
    SceneBuildContext *ctx = (SceneBuildContext *)user;
    MeshLayout *layout;
    Kh1VitaRenderVertex *dst;
    uint32_t global_index;
    if (!ctx || !vertex || mesh_index >= ctx->mesh_count) return 1;
    if (vertex->joint_id >= ctx->joint_count) return 1;
    layout = &ctx->layout[mesh_index];
    if (vertex_index >= layout->vertex_count) return 1;
    global_index = layout->vertex_base + vertex_index;
    dst = &ctx->vertices[global_index];
    if (!ctx->sources) return 1;
    ctx->sources[global_index].x = vertex->x;
    ctx->sources[global_index].y = vertex->y;
    ctx->sources[global_index].z = vertex->z;
    ctx->sources[global_index].u = vertex->u;
    ctx->sources[global_index].v = vertex->v;
    ctx->sources[global_index].joint_id = vertex->joint_id;
    kh1_mdls_transform_point(&ctx->pose[vertex->joint_id],
                             vertex->x, vertex->y, vertex->z,
                             &dst->x, &dst->y, &dst->z);
    dst->u = vertex->u;
    dst->v = vertex->v;

    if (dst->x < ctx->min_v[0]) ctx->min_v[0] = dst->x;
    if (dst->y < ctx->min_v[1]) ctx->min_v[1] = dst->y;
    if (dst->z < ctx->min_v[2]) ctx->min_v[2] = dst->z;
    if (dst->x > ctx->max_v[0]) ctx->max_v[0] = dst->x;
    if (dst->y > ctx->max_v[1]) ctx->max_v[1] = dst->y;
    if (dst->z > ctx->max_v[2]) ctx->max_v[2] = dst->z;
    return 0;
}

static int build_triangle(uint32_t mesh_index,
                          uint32_t triangle_index,
                          const Kh1MdlsTriangle *triangle,
                          void *user) {
    SceneBuildContext *ctx = (SceneBuildContext *)user;
    MeshLayout *layout;
    Kh1VitaRenderTriangle *dst;
    if (!ctx || !triangle || mesh_index >= ctx->mesh_count) return 1;
    layout = &ctx->layout[mesh_index];
    if (triangle_index >= layout->triangle_count ||
        triangle->a >= layout->vertex_count ||
        triangle->b >= layout->vertex_count ||
        triangle->c >= layout->vertex_count) return 1;
    dst = &ctx->triangles[layout->triangle_base + triangle_index];
    dst->a = layout->vertex_base + triangle->a;
    dst->b = layout->vertex_base + triangle->b;
    dst->c = layout->vertex_base + triangle->c;
    dst->texture_index = layout->texture_index;
    dst->color = mesh_color(layout->texture_index);
    return 0;
}

static void free_scene(void) {
    uint32_t i;
    free(g_vertices);
    free(g_triangles);
    free(g_vertex_sources);
    free(g_base_joints);
    free(g_animated_joints);
    free(g_pose);
    if (g_motion_loaded) kh1_mset_free_clip(&g_motion_clip);
    if (g_textures) {
        for (i = 0; i < g_texture_count; ++i) free((void *)g_textures[i].pixels);
    }
    free(g_textures);
    g_vertices = NULL;
    g_triangles = NULL;
    g_vertex_sources = NULL;
    g_base_joints = NULL;
    g_animated_joints = NULL;
    g_pose = NULL;
    g_textures = NULL;
    g_vertex_count = 0;
    g_triangle_count = 0;
    g_texture_count = 0;
    g_joint_count = 0;
    g_motion_loaded = 0;
    g_motion_frame = 0.0f;
    memset(&g_motion_clip, 0, sizeof(g_motion_clip));
}

static int build_wireframe_scene(const char *path) {
    Kh1MdlsGeometryInfo geom;
    Kh1MdlsGeometryCallbacks callbacks;
    MeshLayout *layout = NULL;
    SceneBuildContext ctx;
    uint32_t joint_count = 0;
    uint32_t i;
    uint32_t vb = 0;
    uint32_t tb = 0;
    int rc;

    memset(&geom, 0, sizeof(geom));
    rc = kh1_mdls_scan_geometry(path, NULL, NULL, &geom);
    if (rc != 0 || geom.mesh_count == 0u || geom.vertex_count == 0u || geom.triangle_count == 0u) {
        kh1vita_log("game_bridge: geometry sizing failed rc=%d\n", rc);
        return rc ? rc : -20;
    }

    layout = (MeshLayout *)calloc(geom.mesh_count, sizeof(*layout));
    if (!layout) return -21;
    memset(&ctx, 0, sizeof(ctx));
    ctx.layout = layout;
    ctx.mesh_count = geom.mesh_count;
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.mesh_done = collect_mesh_layout;
    rc = kh1_mdls_scan_geometry(path, &callbacks, &ctx, NULL);
    if (rc != 0) goto done;

    for (i = 0; i < geom.mesh_count; ++i) {
        layout[i].vertex_base = vb;
        layout[i].triangle_base = tb;
        vb += layout[i].vertex_count;
        tb += layout[i].triangle_count;
    }
    if (vb != geom.vertex_count || tb != geom.triangle_count) {
        rc = -22;
        goto done;
    }

    rc = kh1_mdls_read_bind_pose(path, NULL, NULL, 0, &joint_count);
    if (rc != 0 || joint_count == 0u) goto done;

    free_scene();
    g_vertices = (Kh1VitaRenderVertex *)calloc(geom.vertex_count, sizeof(*g_vertices));
    g_triangles = (Kh1VitaRenderTriangle *)calloc(geom.triangle_count, sizeof(*g_triangles));
    g_vertex_sources = (AnimatedVertexSource *)calloc(geom.vertex_count, sizeof(*g_vertex_sources));
    g_base_joints = (Kh1MdlsJointInfo *)calloc(joint_count, sizeof(*g_base_joints));
    g_animated_joints = (Kh1MdlsJointInfo *)calloc(joint_count, sizeof(*g_animated_joints));
    g_pose = (Kh1MdlsMat4 *)calloc(joint_count, sizeof(*g_pose));
    if (!g_vertices || !g_triangles || !g_vertex_sources || !g_base_joints ||
        !g_animated_joints || !g_pose) { rc = -24; goto done; }
    g_vertex_count = geom.vertex_count;
    g_triangle_count = geom.triangle_count;
    g_joint_count = joint_count;
    rc = kh1_mdls_read_bind_pose(path, g_base_joints, g_pose, joint_count, &g_joint_count);
    if (rc != 0) goto done;

    ctx.pose = g_pose;
    ctx.sources = g_vertex_sources;
    ctx.joint_count = g_joint_count;
    ctx.vertices = g_vertices;
    ctx.triangles = g_triangles;
    ctx.min_v[0] = ctx.min_v[1] = ctx.min_v[2] = FLT_MAX;
    ctx.max_v[0] = ctx.max_v[1] = ctx.max_v[2] = -FLT_MAX;
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.vertex = build_vertex;
    callbacks.triangle = build_triangle;
    rc = kh1_mdls_scan_geometry(path, &callbacks, &ctx, NULL);
    if (rc != 0) {
        free_scene();
        goto done;
    }

    kh1vita_log("game_bridge: geometry decoded: meshes=%u verts=%u tris=%u joints=%u\n",
                (unsigned)geom.mesh_count,
                (unsigned)geom.vertex_count,
                (unsigned)geom.triangle_count,
                (unsigned)joint_count);
    kh1vita_log("game_bridge: bind bounds min=<%.3f %.3f %.3f> max=<%.3f %.3f %.3f>\n",
                ctx.min_v[0], ctx.min_v[1], ctx.min_v[2],
                ctx.max_v[0], ctx.max_v[1], ctx.max_v[2]);

done:
    free(layout);
    if (rc != 0) free_scene();
    return rc;
}

static int load_model_textures(const char *path, uint32_t texture_count) {
    uint32_t i;
    Kh1VitaRenderTexture *textures;
    if (!path || texture_count == 0u) return -30;
    textures = (Kh1VitaRenderTexture *)calloc(texture_count, sizeof(*textures));
    if (!textures) return -31;
    for (i = 0; i < texture_count; ++i) {
        Kh1MdlsDecodedTextureInfo info;
        unsigned char *rgba;
        int rc = kh1_mdls_decode_texture_rgba(path, i, NULL, 0u, &info);
        if (rc != 0 || info.rgba_size == 0u) {
            uint32_t j;
            for (j = 0; j < i; ++j) free((void *)textures[j].pixels);
            free(textures);
            return rc ? rc : -32;
        }
        rgba = (unsigned char *)malloc(info.rgba_size);
        if (!rgba) {
            uint32_t j;
            for (j = 0; j < i; ++j) free((void *)textures[j].pixels);
            free(textures);
            return -33;
        }
        rc = kh1_mdls_decode_texture_rgba(path, i, rgba, info.rgba_size, &info);
        if (rc != 0) {
            uint32_t j;
            free(rgba);
            for (j = 0; j < i; ++j) free((void *)textures[j].pixels);
            free(textures);
            return rc;
        }
        textures[i].width = info.width;
        textures[i].height = info.height;
        textures[i].pixels = (const uint32_t *)rgba;
    }
    g_textures = textures;
    g_texture_count = texture_count;
    return 0;
}

typedef struct MsetLogCtx {
    uint32_t expected_joints;
    uint32_t motion_count;
    uint32_t min_frames;
    uint32_t max_frames;
} MsetLogCtx;

static int log_motion(const Kh1MsetMotionInfo *motion, void *user) {
    MsetLogCtx *ctx = (MsetLogCtx *)user;
    if (!motion || !ctx) return 1;
    if (motion->joint_count != ctx->expected_joints) return 1;
    if (ctx->motion_count == 0u || motion->frame_count < ctx->min_frames) ctx->min_frames = motion->frame_count;
    if (motion->frame_count > ctx->max_frames) ctx->max_frames = motion->frame_count;
    ctx->motion_count++;
    return 0;
}

static int scan_matching_mset(uint32_t expected_joints) {
    char name[64];
    char path[256];
    char *ext;
    Kh1MsetInfo info;
    MsetLogCtx ctx;
    int rc;
    if (g_first_mdls[0] == '\0') return -40;
    snprintf(name, sizeof(name), "%s", g_first_mdls);
    ext = strrchr(name, '.');
    if (!ext) return -41;
    snprintf(ext, (size_t)(name + sizeof(name) - ext), ".mset");
    if (snprintf(path, sizeof(path), "%s/%s", KH1VITA_KINGDOM_ROOT, name) >= (int)sizeof(path)) return -42;
    memset(&ctx, 0, sizeof(ctx));
    ctx.expected_joints = expected_joints;
    rc = kh1_mset_scan(path, &info, log_motion, &ctx);
    if (rc != 0) {
        kh1vita_log("game_bridge: MSET %s parse failed rc=%d\n", name, rc);
        return rc;
    }
    kh1vita_log("game_bridge: MSET %s motions=%u frames=%u..%u joints=%u\n",
                name, (unsigned)info.motion_count, (unsigned)ctx.min_frames,
                (unsigned)ctx.max_frames, (unsigned)expected_joints);
    if (g_motion_loaded) kh1_mset_free_clip(&g_motion_clip);
    rc = kh1_mset_load_clip(path, 0u, &g_motion_clip);
    if (rc != KH1_MSET_OK) {
        kh1vita_log("game_bridge: motion 0 load failed rc=%d\n", rc);
        g_motion_loaded = 0;
        return rc;
    }
    g_motion_loaded = 1;
    g_motion_frame = 0.0f;
    kh1vita_log("game_bridge: motion 0 RAM clip: frames=%u channels=%u static=%u keys=%u\n",
                (unsigned)g_motion_clip.info.frame_count,
                (unsigned)g_motion_clip.info.channel_count,
                (unsigned)g_motion_clip.info.static_transform_count,
                (unsigned)g_motion_clip.info.key_count);
    return 0;
}

static int scan_first_model(void) {
    char path[256];
    Kh1MdlsInfo info;
    int rc;

    if (g_first_mdls[0] == '\0') return -1;
    if (snprintf(path, sizeof(path), "%s/%s", KH1VITA_KINGDOM_ROOT, g_first_mdls) >= (int)sizeof(path)) {
        return -2;
    }

    rc = kh1_mdls_scan(path, &info, log_texture, 0);
    if (rc < 0) {
        kh1vita_log("game_bridge: MDLS invalide %s (rc=%d)\n", path, rc);
        return rc;
    }

    kh1vita_log("game_bridge: %s valide: joints=%u meshes=%u textures=%u model_size=%u\n",
                g_first_mdls,
                (unsigned)info.joint_count,
                (unsigned)info.mesh_count,
                (unsigned)info.texture_count,
                (unsigned)info.model_size);

    rc = build_wireframe_scene(path);
    if (rc == 0) rc = load_model_textures(path, info.texture_count);
    if (rc == 0) rc = scan_matching_mset(info.joint_count);
    return rc;
}

int kh1vita_game_bridge_boot(void) {
    int rc;

    rc = scan_room("di08.ard");
    if (rc == 0) rc = scan_first_model();

    g_booted = (rc == 0 && g_vertices && g_triangles);
    g_yaw = 0.0f;
    if (g_booted) {
        kh1vita_log("game_bridge: textured animated Sora test ready.\n");
    }
    return rc;
}

static int update_animation(void) {
    uint32_t i;
    int rc;
    if (!g_motion_loaded || !g_base_joints || !g_animated_joints || !g_pose || !g_vertex_sources) return -1;
    rc = kh1_mset_evaluate_clip(&g_motion_clip, g_motion_frame,
                                g_base_joints, g_joint_count, g_animated_joints);
    if (rc != KH1_MSET_OK) return rc;
    rc = kh1_mdls_build_pose(g_animated_joints, g_joint_count, g_pose, g_joint_count);
    if (rc != 0) return rc;
    for (i = 0u; i < g_vertex_count; ++i) {
        const AnimatedVertexSource *src = &g_vertex_sources[i];
        Kh1VitaRenderVertex *dst = &g_vertices[i];
        if (src->joint_id >= g_joint_count) return -2;
        kh1_mdls_transform_point(&g_pose[src->joint_id], src->x, src->y, src->z,
                                 &dst->x, &dst->y, &dst->z);
        dst->u = src->u;
        dst->v = src->v;
    }
    return 0;
}

void kh1vita_game_bridge_tick(void) {
    int rc;
    if (!g_booted) return;
    rc = update_animation();
    if (rc != 0) {
        kh1vita_log("game_bridge: animation stopped rc=%d\n", rc);
        g_motion_loaded = 0;
    }
    kh1vita_renderer_textured(g_vertices, g_vertex_count,
                              g_triangles, g_triangle_count,
                              g_textures, g_texture_count, g_yaw);
    if (g_motion_loaded) {
        g_motion_frame += g_motion_clip.info.frames_per_second / 60.0f;
        if (g_motion_frame >= (float)g_motion_clip.info.frame_count) g_motion_frame = 0.0f;
    }
    g_yaw += 0.002f;
    if (g_yaw > 6.28318530718f) g_yaw -= 6.28318530718f;
}

void kh1vita_game_bridge_shutdown(void) {
    if (g_booted) kh1vita_log("game_bridge: shutdown animated scene.\n");
    free_scene();
    g_booted = 0;
}
