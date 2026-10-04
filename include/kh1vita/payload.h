#pragma once

typedef struct Kh1PayloadStatus {
    int elf_final_mix;
    int elf_original_jp;
    int kingdom_dir;
    int kingdom_nonempty;
    int room_di08;
    int sora_mdls;
    int sora_mset;
    int sora_test_ready;
} Kh1PayloadStatus;

Kh1PayloadStatus kh1vita_payload_scan(void);
int kh1vita_payload_ready(const Kh1PayloadStatus *status);
