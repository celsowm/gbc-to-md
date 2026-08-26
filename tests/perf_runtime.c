#define _POSIX_C_SOURCE 200809L
#include "basicdemo.h"
#include "gbrt.h"
#include "gbmd_backend.h"
#include <inttypes.h>
#include <stdio.h>
#include <time.h>

typedef struct {
    double ns_per_frame;
    double guest_cycles_per_frame;
    double tick_calls_per_frame;
    double reads_per_frame;
    double writes_per_frame;
    uint8_t final_oam_x;
    uint8_t fallback;
} BenchResult;

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static BenchResult bench(unsigned frames, int host_sync) {
    GBMDBackend md;
    GBContext ctx;
    gbmd_init(&md, NULL, NULL);
    gbrt_sgdk_context_init(&ctx, &md);
    basicdemo_init(&ctx);
    gbrt_sgdk_set_host_vblank_sync(&ctx, host_sync != 0);
    gbmd_set_buttons(&md, GBMD_BTN_RIGHT);

    /* One warm-up/boot frame, then reset counters. */
    gbrt_sgdk_run_frame(&ctx, basicdemo_run);
    const uint64_t start_cycles = ctx.total_cycles;
    const uint32_t start_ticks = ctx.profile_tick_calls;
    const uint32_t start_reads = ctx.profile_read_calls;
    const uint32_t start_writes = ctx.profile_write_calls;
    const uint64_t t0 = now_ns();
    for (unsigned i = 0; i < frames; ++i) {
        gbrt_sgdk_run_frame(&ctx, basicdemo_run);
    }
    const uint64_t t1 = now_ns();

    BenchResult r;
    r.ns_per_frame = (double)(t1 - t0) / frames;
    r.guest_cycles_per_frame = (double)(ctx.total_cycles - start_cycles) / frames;
    r.tick_calls_per_frame = (double)(ctx.profile_tick_calls - start_ticks) / frames;
    r.reads_per_frame = (double)(ctx.profile_read_calls - start_reads) / frames;
    r.writes_per_frame = (double)(ctx.profile_write_calls - start_writes) / frames;
    r.final_oam_x = md.oam[1];
    r.fallback = ctx.fallback_hit;
    return r;
}

static void print_result(const char *name, unsigned frames, BenchResult r) {
    printf("%-12s frames=%u ns/frame=%.1f guest_cycles/frame=%.1f tick_calls/frame=%.1f reads/frame=%.1f writes/frame=%.1f oamx=%u fallback=%u\n",
           name, frames, r.ns_per_frame, r.guest_cycles_per_frame,
           r.tick_calls_per_frame, r.reads_per_frame, r.writes_per_frame,
           r.final_oam_x, r.fallback);
}

int main(void) {
    BenchResult cycle = bench(300, 0);
    BenchResult host = bench(100000, 1);
    print_result("cycle-ppu", 300, cycle);
    print_result("host-vblank", 100000, host);
    printf("speedup=%.1fx guest-cycle-collapse=%.1fx tick-call-collapse=%.1fx\n",
           cycle.ns_per_frame / host.ns_per_frame,
           cycle.guest_cycles_per_frame / host.guest_cycles_per_frame,
           cycle.tick_calls_per_frame / host.tick_calls_per_frame);
    return (cycle.fallback || host.fallback) ? 1 : 0;
}
