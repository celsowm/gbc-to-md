#include "timertest.h"
#include "gbrt.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

uint8_t g_joypad_buttons = 0xFF;
uint8_t g_joypad_dpad = 0xFF;

int main(void) {
    GBConfig cfg = *timertest_default_config();
    cfg.enable_audio = false;
    cfg.enable_serial = false;
    GBContext *ctx = gb_context_create(&cfg);
    assert(ctx);
    timertest_init(ctx);
    for (unsigned frame = 0; frame <= 16; ++frame) {
        uint32_t cycles = gb_run_frame(ctx);
        printf("frame=%u cycles=%u timer=%u vblank=%u TIMA=%u DIV=%u pc=%04x sp=%04x halted=%u\n",
               frame, cycles, ctx->wram[0], ctx->wram[1], ctx->io[0x05], ctx->io[0x04],
               ctx->pc, ctx->sp, ctx->halted);
        assert(ctx->sp == 0xFFFE);
    }
    printf("FINAL timer=%u vblank=%u TIMA=%u DIV=%u\n", ctx->wram[0],ctx->wram[1],ctx->io[0x05],ctx->io[0x04]);
    assert(ctx->wram[1] >= 15u);
    assert(ctx->wram[0] >= 3u && ctx->wram[0] <= 5u);
    puts("PASS: upstream GBRT Timer/VBlank IRQ behavior matches the same low-frequency timer regime");
    gb_context_destroy(ctx);
    return 0;
}
