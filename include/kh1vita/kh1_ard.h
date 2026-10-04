#pragma once

#include <stdint.h>

#define KH1_ARD_ENTRY_COUNT 32u
#define KH1_ARD_RESOURCE_LIST_INDEX 5u
#define KH1_ARD_RESOURCE_NAME_SIZE 0x20u

typedef struct Kh1ArdInfo {
    uint32_t file_size;
    uint32_t resource_slot_count;
    uint32_t resource_nonempty_count;
} Kh1ArdInfo;

typedef int (*Kh1ArdResourceVisitor)(uint32_t slot, const char *name, void *user);

/*
 * Parse the KH1 PS2 room archive header and visit the resource list stored in
 * entry 5. Returns 0 on success and a negative value on malformed or missing
 * data. This module deliberately uses only ISO C stdio so it can be tested on
 * a PC and compiled unchanged for Vita.
 */
int kh1_ard_scan(const char *path,
                 Kh1ArdInfo *info,
                 Kh1ArdResourceVisitor visitor,
                 void *user);
