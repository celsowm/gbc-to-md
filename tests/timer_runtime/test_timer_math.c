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
    puts("PASS: all TAC clock selectors, overflow/reload math and DIV reset"); return 0;
}
