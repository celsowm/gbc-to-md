#include "cakegame.h"
#include "gbrt.h"
#include "gbmd_backend.h"
#include <assert.h>
#include <stdio.h>

typedef struct {
    int16_t x[3], y[3], scroll_x, scroll_y;
    uint16_t tile[3];
    bool visible[3];
    unsigned tile_uploads, sprite_updates, map_rows, sparse_bg_updates, scroll_updates;
    unsigned player_tile_changes;
    uint16_t last_player_tile;
} FakeVDP;

static void load_tile(void *u, uint16_t t, const uint8_t d[32]) { (void)t; (void)d; ((FakeVDP*)u)->tile_uploads++; }
static void set_bg(void *u, uint16_t x, uint16_t y, uint16_t t, uint8_t p) { (void)x;(void)y;(void)t;(void)p; ((FakeVDP*)u)->sparse_bg_updates++; }
static void set_sprite(void *u, uint8_t i, int16_t x, int16_t y, uint16_t t, uint8_t p, bool hf, bool vf, bool vis) {
    (void)p;(void)hf;(void)vf; FakeVDP *v=u;
    if (i<3) { v->x[i]=x; v->y[i]=y; v->tile[i]=t; v->visible[i]=vis; }
    if (i==0 && v->last_player_tile != t) { if (v->last_player_tile) v->player_tile_changes++; v->last_player_tile=t; }
    v->sprite_updates++;
}
static void set_scroll(void *u, int16_t x, int16_t y) { FakeVDP *v=u; v->scroll_x=x; v->scroll_y=y; v->scroll_updates++; }
static void set_bg_row(void *u, uint16_t y, const uint8_t tiles[32], uint8_t p) { (void)y;(void)tiles;(void)p; ((FakeVDP*)u)->map_rows++; }

int main(void) {
    FakeVDP v={0};
    GBMDVideoOps ops={load_tile,set_bg,set_sprite,set_scroll,set_bg_row};
    GBMDBackend md; GBContext ctx;
    gbmd_init(&md,&ops,&v); gbrt_sgdk_context_init(&ctx,&md); cakegame_init(&ctx);

    for (unsigned f=0; f<28; ++f) {
        const uint8_t before_x=ctx.wram[1];
        const uint8_t before_frame=ctx.wram[7];
        gbmd_set_buttons(&md,GBMD_BTN_RIGHT);
        gbrt_sgdk_run_frame(&ctx,cakegame_run);
        printf("f=%u x=%u frameirq=%u timerirq=%u animtile=%u hit=%u map=%u IF=%02x halted=%u fallback=%u\n",
               f,ctx.wram[1],ctx.wram[7],ctx.wram[6],md.oam[2],ctx.wram[5],md.vram[0x190D],md.io[0x0F],ctx.halted,ctx.fallback_hit);
        assert(!ctx.fallback_hit); assert(ctx.halted); assert(ctx.sp==0xFFFE);
        if (f>0) assert((uint8_t)(ctx.wram[7]-before_frame)==1u); /* exactly one gameplay VBlank */
        if (before_x<104 && f>0) assert(ctx.wram[1] <= (uint8_t)(before_x+1u)); /* Timer wake never double-steps. */
    }

    assert(ctx.wram[7]==27u);
    assert(ctx.wram[6]==7u);
    assert(ctx.wram[1]==104u && md.oam[1]==104u);
    assert(ctx.wram[5]==1u && md.vram[0x190D]==4u);
    assert(v.tile_uploads>=5u && v.map_rows==32u && v.sparse_bg_updates==1u);
    assert(v.visible[0] && v.visible[1] && v.visible[2]);
    assert(v.x[0]==96 && v.x[1]==104 && v.x[2]==144);
    assert(v.player_tile_changes>=4u); /* timer-driven cake animation really changed OAM tile. */
    assert(v.scroll_x==16);
    puts("PASS: IRQ-driven mini-game uses VBlank for gameplay, Timer for animation, collision/map mutation, and never double-steps on Timer wake");
    return 0;
}
