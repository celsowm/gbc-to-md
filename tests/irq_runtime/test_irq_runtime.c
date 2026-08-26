#include "irqtest.h"
#include "gbrt.h"
#include "gbmd_backend.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    GBMDBackend md;
    GBContext ctx;
    gbmd_init(&md, NULL, NULL);
    gbrt_sgdk_context_init(&ctx, &md);
    irqtest_init(&ctx);

    for (unsigned frame = 0; frame < 8; ++frame) {
        gbrt_sgdk_run_frame(&ctx, irqtest_run);
        printf("frame=%u counter=%u pc=%04x sp=%04x ime=%u halted=%u IF=%02x IE=%02x fallback=%u\n",
               frame, ctx.wram[0], ctx.pc, ctx.sp, ctx.ime, ctx.halted,
               md.io[0x0F], ctx.ie, ctx.fallback_hit);
        assert(!ctx.fallback_hit);
        assert(ctx.halted);
        assert(ctx.sp == 0xFFFE);
        assert(ctx.ie == 0x01);
        if (frame == 0) assert(ctx.wram[0] == 0);
        else assert(ctx.wram[0] == frame);
    }

    const uint8_t before_lcd_off = ctx.wram[0];
    gbmd_write8(&md, 0xFF40u, 0x00u); /* LCD off: VBlank IRQ must stop. */
    gbrt_sgdk_run_frame(&ctx, irqtest_run);
    assert(ctx.wram[0] == before_lcd_off);
    assert(ctx.halted);

    gbmd_write8(&md, 0xFF40u, 0x91u); /* LCD on again: IRQ resumes. */
    gbrt_sgdk_run_frame(&ctx, irqtest_run);
    assert(ctx.wram[0] == (uint8_t)(before_lcd_off + 1u));

    puts("PASS: host VBlank IRQ + HALT/RETI works; LCD off suppresses IRQ and LCD on resumes it");
    return 0;
}
