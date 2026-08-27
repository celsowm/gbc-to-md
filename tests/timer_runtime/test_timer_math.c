#include "gbrt.h"
#include "gbmd_backend.h"
#include <assert.h>
#include <stdio.h>

static void reset_timer(GBContext *ctx, GBMDBackend *md, uint8_t tac) {
    ctx->div_counter=0; md->io[0x04]=0; md->io[0x05]=0; md->io[0x06]=0; md->io[0x07]=tac; md->io[0x0F]=0;
}
int main(void){
    GBMDBackend md; GBContext ctx; gbmd_init(&md,NULL,NULL); gbrt_sgdk_context_init(&ctx,&md);
    struct { uint8_t tac; uint8_t tima; } cases[]={{0x04,68},{0x05,37},{0x06,73},{0x07,18}};
    for(unsigned i=0;i<4;i++){
        reset_timer(&ctx,&md,cases[i].tac); gbrt_sgdk_advance_host_clock(&ctx,70224u);
        printf("TAC=%02x TIMA=%u IF=%02x DIV=%u\n",cases[i].tac,md.io[0x05],md.io[0x0F],gb_read8(&ctx,0xFF04));
        assert(md.io[0x05]==cases[i].tima);
        if(i>0) assert(md.io[0x0F]&0x04u);
    }
    reset_timer(&ctx,&md,0x04); md.io[0x05]=254; md.io[0x06]=250;
    gbrt_sgdk_advance_host_clock(&ctx,4096u);
    assert(md.io[0x05]==252u && (md.io[0x0F]&0x04u));
    gb_write8(&ctx,0xFF04u,0x99u); assert(ctx.div_counter==0u && gb_read8(&ctx,0xFF04u)==0u);

    /* Host-VBlank LY must expose all ten VBlank scanlines. Real cartridges
       wait for exact values such as LY=148 rather than merely LY>=144. */
    ctx.host_vblank_sync=1u; ctx.host_ly_reads=0u; ctx.stopped=0u;
    for(unsigned i=0;i<10u;i++) assert(gb_read8(&ctx,0xFF44u)==(uint8_t)(144u+i));
    assert(!ctx.stopped);
    assert(gb_read8(&ctx,0xFF44u)==0u && ctx.stopped);

    /* Generated busy loops must yield even when they never read LY. */
    ctx.stopped=0u; ctx.host_guest_cycle_budget=32u;
    gb_tick(&ctx,12u); assert(!ctx.stopped && ctx.host_guest_cycle_budget==20u);
    gb_tick(&ctx,20u); assert(ctx.stopped && ctx.host_guest_cycle_budget==0u);

    /* The canonical copied-to-HRAM DMA routine must perform OAM DMA and RET. */
    for(unsigned i=0;i<160u;i++) ctx.wram[i]=(uint8_t)(i^0x5Au);
    const uint8_t dma_stub[10]={0x3E,0xC0,0xE0,0x46,0x3E,0x28,0x3D,0x20,0xFD,0xC9};
    for(unsigned i=0;i<10u;i++) ctx.hram[0x36u+i]=dma_stub[i];
    ctx.sp=0xC100u; ctx.wram[0x100u]=0x34u; ctx.wram[0x101u]=0x12u;
    ctx.stopped=0u; ctx.host_guest_cycle_budget=1000u;
    assert(gbrt_try_execute_hram_stub(&ctx,0xFFB6u));
    assert(ctx.pc==0x1234u && ctx.sp==0xC102u);
    for(unsigned i=0;i<160u;i++) assert(md.oam[i]==(uint8_t)(i^0x5Au));

    puts("PASS: timer math, commercial VBlank LY, host budget and HRAM OAM DMA"); return 0;
}
