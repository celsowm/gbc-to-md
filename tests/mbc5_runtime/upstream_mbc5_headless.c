#include "mbc5test.h"
#include "gbrt.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
uint8_t g_joypad_buttons=0xFF, g_joypad_dpad=0xFF;
int main(void) {
    GBConfig cfg=*mbc5test_default_config(); cfg.enable_audio=false; cfg.enable_serial=false;
    GBContext *ctx=gb_context_create(&cfg); assert(ctx); mbc5test_init(ctx); mbc5test_run(ctx);
    assert(ctx->wram[0]==0x99u && ctx->halted);
    for (unsigned bank=1;bank<512;bank++) {
        unsigned off=0x100u+2u*(bank-1u);
        assert(ctx->wram[off]==(uint8_t)bank);
        assert(ctx->wram[off+1u]==(uint8_t)(bank>>8));
    }
    assert(ctx->wram[0x500]==0x42u);
    for (unsigned i=0;i<4;i++) assert(ctx->wram[0x510u+i]==(uint8_t)(0xD0u+i));
    assert(ctx->wram[0x514]==0xFFu);
    puts("PASS: upstream GBRT MBC5 reference matches all 511 switchable executable banks");
    gb_context_destroy(ctx); return 0;
}
