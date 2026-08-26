#define _POSIX_C_SOURCE 200809L
#include "irqtest.h"
#include "gbrt.h"
#include "gbmd_backend.h"
#include <inttypes.h>
#include <stdio.h>
#include <time.h>

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

int main(void) {
    const unsigned frames = 100000u;
    GBMDBackend md;
    GBContext ctx;
    gbmd_init(&md, NULL, NULL);
    gbrt_sgdk_context_init(&ctx, &md);
    irqtest_init(&ctx);

    /* Boot to HALT. */
    gbrt_sgdk_run_frame(&ctx, irqtest_run);

    const uint64_t start_cycles = ctx.total_cycles;
    const uint32_t start_ticks = ctx.profile_tick_calls;
    const uint32_t start_reads = ctx.profile_read_calls;
    const uint32_t start_writes = ctx.profile_write_calls;
    const uint64_t t0 = now_ns();
    for (unsigned i = 0; i < frames; ++i) gbrt_sgdk_run_frame(&ctx, irqtest_run);
    const uint64_t t1 = now_ns();

    printf("irq-vblank frames=%u ns/frame=%.1f guest_cycles/frame=%.1f tick_calls/frame=%.1f reads/frame=%.1f writes/frame=%.1f counter=%u fallback=%u\n",
           frames,
           (double)(t1 - t0) / frames,
           (double)(ctx.total_cycles - start_cycles) / frames,
           (double)(ctx.profile_tick_calls - start_ticks) / frames,
           (double)(ctx.profile_read_calls - start_reads) / frames,
           (double)(ctx.profile_write_calls - start_writes) / frames,
           ctx.wram[0], ctx.fallback_hit);
    return ctx.fallback_hit ? 1 : 0;
}
