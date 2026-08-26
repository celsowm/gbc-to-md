#include "basicdemo.h"
#include "gbrt.h"
#include "gbmd_backend.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  int16_t x[3], y[3], scroll_x, scroll_y;
  uint16_t tile[3];
  bool visible[3];
  unsigned tile_uploads, sprite_updates, map_rows, sparse_bg_updates, scroll_updates;
  uint16_t last_bg_x, last_bg_y, last_bg_tile;
} FakeVDP;

static void load_tile(void *u, uint16_t t, const uint8_t d[32]) {
  (void)t; (void)d; ((FakeVDP *)u)->tile_uploads++;
}
static void set_bg(void *u, uint16_t x, uint16_t y, uint16_t t, uint8_t p) {
  (void)p;
  FakeVDP *v = u;
  v->sparse_bg_updates++;
  v->last_bg_x = x;
  v->last_bg_y = y;
  v->last_bg_tile = t;
}
static void set_sprite(void *u, uint8_t i, int16_t x, int16_t y, uint16_t t,
                       uint8_t p, bool hf, bool vf, bool vis) {
  (void)p; (void)hf; (void)vf;
  FakeVDP *v = u;
  if (i < 3) {
    v->x[i] = x;
    v->y[i] = y;
    v->tile[i] = t;
    v->visible[i] = vis;
  }
  v->sprite_updates++;
}
static void set_scroll(void *u, int16_t x, int16_t y) {
  FakeVDP *v = u;
  v->scroll_x = x;
  v->scroll_y = y;
  v->scroll_updates++;
}
static void set_bg_row(void *u, uint16_t y, const uint8_t tiles[32], uint8_t p) {
  FakeVDP *v = u;
  (void)y; (void)tiles; (void)p;
  v->map_rows++;
}

int main(void) {
  FakeVDP v = {0};
  GBMDVideoOps ops = {load_tile, set_bg, set_sprite, set_scroll, set_bg_row};
  GBMDBackend md;
  GBContext ctx;

  gbmd_init(&md, &ops, &v);
  gbrt_sgdk_context_init(&ctx, &md);
  basicdemo_init(&ctx);

  printf("RAM minctx=%zu backend=%zu total=%zu\n", sizeof(ctx), sizeof(md), sizeof(ctx) + sizeof(md));

  for (unsigned f = 0; f < 28; f++) {
    gbmd_set_buttons(&md, GBMD_BTN_RIGHT);
    gbrt_sgdk_run_frame(&ctx, basicdemo_run);
    printf("f=%u pc=%04x ly=%u oamx=%u mdx=%d enemy=%d third=%d map_sparse=%u scroll=%d fallback=%u\n",
           f, ctx.pc, ctx.ly, md.oam[1], v.x[0], v.x[1], v.x[2],
           v.sparse_bg_updates, v.scroll_x, ctx.fallback_hit);
    assert(!ctx.fallback_hit);
  }

  assert(v.tile_uploads >= 4);
  assert(v.map_rows == 32);
  assert(md.oam[2] == 1);
  assert(md.oam[6] == 3);
  assert(md.oam[10] == 3);
  assert(v.visible[0] && v.visible[1] && v.visible[2]);
  assert(v.x[0] == 96);   /* OAM X 104 -> screen X 96. */
  assert(v.x[1] == 104);  /* collision object. */
  assert(v.x[2] == 144);  /* third object. */
  assert(md.oam[1] == 104); /* movement stops at collision boundary. */
  assert(ctx.wram[0x0005] == 1); /* HITVAR $C005. */
  assert(md.vram[0x190D] == 3); /* $990D collision marker. */
  assert(v.sparse_bg_updates == 1); /* dirty-on-change avoids repeated DMA. */
  assert(v.last_bg_x == 13 && v.last_bg_y == 8 && v.last_bg_tile == 3);
  assert(v.scroll_updates > 0);
  assert(v.scroll_x == 16); /* camera stopped with player. */

  puts("PASS: upstream C -> minimal GBRT-SGDK handles 3 sprites, collision, scroll and sparse runtime map mutation");
  return 0;
}
