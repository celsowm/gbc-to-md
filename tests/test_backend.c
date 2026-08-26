#include "gbmd_backend.h"
#include "basicdemo_generated.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    unsigned tile_uploads;
    unsigned map_writes;
    unsigned map_rows;
    unsigned sprite_writes;
    uint8_t tile1[32];
    int16_t sprite_x;
    int16_t sprite_y;
    bool sprite_visible;
} FakeVDP;

static void fake_load_tile(void *user, uint16_t tile, const uint8_t data[32]) {
    FakeVDP *v = user;
    ++v->tile_uploads;
    if (tile == 1) memcpy(v->tile1, data, 32);
}
static void fake_set_bg(void *user, uint16_t x, uint16_t y, uint16_t tile, uint8_t pal) {
    FakeVDP *v = user; (void)x; (void)y; (void)tile; (void)pal; ++v->map_writes;
}
static void fake_set_sprite(void *user, uint8_t index, int16_t x, int16_t y,
                            uint16_t tile, uint8_t pal, bool hf, bool vf, bool visible) {
    FakeVDP *v = user; (void)index; (void)tile; (void)pal; (void)hf; (void)vf;
    ++v->sprite_writes; v->sprite_x = x; v->sprite_y = y; v->sprite_visible = visible;
}
static void fake_scroll(void *user, int16_t x, int16_t y) { (void)user; (void)x; (void)y; }
static void fake_set_bg_row(void *user, uint16_t y, const uint8_t tiles[32], uint8_t pal) {
    FakeVDP *v = user; (void)y; (void)tiles; (void)pal; ++v->map_rows;
}

static void test_tile_converter(void) {
    const uint8_t gb[16] = {
        0x80,0x00,
        0x00,0x80,
        0x80,0x80,
        0,0, 0,0, 0,0, 0,0, 0,0
    };
    uint8_t md[32];
    gbmd_convert_tile_2bpp_to_4bpp(gb, md, 0);
    assert(md[0] == 0x10);
    assert(md[4] == 0x20);
    assert(md[8] == 0x30);
}

static void test_joypad_mapping(void) {
    GBMDBackend b; gbmd_init(&b, NULL, NULL);
    gbmd_write8(&b, 0xFF00, 0x20);
    gbmd_set_buttons(&b, GBMD_BTN_RIGHT | GBMD_BTN_UP);
    const uint8_t v = gbmd_read8(&b, 0xFF00);
    assert((v & 0x01) == 0);
    assert((v & 0x02) != 0);
    assert((v & 0x04) == 0);
    assert((v & 0x08) != 0);
}

static void test_basicdemo_moves(void) {
    FakeVDP v = {0};
    const GBMDVideoOps ops = {fake_load_tile, fake_set_bg, fake_set_sprite, fake_scroll, NULL};
    GBMDBackend b; gbmd_init(&b, &ops, &v);
    basicdemo_boot(&b);
    assert(v.tile_uploads >= 2);
    assert(v.map_writes == 20u * 18u);
    assert(v.sprite_visible);
    const int16_t x0 = v.sprite_x;
    const int16_t y0 = v.sprite_y;

    gbmd_set_buttons(&b, GBMD_BTN_RIGHT);
    for (int i = 0; i < 5; ++i) basicdemo_frame(&b);
    assert(v.sprite_x == x0 + 5);
    assert(v.sprite_y == y0);

    gbmd_set_buttons(&b, GBMD_BTN_UP);
    for (int i = 0; i < 3; ++i) basicdemo_frame(&b);
    assert(v.sprite_x == x0 + 5);
    assert(v.sprite_y == y0 - 3);
}

static void test_first_zero_tile_sync_and_idempotence(void) {
    FakeVDP v = {0};
    const GBMDVideoOps ops = {fake_load_tile, fake_set_bg, fake_set_sprite, fake_scroll, fake_set_bg_row};
    GBMDBackend b; gbmd_init(&b, &ops, &v);

    for (unsigned i = 0; i < 16u; ++i) gbmd_write8(&b, (uint16_t)(0x8000u + i), 0);
    gbmd_flush_video(&b);
    assert(v.tile_uploads == 1u);

    for (unsigned i = 0; i < 16u; ++i) gbmd_write8(&b, (uint16_t)(0x8000u + i), 0);
    gbmd_flush_video(&b);
    assert(v.tile_uploads == 1u);
}

static void test_dense_map_batching(void) {
    FakeVDP v = {0};
    GBMDVideoOps ops = {fake_load_tile, fake_set_bg, fake_set_sprite, fake_scroll, fake_set_bg_row};
    GBMDBackend b; gbmd_init(&b, &ops, &v);
    for (unsigned i = 0; i < 1024u; ++i) gbmd_write8(&b, (uint16_t)(0x9800u + i), (uint8_t)(i & 3u));
    gbmd_flush_video(&b);
    assert(v.map_rows == 32u);
    assert(v.map_writes == 0u);
}

int main(void) {
    test_tile_converter();
    test_joypad_mapping();
    test_dense_map_batching();
    test_first_zero_tile_sync_and_idempotence();
    test_basicdemo_moves();
    printf("PASS: tile 2bpp->4bpp, first-sync/idempotence, JOYP, OAM and movement\n");
    printf("GBMDBackend RAM footprint: %zu bytes\n", gbmd_runtime_ram_bytes());
    return 0;
}
