#include "kh1vita/kh1_mdls_texture.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

static uint32_t fnv1a(const unsigned char *p, uint32_t n) {
    uint32_t h = 2166136261u;
    uint32_t i;
    for (i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

int main(int argc, char **argv) {
    Kh1MdlsDecodedTextureInfo info;
    unsigned char *rgba;
    uint32_t hash;
    int rc;
    if (argc != 2) return 2;
    rc = kh1_mdls_decode_texture_rgba(argv[1], 0, NULL, 0, &info);
    if (rc != 0) return 3;
    rgba = (unsigned char *)malloc(info.rgba_size);
    if (!rgba) return 4;
    rc = kh1_mdls_decode_texture_rgba(argv[1], 0, rgba, info.rgba_size, &info);
    hash = fnv1a(rgba, info.rgba_size);
    printf("TEXTURE rc=%d %ux%u bytes=%u fnv1a=%08x first=%02x%02x%02x%02x\n",
           rc, (unsigned)info.width, (unsigned)info.height, (unsigned)info.rgba_size,
           (unsigned)hash, rgba[0], rgba[1], rgba[2], rgba[3]);
    free(rgba);
    if (rc != 0 || info.width != 128u || info.height != 128u || hash != 0x7effca0cu) return 5;
    return 0;
}
