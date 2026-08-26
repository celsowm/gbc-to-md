#include "basicdemo.h"
#include "gbrt.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

uint8_t g_joypad_buttons = 0xFF;
uint8_t g_joypad_dpad = 0xFF;

int main(void) {
    GBConfig cfg = *basicdemo_default_config();
    cfg.enable_audio = false;
    cfg.enable_serial = false;
    GBContext *ctx = gb_context_create(&cfg);
    assert(ctx);
    basicdemo_init(ctx);

    g_joypad_dpad = 0xFE;
    unsigned ran = 0;
    for (unsigned i = 0; i < 22; ++i) {
        uint32_t cycles = gb_run_frame(ctx);
        ++ran;
        if (i < 4 || i >= 16) {
            printf("frame=%u cycles=%u pc=%04X xvar=%u oam_x=%u hit=%u map=%u ly=%u\n",
                   i, cycles, ctx->pc, ctx->wram[1], ctx->oam[1], ctx->wram[5],
                   ctx->vram[0x190D], ctx->io[0x44]);
        }
    }

    assert(ctx->wram[1] == 104);
    assert(ctx->oam[1] == 104);
    assert(ctx->oam[6] == 3);
    assert(ctx->oam[10] == 3);
    assert(ctx->wram[5] == 1);
    assert(ctx->vram[0x190D] == 3);
    printf("PASS: upstream GBRT reaches collision at X=%u and mutates BG after %u frames\n",
           ctx->wram[1], ran);
    gb_context_destroy(ctx);
    return 0;
}
