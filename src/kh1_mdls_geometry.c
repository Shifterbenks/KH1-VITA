#include "kh1vita/kh1_mdls_geometry.h"
#include "kh1vita/kh1_mdls.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KH1_MDLS_HEADER_U32_COUNT 14u
#define KH1_MDLS_HEADER_SIZE (8u + KH1_MDLS_HEADER_U32_COUNT * 4u)
#define KH1_MDLS_MODEL_HEADER_SIZE 16u
#define KH1_MDLS_MESH_HEADER_SIZE 16u
#define KH1_MDLS_PACKET_HEADER_SIZE 16u
#define KH1_MDLS_MATRIX_COMMAND_SIZE 0x80u
#define KH1_MDLS_STRIP_HEADER_SIZE 32u
#define KH1_MDLS_VERTEX_SIZE 48u
#define KH1_MDLS_PACKET_END_MARKER 0x00008000u
#define KH1_MDLS_PACKET_END_SIZE 0x20u

static uint16_t load_u16le(const unsigned char *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t load_u32le(const unsigned char *p) {
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static int32_t load_i32le(const unsigned char *p) {
    return (int32_t)load_u32le(p);
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

typedef struct InternalMeshHeader {
    uint32_t joint_count;
    uint32_t texture_index;
    uint32_t texture_index2;
    uint32_t packet_offset;
} InternalMeshHeader;

static int decode_packet(const unsigned char *packet,
                         uint32_t packet_size,
                         uint32_t mesh_index,
                         const InternalMeshHeader *header,
                         const Kh1MdlsGeometryCallbacks *cb,
                         void *user,
                         Kh1MdlsMeshInfo *mesh_info) {
    uint32_t pos = 0;
    uint32_t weight_matrix[KH1_MDLS_MATRIX_SLOTS];
    uint32_t vertex_index = 0;
    uint32_t triangle_index = 0;

    if (!packet || !header || !mesh_info) return KH1_MDLS_GEOMETRY_BAD_ARGUMENT;
    memset(weight_matrix, 0, sizeof(weight_matrix));
    memset(mesh_info, 0, sizeof(*mesh_info));
    mesh_info->mesh_index = mesh_index;
    mesh_info->joint_count = header->joint_count;
    mesh_info->texture_index = header->texture_index;
    mesh_info->texture_index2 = header->texture_index2;
    mesh_info->packet_offset = header->packet_offset;
    mesh_info->packet_size = packet_size;

    while (pos < packet_size) {
        if (packet_size - pos < KH1_MDLS_PACKET_HEADER_SIZE) {
            return KH1_MDLS_GEOMETRY_BAD_PACKET;
        }

        /*
         * 16-byte VIF/DMA-ish subpacket header. We do not need to emulate VIF;
         * the following payload is already a simple command stream for these
         * KH1 model packets.
         */
        (void)load_u16le(packet + pos);
        pos += KH1_MDLS_PACKET_HEADER_SIZE;
        mesh_info->subpacket_count++;

        for (;;) {
            uint32_t command;
            if (packet_size - pos < 4u) return KH1_MDLS_GEOMETRY_BAD_PACKET;
            command = load_u32le(packet + pos);

            if (command == KH1_MDLS_PACKET_END_MARKER) {
                if (packet_size - pos < KH1_MDLS_PACKET_END_SIZE) {
                    return KH1_MDLS_GEOMETRY_BAD_PACKET;
                }
                pos += KH1_MDLS_PACKET_END_SIZE;
                break;
            }

            if (command == 0u) {
                uint32_t joint_id;
                uint32_t table_index;
                uint32_t table_index_1;
                if (packet_size - pos < KH1_MDLS_MATRIX_COMMAND_SIZE) {
                    return KH1_MDLS_GEOMETRY_BAD_PACKET;
                }
                joint_id = load_u32le(packet + pos + 4u);
                table_index = load_u32le(packet + pos + 8u);
                table_index_1 = load_u32le(packet + pos + 12u);

                /* The currently understood single-weight form uses table 0. */
                if (table_index_1 != 0u) {
                    return KH1_MDLS_GEOMETRY_UNSUPPORTED_MULTIWEIGHT;
                }
                if (table_index >= KH1_MDLS_MATRIX_SLOTS) {
                    return KH1_MDLS_GEOMETRY_BAD_PACKET;
                }
                weight_matrix[table_index] = joint_id;
                mesh_info->matrix_definition_count++;
                pos += KH1_MDLS_MATRIX_COMMAND_SIZE;
                continue;
            }

            if (command == 1u) {
                uint32_t vertex_count;
                uint32_t winding;
                uint32_t vertex_count_2;
                uint32_t strip_start;
                uint32_t i;

                if (packet_size - pos < KH1_MDLS_STRIP_HEADER_SIZE) {
                    return KH1_MDLS_GEOMETRY_BAD_PACKET;
                }
                vertex_count = load_u32le(packet + pos + 8u);
                winding = load_u32le(packet + pos + 12u);
                vertex_count_2 = load_u32le(packet + pos + 16u);
                pos += KH1_MDLS_STRIP_HEADER_SIZE;

                if (vertex_count != vertex_count_2 ||
                    (uint64_t)vertex_count * KH1_MDLS_VERTEX_SIZE > packet_size - pos) {
                    return KH1_MDLS_GEOMETRY_BAD_PACKET;
                }

                strip_start = vertex_index;
                mesh_info->strip_count++;
                for (i = 0; i < vertex_count; ++i) {
                    const unsigned char *v = packet + pos;
                    Kh1MdlsVertex decoded;
                    memset(&decoded, 0, sizeof(decoded));
                    decoded.normal_x = load_f32le(v + 0u);
                    decoded.normal_y = load_f32le(v + 4u);
                    decoded.normal_z = load_f32le(v + 8u);
                    decoded.matrix_id = load_u32le(v + 12u);
                    decoded.x = load_f32le(v + 16u);
                    decoded.y = load_f32le(v + 20u);
                    decoded.z = load_f32le(v + 24u);
                    decoded.weight = load_f32le(v + 28u);
                    decoded.u = load_f32le(v + 32u);
                    decoded.v = load_f32le(v + 36u);
                    decoded.texcoord_1 = load_f32le(v + 40u);
                    decoded.padding = load_i32le(v + 44u);
                    if (decoded.matrix_id >= KH1_MDLS_MATRIX_SLOTS) {
                        return KH1_MDLS_GEOMETRY_BAD_PACKET;
                    }
                    decoded.joint_id = weight_matrix[decoded.matrix_id];

                    if (cb && cb->vertex && cb->vertex(mesh_index, vertex_index, &decoded, user) != 0) {
                        return KH1_MDLS_GEOMETRY_CALLBACK_ABORT;
                    }

                    if (i >= 2u) {
                        Kh1MdlsTriangle tri;
                        uint32_t current = strip_start + i;
                        if (winding == 0u) {
                            if (((i + 1u) & 1u) == 0u) {
                                tri.a = current;
                                tri.b = current - 1u;
                                tri.c = current - 2u;
                            } else {
                                tri.a = current;
                                tri.b = current - 2u;
                                tri.c = current - 1u;
                            }
                        } else {
                            if (((i + 1u) & 1u) == 0u) {
                                tri.a = current;
                                tri.b = current - 2u;
                                tri.c = current - 1u;
                            } else {
                                tri.a = current;
                                tri.b = current - 1u;
                                tri.c = current - 2u;
                            }
                        }
                        if (cb && cb->triangle &&
                            cb->triangle(mesh_index, triangle_index, &tri, user) != 0) {
                            return KH1_MDLS_GEOMETRY_CALLBACK_ABORT;
                        }
                        triangle_index++;
                    }

                    vertex_index++;
                    pos += KH1_MDLS_VERTEX_SIZE;
                }
                continue;
            }

            if (command == 2u) {
                return KH1_MDLS_GEOMETRY_UNSUPPORTED_MULTIWEIGHT;
            }

            return KH1_MDLS_GEOMETRY_BAD_PACKET;
        }
    }

    mesh_info->vertex_count = vertex_index;
    mesh_info->triangle_count = triangle_index;
    if (cb && cb->mesh_done && cb->mesh_done(mesh_info, user) != 0) {
        return KH1_MDLS_GEOMETRY_CALLBACK_ABORT;
    }
    return KH1_MDLS_GEOMETRY_OK;
}

int kh1_mdls_scan_geometry(const char *path,
                           const Kh1MdlsGeometryCallbacks *callbacks,
                           void *user,
                           Kh1MdlsGeometryInfo *out_info) {
    FILE *f;
    uint32_t file_size;
    unsigned char header_bytes[KH1_MDLS_HEADER_SIZE];
    uint32_t h[KH1_MDLS_HEADER_U32_COUNT];
    uint32_t model_abs;
    uint32_t joint_count;
    uint32_t joint_info_offset;
    uint32_t bone_data_offset;
    uint32_t mesh_count;
    InternalMeshHeader *meshes = NULL;
    Kh1MdlsGeometryInfo total;
    uint32_t i;
    int rc = KH1_MDLS_GEOMETRY_OK;

    (void)joint_count;
    (void)joint_info_offset;
    (void)bone_data_offset;

    if (!path) return KH1_MDLS_GEOMETRY_BAD_ARGUMENT;
    memset(&total, 0, sizeof(total));

    f = fopen(path, "rb");
    if (!f) return KH1_MDLS_GEOMETRY_OPEN_FAILED;
    if (file_size_u32(f, &file_size) < 0 ||
        file_size < KH1_MDLS_BASE_OFFSET + KH1_MDLS_HEADER_SIZE ||
        read_at(f, KH1_MDLS_BASE_OFFSET, header_bytes, sizeof(header_bytes)) < 0) {
        rc = KH1_MDLS_GEOMETRY_BAD_FILE;
        goto done;
    }
    if (load_u32le(header_bytes) != KH1_MDLS_MAGIC) {
        rc = KH1_MDLS_GEOMETRY_BAD_FILE;
        goto done;
    }
    for (i = 0; i < KH1_MDLS_HEADER_U32_COUNT; ++i) {
        h[i] = load_u32le(header_bytes + 8u + i * 4u);
    }

    if ((uint64_t)KH1_MDLS_BASE_OFFSET + h[6] + KH1_MDLS_MODEL_HEADER_SIZE > file_size ||
        (uint64_t)KH1_MDLS_BASE_OFFSET + h[8] > file_size) {
        rc = KH1_MDLS_GEOMETRY_BAD_FILE;
        goto done;
    }
    model_abs = KH1_MDLS_BASE_OFFSET + h[6];

    {
        unsigned char mh[KH1_MDLS_MODEL_HEADER_SIZE];
        if (read_at(f, model_abs, mh, sizeof(mh)) < 0) {
            rc = KH1_MDLS_GEOMETRY_BAD_FILE;
            goto done;
        }
        joint_count = load_u32le(mh + 0u);
        joint_info_offset = load_u32le(mh + 4u);
        bone_data_offset = load_u32le(mh + 8u);
        mesh_count = load_u32le(mh + 12u);
    }

    if (mesh_count == 0u || mesh_count > 4096u ||
        (uint64_t)model_abs + KH1_MDLS_MODEL_HEADER_SIZE +
            (uint64_t)mesh_count * KH1_MDLS_MESH_HEADER_SIZE > file_size) {
        rc = KH1_MDLS_GEOMETRY_BAD_FILE;
        goto done;
    }

    meshes = (InternalMeshHeader *)calloc(mesh_count, sizeof(*meshes));
    if (!meshes) {
        rc = KH1_MDLS_GEOMETRY_BAD_FILE;
        goto done;
    }

    for (i = 0; i < mesh_count; ++i) {
        unsigned char b[KH1_MDLS_MESH_HEADER_SIZE];
        uint32_t off = model_abs + KH1_MDLS_MODEL_HEADER_SIZE + i * KH1_MDLS_MESH_HEADER_SIZE;
        if (read_at(f, off, b, sizeof(b)) < 0) {
            rc = KH1_MDLS_GEOMETRY_BAD_FILE;
            goto done;
        }
        meshes[i].joint_count = load_u32le(b + 0u);
        meshes[i].texture_index = load_u32le(b + 4u);
        meshes[i].texture_index2 = load_u32le(b + 8u);
        meshes[i].packet_offset = load_u32le(b + 12u);
    }

    for (i = 0; i < mesh_count; ++i) {
        uint32_t packet_abs = model_abs + meshes[i].packet_offset;
        uint32_t packet_end = (i + 1u < mesh_count)
                            ? model_abs + meshes[i + 1u].packet_offset
                            : KH1_MDLS_BASE_OFFSET + h[8];
        uint32_t packet_size;
        unsigned char *packet;
        Kh1MdlsMeshInfo mesh_info;

        if (packet_abs > packet_end || packet_end > file_size) {
            rc = KH1_MDLS_GEOMETRY_BAD_FILE;
            goto done;
        }
        packet_size = packet_end - packet_abs;
        if (packet_size == 0u) {
            rc = KH1_MDLS_GEOMETRY_BAD_PACKET;
            goto done;
        }
        packet = (unsigned char *)malloc(packet_size);
        if (!packet) {
            rc = KH1_MDLS_GEOMETRY_BAD_FILE;
            goto done;
        }
        if (read_at(f, packet_abs, packet, packet_size) < 0) {
            free(packet);
            rc = KH1_MDLS_GEOMETRY_BAD_FILE;
            goto done;
        }

        rc = decode_packet(packet, packet_size, i, &meshes[i], callbacks, user, &mesh_info);
        free(packet);
        if (rc != KH1_MDLS_GEOMETRY_OK) goto done;

        total.mesh_count++;
        total.subpacket_count += mesh_info.subpacket_count;
        total.matrix_definition_count += mesh_info.matrix_definition_count;
        total.strip_count += mesh_info.strip_count;
        total.vertex_count += mesh_info.vertex_count;
        total.triangle_count += mesh_info.triangle_count;
    }

    if (out_info) *out_info = total;

done:
    free(meshes);
    fclose(f);
    return rc;
}
