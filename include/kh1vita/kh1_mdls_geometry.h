#pragma once

#include <stdint.h>

#define KH1_MDLS_MATRIX_SLOTS 16u

typedef struct Kh1MdlsMeshInfo {
    uint32_t mesh_index;
    uint32_t joint_count;
    uint32_t texture_index;
    uint32_t texture_index2;
    uint32_t packet_offset;
    uint32_t packet_size;
    uint32_t subpacket_count;
    uint32_t matrix_definition_count;
    uint32_t strip_count;
    uint32_t vertex_count;
    uint32_t triangle_count;
} Kh1MdlsMeshInfo;

typedef struct Kh1MdlsVertex {
    float normal_x;
    float normal_y;
    float normal_z;
    uint32_t matrix_id;
    float x;
    float y;
    float z;
    float weight;
    float u;
    float v;
    float texcoord_1;
    int32_t padding;
    uint32_t joint_id;
} Kh1MdlsVertex;

typedef struct Kh1MdlsTriangle {
    uint32_t a;
    uint32_t b;
    uint32_t c;
} Kh1MdlsTriangle;

typedef struct Kh1MdlsGeometryInfo {
    uint32_t mesh_count;
    uint32_t subpacket_count;
    uint32_t matrix_definition_count;
    uint32_t strip_count;
    uint32_t vertex_count;
    uint32_t triangle_count;
} Kh1MdlsGeometryInfo;

typedef struct Kh1MdlsGeometryCallbacks {
    int (*vertex)(uint32_t mesh_index,
                  uint32_t vertex_index,
                  const Kh1MdlsVertex *vertex,
                  void *user);
    int (*triangle)(uint32_t mesh_index,
                    uint32_t triangle_index,
                    const Kh1MdlsTriangle *triangle,
                    void *user);
    int (*mesh_done)(const Kh1MdlsMeshInfo *mesh, void *user);
} Kh1MdlsGeometryCallbacks;

/*
 * Decode the mesh packet stream used by KH1 PS2 MDLS files.
 *
 * This currently supports the single-weight packet form used by the selected
 * Final Mix test model. Multi-weight command packets are detected and return
 * KH1_MDLS_GEOMETRY_UNSUPPORTED_MULTIWEIGHT instead of being mis-decoded.
 *
 * Vertex and triangle indices are local to each mesh. The callback data is
 * valid only during the callback and should be copied if it needs to persist.
 */
enum {
    KH1_MDLS_GEOMETRY_OK = 0,
    KH1_MDLS_GEOMETRY_BAD_ARGUMENT = -1,
    KH1_MDLS_GEOMETRY_OPEN_FAILED = -2,
    KH1_MDLS_GEOMETRY_BAD_FILE = -3,
    KH1_MDLS_GEOMETRY_BAD_PACKET = -4,
    KH1_MDLS_GEOMETRY_UNSUPPORTED_MULTIWEIGHT = -5,
    KH1_MDLS_GEOMETRY_CALLBACK_ABORT = -6
};

int kh1_mdls_scan_geometry(const char *path,
                           const Kh1MdlsGeometryCallbacks *callbacks,
                           void *user,
                           Kh1MdlsGeometryInfo *out_info);
