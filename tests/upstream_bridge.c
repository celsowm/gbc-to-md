#include "basicdemo.h"
#include "gbrt.h"
#include "gbmd_backend.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

uint8_t g_joypad_buttons = 0xFF;
uint8_t g_joypad_dpad = 0xFF;

typedef struct {
    unsigned tile_uploads;
    unsigned sprite_updates;
    int16_t player_x;
    int16_t player_y;
    uint16_t player_tile;
} FakeVDP;

static void load_tile(void *u, uint16_t tile, const uint8_t data[32]) {
    FakeVDP *v=u; (void)tile; (void)data; v->tile_uploads++;
}
static void set_bg(void *u, uint16_t x, uint16_t y, uint16_t tile, uint8_t pal) {
    (void)u; (void)x; (void)y; (void)tile; (void)pal;
}
static void set_sprite(void *u, uint8_t index, int16_t x, int16_t y,
                       uint16_t tile, uint8_t pal, bool hf, bool vf, bool visible) {
    FakeVDP *v=u; (void)pal; (void)hf; (void)vf;
    if (index == 0 && visible) { v->player_x=x; v->player_y=y; v->player_tile=tile; }
    v->sprite_updates++;
}
static void set_scroll(void *u, int16_t x, int16_t y) { (void)u; (void)x; (void)y; }

static void sync_gbrt_to_gbmd(const GBContext *ctx, GBMDBackend *md) {
    for (unsigned i=0; i<0x2000; ++i) {
        if (md->vram[i] != ctx->vram[i]) gbmd_write8(md, (uint16_t)(0x8000u+i), ctx->vram[i]);
    }
    for (unsigned i=0; i<0xA0; ++i) {
        if (md->oam[i] != ctx->oam[i]) gbmd_write8(md, (uint16_t)(0xFE00u+i), ctx->oam[i]);
    }
    if (md->io[0x42] != ctx->io[0x42]) gbmd_write8(md, 0xFF42, ctx->io[0x42]);
    if (md->io[0x43] != ctx->io[0x43]) gbmd_write8(md, 0xFF43, ctx->io[0x43]);
    gbmd_flush_video(md);
}

int main(void) {
    GBConfig cfg=*basicdemo_default_config(); cfg.enable_audio=false; cfg.enable_serial=false;
    GBContext *ctx=gb_context_create(&cfg); assert(ctx); basicdemo_init(ctx);

    FakeVDP v={0};
    const GBMDVideoOps ops={load_tile,set_bg,set_sprite,set_scroll,NULL};
    GBMDBackend md; gbmd_init(&md,&ops,&v);

    g_joypad_dpad=0xFE;
    for (unsigned frame=0; frame<22; ++frame) {
        gb_run_frame(ctx);
        sync_gbrt_to_gbmd(ctx,&md);
        printf("frame=%u gbrt_x=%u md_player_x=%d tile_uploads=%u\n",
               frame, ctx->oam[1], v.player_x, v.tile_uploads);
    }
    assert(v.tile_uploads >= 1);
    assert(v.player_tile == 1);
    assert(v.player_x == (int16_t)ctx->oam[1]-8);
    assert(ctx->oam[1] == 104);
    assert(v.player_x == 96);
    assert(ctx->wram[5] == 1);
    puts("PASS: upstream GBRT -> framebuffer-free bridge reaches the same collision state");
    gb_context_destroy(ctx);
    return 0;
}
