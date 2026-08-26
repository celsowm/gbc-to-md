#define _GNU_SOURCE
#include "mbc5test.h"
#include "gbrt.h"
#include "gbmd_backend.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>

extern const uint8_t rom_data[];
extern const size_t rom_size;

int main(void) {
    const uintptr_t logical_base=0x00110000u; /* deliberately not 512 KiB aligned */
    void *mapped=mmap((void *)logical_base,rom_size,PROT_READ|PROT_WRITE,
                      MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
    assert(mapped != MAP_FAILED && (uintptr_t)mapped == logical_base);
    memcpy(mapped,rom_data,rom_size);
    assert(mprotect(mapped,rom_size,PROT_READ)==0);

    GBMDBackend md; GBContext ctx;
    gbmd_init(&md,NULL,NULL);
    gbrt_sgdk_context_init(&ctx,&md);
    gb_context_load_rom(&ctx,(const uint8_t *)mapped,rom_size);
    gb_context_reset(&ctx,true);
    assert(ctx.mbc_type==0x1Bu && ctx.eram_size==0x8000u);

    gbrt_sgdk_run_frame(&ctx,mbc5test_run);
    assert(ctx.wram[0]==0x99u && ctx.fallback_hit==0u && ctx.halted);

    ctx.far_rom_bank_valid=0u;
    ctx.far_rom_switches=0u;
    for (unsigned bank=0u; bank<512u; bank+=32u) {
        gb_write8(&ctx,0x2000u,(uint8_t)bank);
        gb_write8(&ctx,0x3000u,(uint8_t)(bank>>8));
        const size_t off=(size_t)bank*0x4000u;
        assert(gb_read8(&ctx,0x4000u)==rom_data[off]);
        /* Repeated/same-page reads must not cause a physical mapper write. */
        const uint32_t before=ctx.far_rom_switches;
        assert(gb_read8(&ctx,0x4001u)==rom_data[off+1u]);
        assert(ctx.far_rom_switches==before);
    }
    /* Base 0x110000 means the 16 sampled 512 KiB guest pages occupy physical
       mapper pages 2..17; logical addresses below 0x300000 are direct, so
       pages 6..17 require exactly 12 region-7 selections. */
    assert(ctx.far_rom_switches==12u);
    assert(munmap(mapped,rom_size)==0);
    printf("PASS: target far-ROM branch on low logical mmap; mapper_switches=%u base=0x%lX fallback=%u\n",
           (unsigned)ctx.far_rom_switches,(unsigned long)logical_base,ctx.fallback_hit);
    return 0;
}
