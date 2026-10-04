#include "kh1vita/payload.h"
#include "kh1vita/config.h"
#include "kh1vita/platform.h"

#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <string.h>

static int exists_file(const char *path) {
    SceIoStat st;
    memset(&st, 0, sizeof(st));
    return sceIoGetstat(path, &st) >= 0 && SCE_S_ISREG(st.st_mode);
}

static int exists_dir(const char *path) {
    SceIoStat st;
    memset(&st, 0, sizeof(st));
    return sceIoGetstat(path, &st) >= 0 && SCE_S_ISDIR(st.st_mode);
}

static int dir_nonempty(const char *path) {
    SceUID d = sceIoDopen(path);
    if (d < 0) return 0;
    SceIoDirent ent;
    int found = 0;
    memset(&ent, 0, sizeof(ent));
    while (sceIoDread(d, &ent) > 0) {
        if (strcmp(ent.d_name, ".") && strcmp(ent.d_name, "..") && strcmp(ent.d_name, ".gitkeep")) {
            found = 1;
            break;
        }
        memset(&ent, 0, sizeof(ent));
    }
    sceIoDclose(d);
    return found;
}

Kh1PayloadStatus kh1vita_payload_scan(void) {
    Kh1PayloadStatus s;
    memset(&s, 0, sizeof(s));

    s.elf_final_mix = exists_file(KH1VITA_GAME_ROOT "/SLPS_251.98");
    s.elf_original_jp = exists_file(KH1VITA_GAME_ROOT "/SLPS_251.05");
    s.kingdom_dir = exists_dir(KH1VITA_KINGDOM_ROOT);
    s.kingdom_nonempty = s.kingdom_dir && dir_nonempty(KH1VITA_KINGDOM_ROOT);
    s.room_di08 = exists_file(KH1VITA_KINGDOM_ROOT "/di08.ard");
    s.sora_mdls = exists_file(KH1VITA_KINGDOM_ROOT "/xa_ex_0010.mdls");
    s.sora_mset = exists_file(KH1VITA_KINGDOM_ROOT "/xa_ex_0010.mset");
    s.sora_test_ready = s.room_di08 && s.sora_mdls && s.sora_mset;

    kh1vita_log("Payload scan: fm=%d jp=%d kingdom_dir=%d kingdom_nonempty=%d di08=%d sora_mdls=%d sora_mset=%d ready=%d\n",
                s.elf_final_mix, s.elf_original_jp, s.kingdom_dir, s.kingdom_nonempty,
                s.room_di08, s.sora_mdls, s.sora_mset, s.sora_test_ready);
    return s;
}

int kh1vita_payload_ready(const Kh1PayloadStatus *s) {
    if (!s) return 0;
    return s->kingdom_nonempty && s->sora_test_ready;
}
