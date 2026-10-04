#include "kh1vita/kh1_ard.h"

#include <stdio.h>
#include <string.h>

#define KH1_ARD_HEADER_SIZE (4u + ((KH1_ARD_ENTRY_COUNT + 1u) * 4u))
#define KH1_ARD_MAX_TRAILING_PAD 0x800u

static int read_u32le(FILE *f, uint32_t *out) {
    unsigned char b[4];
    if (!f || !out || fread(b, 1, sizeof(b), f) != sizeof(b)) return -1;
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

int kh1_ard_scan(const char *path,
                 Kh1ArdInfo *info,
                 Kh1ArdResourceVisitor visitor,
                 void *user) {
    FILE *f;
    uint32_t count;
    uint32_t offsets[KH1_ARD_ENTRY_COUNT + 1u];
    uint32_t file_size;
    uint32_t start;
    uint32_t end;
    uint32_t list_size;
    uint32_t slot_count;
    uint32_t nonempty = 0;
    uint32_t i;

    if (!path) return -1;
    f = fopen(path, "rb");
    if (!f) return -2;

    if (get_file_size(f, &file_size) < 0 || file_size < KH1_ARD_HEADER_SIZE) {
        fclose(f);
        return -3;
    }

    if (read_u32le(f, &count) < 0 || count != KH1_ARD_ENTRY_COUNT) {
        fclose(f);
        return -4;
    }

    for (i = 0; i <= KH1_ARD_ENTRY_COUNT; ++i) {
        if (read_u32le(f, &offsets[i]) < 0) {
            fclose(f);
            return -5;
        }
    }

    if (offsets[0] < KH1_ARD_HEADER_SIZE) {
        fclose(f);
        return -6;
    }

    for (i = 0; i < KH1_ARD_ENTRY_COUNT; ++i) {
        if (offsets[i] > offsets[i + 1u] || offsets[i] > file_size) {
            fclose(f);
            return -7;
        }
    }

    /* Some retail files declare the sector-aligned end rather than the exact
       on-disk length. Accept at most one PS2 sector of trailing pad. */
    if (offsets[KH1_ARD_ENTRY_COUNT] > file_size + KH1_ARD_MAX_TRAILING_PAD) {
        fclose(f);
        return -8;
    }

    start = offsets[KH1_ARD_RESOURCE_LIST_INDEX];
    end = offsets[KH1_ARD_RESOURCE_LIST_INDEX + 1u];
    if (start >= end || end > file_size) {
        fclose(f);
        return -9;
    }

    list_size = end - start;
    if ((list_size % KH1_ARD_RESOURCE_NAME_SIZE) != 0) {
        fclose(f);
        return -10;
    }
    slot_count = list_size / KH1_ARD_RESOURCE_NAME_SIZE;

    if (fseek(f, (long)start, SEEK_SET) != 0) {
        fclose(f);
        return -11;
    }

    for (i = 0; i < slot_count; ++i) {
        unsigned char raw[KH1_ARD_RESOURCE_NAME_SIZE];
        char name[KH1_ARD_RESOURCE_NAME_SIZE + 1u];
        uint32_t n = 0;

        if (fread(raw, 1, sizeof(raw), f) != sizeof(raw)) {
            fclose(f);
            return -12;
        }

        while (n < KH1_ARD_RESOURCE_NAME_SIZE && raw[n] != 0) {
            unsigned char c = raw[n];
            name[n] = (c >= 0x20 && c <= 0x7e) ? (char)c : '?';
            ++n;
        }
        name[n] = '\0';

        if (n != 0) {
            ++nonempty;
            if (visitor && visitor(i, name, user) != 0) break;
        }
    }

    if (info) {
        memset(info, 0, sizeof(*info));
        info->file_size = file_size;
        info->resource_slot_count = slot_count;
        info->resource_nonempty_count = nonempty;
    }

    fclose(f);
    return 0;
}
