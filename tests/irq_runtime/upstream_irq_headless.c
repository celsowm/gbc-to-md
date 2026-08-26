#include "irqtest.h"
#include "gbrt.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

uint8_t g_joypad_buttons = 0xFF;
uint8_t g_joypad_dpad = 0xFF;

int main(void) {
    GBConfig cfg = *irqtest_default_config();
    cfg.enable_audio = false;
    cfg.enable_serial = false;
    GBContext *ctx = gb_context_create(&cfg);
    assert(ctx);
    irqtest_init(ctx);

    for (unsigned frame = 0; frame < 8; ++frame) {
        uint32_t cycles = gb_run_frame(ctx);
        printf("frame=%u cycles=%u counter=%u pc=%04x sp=%04x ime=%u halted=%u IF=%02x IE=%02x\n",
               frame, cycles, ctx->wram[0], ctx->pc, ctx->sp, ctx->ime,
               ctx->halted, ctx->io[0x0F], ctx->io[0x80]);
        if (frame >= 1) assert(ctx->wram[0] >= frame - 1);
        assert(ctx->sp == 0xFFFE);
    }

    assert(ctx->wram[0] > 0);
    puts("PASS: upstream GBRT services generated VBlank ISR and returns through RETI");
    gb_context_destroy(ctx);
    return 0;
}
