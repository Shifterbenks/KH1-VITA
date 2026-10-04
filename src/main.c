#include "kh1vita/platform.h"
#include "kh1vita/renderer.h"
#include "kh1vita/payload.h"
#include "kh1vita/game_bridge.h"

#include <psp2/ctrl.h>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    int fatal = 0;
    if (kh1vita_platform_init() < 0) fatal = 1;
    if (kh1vita_renderer_init() < 0) fatal = 1;

    Kh1PayloadStatus status = kh1vita_payload_scan();
    kh1vita_renderer_status(status.sora_test_ready,
                            status.kingdom_nonempty, fatal);

    if (!fatal && kh1vita_payload_ready(&status)) {
        kh1vita_game_bridge_boot();
    }

    unsigned old_buttons = 0;
    for (;;) {
        unsigned buttons = kh1vita_platform_buttons();
        unsigned pressed = buttons & ~old_buttons;
        old_buttons = buttons;

        if (pressed & SCE_CTRL_START) break;

        if (pressed & SCE_CTRL_CROSS) {
            status = kh1vita_payload_scan();
            kh1vita_renderer_status(status.sora_test_ready,
                                    status.kingdom_nonempty, fatal);
        }

        if (!fatal && kh1vita_payload_ready(&status)) {
            kh1vita_game_bridge_tick();
        }

        kh1vita_platform_delay_ms(16);
    }

    kh1vita_game_bridge_shutdown();
    kh1vita_renderer_shutdown();
    kh1vita_platform_shutdown();
    return 0;
}
