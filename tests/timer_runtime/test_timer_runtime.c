#include "timertest.h"
#include "gbrt.h"
#include "gbmd_backend.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    GBMDBackend md;
    GBContext ctx;
    gbmd_init(&md, NULL, NULL);
    gbrt_sgdk_context_init(&ctx, &md);
    timertest_init(&ctx);

    for (unsigned frame = 0; frame <= 16; ++frame) {
        gbrt_sgdk_run_frame(&ctx, timertest_run);
        printf("frame=%u timer=%u vblank=%u TIMA=%u DIV=%u pc=%04x sp=%04x ime=%u halted=%u IF=%02x fallback=%u\n",
               frame, ctx.wram[0], ctx.wram[1], md.io[0x05], gb_read8(&ctx,0xFF04),
               ctx.pc, ctx.sp, ctx.ime, ctx.halted, md.io[0x0F], ctx.fallback_hit);
        assert(!ctx.fallback_hit);
        assert(ctx.halted);
        assert(ctx.sp == 0xFFFE);
    }
    assert(ctx.wram[1] == 16u);
    assert(ctx.wram[0] == 4u);
    assert(md.io[0x05] == 141u);
    assert(gb_read8(&ctx, 0xFF04u) == 55u);
    puts("PASS: host-clock DIV/TIMA drives Timer IRQ while HALTed and coexists with VBlank IRQ");
    return 0;
}
