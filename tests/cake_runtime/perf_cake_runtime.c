#define _POSIX_C_SOURCE 200809L
#include "cakegame.h"
#include "gbrt.h"
#include "gbmd_backend.h"
#include <inttypes.h>
#include <stdio.h>
#include <time.h>

static uint64_t now_ns(void){ struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts); return (uint64_t)ts.tv_sec*1000000000ull+(uint64_t)ts.tv_nsec; }
int main(void){
    const unsigned frames=100000u;
    GBMDBackend md; GBContext ctx; gbmd_init(&md,NULL,NULL); gbrt_sgdk_context_init(&ctx,&md); cakegame_init(&ctx);
    gbmd_set_buttons(&md,GBMD_BTN_RIGHT);
    for(unsigned i=0;i<28;i++) gbrt_sgdk_run_frame(&ctx,cakegame_run); /* settle at collision */
    uint64_t c0=ctx.total_cycles; uint32_t t0c=ctx.profile_tick_calls,r0=ctx.profile_read_calls,w0=ctx.profile_write_calls; uint64_t t0=now_ns();
    for(unsigned i=0;i<frames;i++) gbrt_sgdk_run_frame(&ctx,cakegame_run);
    uint64_t t1=now_ns();
    printf("cake-irq frames=%u ns/frame=%.1f guest_cycles/frame=%.1f tick_calls/frame=%.1f reads/frame=%.1f writes/frame=%.1f x=%u frameirq=%u timerirq=%u fallback=%u\n",
      frames,(double)(t1-t0)/frames,(double)(ctx.total_cycles-c0)/frames,(double)(ctx.profile_tick_calls-t0c)/frames,
      (double)(ctx.profile_read_calls-r0)/frames,(double)(ctx.profile_write_calls-w0)/frames,ctx.wram[1],ctx.wram[7],ctx.wram[6],ctx.fallback_hit);
    return ctx.fallback_hit?1:0;
}
