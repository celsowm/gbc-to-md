#include "mbc3test.h"
#include "gbrt.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
uint8_t g_joypad_buttons=0xFF, g_joypad_dpad=0xFF;
int main(void) {
    GBConfig cfg=*mbc3test_default_config(); cfg.enable_audio=false; cfg.enable_serial=false; cfg.ignore_rtc_persistence=true;
    GBContext *ctx=gb_context_create(&cfg); assert(ctx); mbc3test_init(ctx); mbc3test_run(ctx);
    assert(ctx->wram[0]==0x99u && ctx->halted);
    for (unsigned bank=1;bank<128;bank++) assert(ctx->wram[0x100u+(bank-1u)]==(uint8_t)bank);
    assert(ctx->wram[0x180]==1u);
    for (unsigned i=0;i<4;i++) assert(ctx->wram[0x190u+i]==(uint8_t)(0xB0u+i));
    assert(ctx->wram[0x1A0]==58u && ctx->wram[0x1A1]==59u && ctx->wram[0x1A2]==23u && ctx->wram[0x1A3]==0xFEu && ctx->wram[0x1A4]==1u && ctx->wram[0x1A5]==0xFFu);
    puts("PASS: upstream GBRT MBC3 reference matches all 127 banks + ERAM + RTC latch");
    gb_context_destroy(ctx); return 0;
}
