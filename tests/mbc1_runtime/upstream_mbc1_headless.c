#include "mbc1test.h"
#include "gbrt.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
uint8_t g_joypad_buttons=0xFF, g_joypad_dpad=0xFF;
int main(void) {
    GBConfig cfg=*mbc1test_default_config(); cfg.enable_audio=false; cfg.enable_serial=false;
    GBContext *ctx=gb_context_create(&cfg); assert(ctx); mbc1test_init(ctx);
    /* No LCD/frame timing needed: dispatch until the test HALTs. */
    mbc1test_run(ctx);
    printf("done=%02x b1=%02x b2=%02x b3=%02x eram=%02x,%02x,%02x,%02x disabled=%02x bank2op=%02x rombank=%u rambank=%u mode=%u halted=%u\n",
           ctx->wram[0],ctx->wram[0x10],ctx->wram[0x11],ctx->wram[0x12],
           ctx->wram[0x20],ctx->wram[0x21],ctx->wram[0x22],ctx->wram[0x23],ctx->wram[0x24],ctx->wram[0x25],
           ctx->rom_bank,ctx->ram_bank,ctx->mbc_mode,ctx->halted);
    assert(ctx->wram[0]==0x99u);
    assert(ctx->wram[0x10]==0x11u && ctx->wram[0x11]==0x22u && ctx->wram[0x12]==0x33u);
    assert(ctx->wram[0x20]==0xA0u && ctx->wram[0x21]==0xA1u && ctx->wram[0x22]==0xA2u && ctx->wram[0x23]==0xA3u);
    assert(ctx->wram[0x24]==0xFFu && ctx->wram[0x25]==0x3Eu);
    puts("PASS: upstream GBRT MBC1 reference state matches expected banking semantics");
    gb_context_destroy(ctx); return 0;
}
