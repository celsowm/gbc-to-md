#include "mbc1test.h"
#include "gbrt.h"
#include "gbmd_backend.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    GBMDBackend md; GBContext ctx;
    gbmd_init(&md,NULL,NULL);
    gbrt_sgdk_context_init(&ctx,&md);
    mbc1test_init(&ctx);
    assert(ctx.mbc_type==0x03u);
    assert(ctx.eram_size==0x8000u);
    gbrt_sgdk_run_frame(&ctx,mbc1test_run);
    printf("done=%02x b1=%02x b2=%02x b3=%02x eram=%02x,%02x,%02x,%02x disabled=%02x bank2op=%02x rombank=%u rambank=%u mode=%u fallback=%u RAM=%zu\n",
           ctx.wram[0],ctx.wram[0x10],ctx.wram[0x11],ctx.wram[0x12],
           ctx.wram[0x20],ctx.wram[0x21],ctx.wram[0x22],ctx.wram[0x23],ctx.wram[0x24],ctx.wram[0x25],
           ctx.rom_bank,ctx.ram_bank,ctx.mbc_mode,ctx.fallback_hit,
           gbrt_sgdk_context_ram_bytes()+sizeof(GBMDBackend));
    assert(ctx.wram[0]==0x99u);
    assert(ctx.wram[0x10]==0x11u && ctx.wram[0x11]==0x22u && ctx.wram[0x12]==0x33u);
    assert(ctx.wram[0x20]==0xA0u && ctx.wram[0x21]==0xA1u && ctx.wram[0x22]==0xA2u && ctx.wram[0x23]==0xA3u);
    assert(ctx.wram[0x24]==0xFFu);
    assert(ctx.wram[0x25]==0x3Eu);
    assert(ctx.fallback_hit==0u);
    assert(ctx.halted);
    puts("PASS: MBC1 banked code + 4 ERAM banks execute on minimal SGDK runtime");
    return 0;
}
