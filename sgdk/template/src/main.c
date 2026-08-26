#include <genesis.h>
#include "gbmd_backend.h"
#include "gbrt.h"
#include "cakegame.h"

static GBMDBackend backend;
static GBContext gbctx;

static void sgdk_load_tile(void *user, uint16_t tile, const uint8_t data[32]) {
    (void)user;
    VDP_loadTileData((const u32 *)data, TILE_USER_INDEX + tile, 1, DMA_QUEUE_COPY);
}

static void sgdk_set_bg(void *user, uint16_t x, uint16_t y, uint16_t tile, uint8_t pal) {
    (void)user;
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(pal, 0, 0, 0, TILE_USER_INDEX + tile), x, y);
}

static void sgdk_set_sprite(void *user, uint8_t index, int16_t x, int16_t y,
                            uint16_t tile, uint8_t pal, bool hf, bool vf, bool visible) {
    (void)user;
    const u8 link = (index + 1u < GBMD_MAX_SPRITES) ? (u8)(index + 1u) : 0u;
    if (!visible) {
        VDP_setSpriteFull(index, -32, -32, SPRITE_SIZE(1,1), 0, link);
        return;
    }
    VDP_setSpriteFull(index, x, y, SPRITE_SIZE(1,1),
        TILE_ATTR_FULL(pal, 1, vf, hf, TILE_USER_INDEX + tile), link);
}

static void sgdk_set_scroll(void *user, int16_t x, int16_t y) {
    (void)user;
    VDP_setHorizontalScroll(BG_A, (s16)-x);
    VDP_setVerticalScroll(BG_A, (s16)y);
}


static void sgdk_set_bg_row(void *user, uint16_t y, const uint8_t tiles[32], uint8_t pal) {
    (void)user;
    u16 row[32];
    for (u16 x = 0; x < 32u; ++x) row[x] = tiles[x];
    VDP_setTileMapDataRowEx(BG_A, row,
        TILE_ATTR_FULL(pal, 0, 0, 0, TILE_USER_INDEX), y, 0, 32, DMA_QUEUE_COPY);
}

static uint16_t read_md_buttons(void) {
    const u16 p = JOY_readJoypad(JOY_1);
    uint16_t out = 0;
    if (p & BUTTON_UP) out |= GBMD_BTN_UP;
    if (p & BUTTON_DOWN) out |= GBMD_BTN_DOWN;
    if (p & BUTTON_LEFT) out |= GBMD_BTN_LEFT;
    if (p & BUTTON_RIGHT) out |= GBMD_BTN_RIGHT;
    if (p & BUTTON_A) out |= GBMD_BTN_A;
    if (p & BUTTON_B) out |= GBMD_BTN_B;
    if (p & BUTTON_START) out |= GBMD_BTN_START;
    if (p & BUTTON_C) out |= GBMD_BTN_SELECT;
    return out;
}

int main(bool hard) {
    (void)hard;
    const GBMDVideoOps ops = {
        .load_tile = sgdk_load_tile,
        .set_bg_tile = sgdk_set_bg,
        .set_sprite = sgdk_set_sprite,
        .set_scroll = sgdk_set_scroll,
        .set_bg_row = sgdk_set_bg_row
    };
    const u16 greys[4] = {
        RGB24_TO_VDPCOLOR(0xF8F8F8), RGB24_TO_VDPCOLOR(0xA8A8A8),
        RGB24_TO_VDPCOLOR(0x585858), RGB24_TO_VDPCOLOR(0x101010)
    };

    PAL_setColors(0, greys, 4, CPU);
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearSprites();
    /* Keep the hardware sprite list linked even when only sparse OAM entries
       are dirty. Hidden entries remain offscreen but preserve reachability. */
    for (u16 i = 0; i < GBMD_MAX_SPRITES; ++i) {
        const u8 link = (i + 1u < GBMD_MAX_SPRITES) ? (u8)(i + 1u) : 0u;
        VDP_setSpriteFull(i, -32, -32, SPRITE_SIZE(1,1), 0, link);
    }

    gbmd_init(&backend, &ops, NULL);
    gbrt_sgdk_context_init(&gbctx, &backend);
    cakegame_init(&gbctx);

    while (TRUE) {
        gbmd_set_buttons(&backend, read_md_buttons());
        gbrt_sgdk_run_frame(&gbctx, cakegame_run);
        VDP_updateSprites(GBMD_MAX_SPRITES, DMA_QUEUE);
        SYS_doVBlankProcess();
    }
    return 0;
}
