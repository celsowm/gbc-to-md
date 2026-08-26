#include "mbc3test.h"
#include "gbrt.h"
#include "gbmd_backend.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    GBMDBackend md; GBContext ctx;
    gbmd_init(&md,NULL,NULL); gbrt_sgdk_context_init(&ctx,&md); mbc3test_init(&ctx);
    assert(ctx.mbc_type==0x10u && ctx.eram_size==0x8000u);
    gbrt_sgdk_run_frame(&ctx,mbc3test_run);
    assert(ctx.wram[0]==0x99u && ctx.fallback_hit==0u && ctx.halted);
    for (unsigned bank=1;bank<128;bank++) assert(ctx.wram[0x100u+(bank-1u)]==(uint8_t)bank);
    assert(ctx.wram[0x180]==1u);
    for (unsigned i=0;i<4;i++) assert(ctx.wram[0x190u+i]==(uint8_t)(0xB0u+i));
    assert(ctx.wram[0x1A0]==58u && ctx.wram[0x1A1]==59u && ctx.wram[0x1A2]==23u);
    assert(ctx.wram[0x1A3]==0xFEu && ctx.wram[0x1A4]==0x01u && ctx.wram[0x1A5]==0xFFu);
    /* Host-time RTC: two exact GB seconds roll 23:59:58 day 0x1FE -> 00:00:00 day 0x1FF. */
    gbrt_sgdk_advance_host_clock(&ctx,4194304u);
    assert(ctx.rtc.s==59u && ctx.rtc.m==59u && ctx.rtc.h==23u);
    gbrt_sgdk_advance_host_clock(&ctx,4194304u);
    assert(ctx.rtc.s==0u && ctx.rtc.m==0u && ctx.rtc.h==0u && ctx.rtc.dl==0xFFu && (ctx.rtc.dh&1u)==1u);
    /* Day 511 rollover sets carry and wraps the 9-bit day counter. */
    ctx.rtc.s=59u; ctx.rtc.m=59u; ctx.rtc.h=23u; ctx.rtc.dl=0xFFu; ctx.rtc.dh=0x01u; ctx.rtc.active=1u; ctx.rtc.cycle_remainder=0u;
    gbrt_sgdk_advance_host_clock(&ctx,4194304u);
    assert(ctx.rtc.s==0u && ctx.rtc.m==0u && ctx.rtc.h==0u && ctx.rtc.dl==0u && (ctx.rtc.dh&0x81u)==0x80u);
    /* Halt bit freezes RTC host-time advancement. */
    ctx.rtc.s=12u; ctx.rtc.active=0u; ctx.rtc.cycle_remainder=0u;
    gbrt_sgdk_advance_host_clock(&ctx,4194304u);
    assert(ctx.rtc.s==12u);
    printf("PASS: MBC3 127 ROM banks + 4 ERAM banks + RTC/latch; fallback=%u RAM=%zu\n",ctx.fallback_hit,gbrt_sgdk_context_ram_bytes()+sizeof(GBMDBackend));
    return 0;
}
