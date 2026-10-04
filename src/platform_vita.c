#include "kh1vita/platform.h"
#include "kh1vita/config.h"

#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/io/fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static void ensure_dir(const char *path) {
    int r = sceIoMkdir(path, 0777);
    (void)r; /* EEXIST est normal. */
}

int kh1vita_platform_mkdirs(void) {
    ensure_dir("ux0:data");
    ensure_dir(KH1VITA_DATA_ROOT);
    ensure_dir(KH1VITA_GAME_ROOT);
    ensure_dir(KH1VITA_KINGDOM_ROOT);
    return 0;
}

int kh1vita_platform_init(void) {
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    kh1vita_platform_mkdirs();
    kh1vita_log("=== KH1Vita starter boot ===\n");
    return 0;
}

void kh1vita_platform_shutdown(void) {
    kh1vita_log("Shutdown.\n");
}

void kh1vita_platform_delay_ms(unsigned ms) {
    sceKernelDelayThread(ms * 1000u);
}

uint32_t kh1vita_platform_buttons(void) {
    SceCtrlData pad;
    memset(&pad, 0, sizeof(pad));
    sceCtrlPeekBufferPositive(0, &pad, 1);
    return pad.buttons;
}

void kh1vita_log(const char *fmt, ...) {
    FILE *f = fopen(KH1VITA_LOG_PATH, "a");
    if (!f) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fclose(f);
}
