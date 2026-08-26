#include "cakegame.h"
#include "gbrt.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

uint8_t g_joypad_buttons=0xFF;
uint8_t g_joypad_dpad=0xFE; /* RIGHT */

int main(void) {
    GBConfig cfg=*cakegame_default_config(); cfg.enable_audio=false; cfg.enable_serial=false;
    GBContext *ctx=gb_context_create(&cfg); assert(ctx); cakegame_init(ctx);
    for (unsigned f=0; f<28; ++f) {
        uint32_t cycles=gb_run_frame(ctx);
        printf("f=%u cycles=%u x=%u frameirq=%u timerirq=%u tile=%u hit=%u map=%u halted=%u sp=%04x\n",
               f,cycles,ctx->wram[1],ctx->wram[7],ctx->wram[6],ctx->oam[2],ctx->wram[5],ctx->vram[0x190D],ctx->halted,ctx->sp);
        assert(ctx->sp==0xFFFE);
    }
    assert(ctx->wram[1]==104u && ctx->oam[1]==104u);
    assert(ctx->wram[5]==1u && ctx->vram[0x190D]==4u);
    assert(ctx->wram[7]>=26u && ctx->wram[7]<=28u);
    assert(ctx->wram[6]>=6u && ctx->wram[6]<=8u);
    assert(ctx->oam[6]==4u && ctx->oam[10]==4u);
    puts("PASS: upstream GBRT reaches the same IRQ-driven mini-game collision/map state");
    gb_context_destroy(ctx); return 0;
}
