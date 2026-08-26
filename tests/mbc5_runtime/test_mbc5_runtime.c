#include "mbc5test.h"
#include "gbrt.h"
#include "gbmd_backend.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    GBMDBackend md; GBContext ctx;
    gbmd_init(&md,NULL,NULL); gbrt_sgdk_context_init(&ctx,&md); mbc5test_init(&ctx);
    assert(ctx.mbc_type==0x1Bu && ctx.eram_size==0x8000u);
    gbrt_sgdk_run_frame(&ctx,mbc5test_run);
    assert(ctx.wram[0]==0x99u && ctx.fallback_hit==0u && ctx.halted);
    for (unsigned bank=1;bank<512;bank++) {
        unsigned off=0x100u+2u*(bank-1u);
        assert(ctx.wram[off]==(uint8_t)bank);
        assert(ctx.wram[off+1u]==(uint8_t)(bank>>8));
    }
    assert(ctx.wram[0x500]==0x42u);
    for (unsigned i=0;i<4;i++) assert(ctx.wram[0x510u+i]==(uint8_t)(0xD0u+i));
    assert(ctx.wram[0x514]==0xFFu);
#ifdef GBRT_SGDK_FAR_ROM_HOST_TEST
    /* Native dispatch does not need raw opcode bytes. Exercise the physical
       Mega Drive mapper explicitly by reading one byte from every 512 KiB
       page of the 8 MiB guest ROM (32 GB banks per mapper page). */
    ctx.far_rom_bank_valid=0u;
    ctx.far_rom_switches=0u;
    for (unsigned bank=0u; bank<512u; bank+=32u) {
        gb_write8(&ctx,0x2000u,(uint8_t)bank);
        gb_write8(&ctx,0x3000u,(uint8_t)(bank>>8));
        const size_t off=(size_t)bank * 0x4000u;
        assert(gb_read8(&ctx,0x4000u)==ctx.rom[off]);
    }
    assert(ctx.far_rom_switches==10u); /* pages 0..5 are below 0x300000 */
    printf("PASS: MBC5 far-ROM 8 MiB data sweep; mapper_switches=%u fallback=%u RAM=%zu\n",
           (unsigned)ctx.far_rom_switches,ctx.fallback_hit,gbrt_sgdk_context_ram_bytes()+sizeof(GBMDBackend));
#else
    printf("PASS: MBC5 511 executable banks incl 9th bit + bank0 + ERAM; fallback=%u RAM=%zu\n",ctx.fallback_hit,gbrt_sgdk_context_ram_bytes()+sizeof(GBMDBackend));
#endif
    return 0;
}
