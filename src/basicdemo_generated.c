/*
 * Generated-style semantic fixture for the SGDK backend POC.
 * This is deliberately small and mirrors the writes a tiny GB homebrew makes.
 * It is NOT claimed to be output from upstream gbrecomp in this sandbox.
 */
#include "basicdemo_generated.h"

static uint8_t player_x = 88;
static uint8_t player_y = 88;

static const uint8_t tile_blank[16] = {
    0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0
};

/* 8x8 small cupcake-like sprite, four DMG colors. */
static const uint8_t tile_basicdemo[16] = {
    0x00,0x18,
    0x3C,0x24,
    0x7E,0x42,
    0x7E,0x5A,
    0x3C,0x24,
    0x3C,0x3C,
    0x18,0x18,
    0x00,0x00
};

static void write_tile(GBMDBackend *b, uint16_t tile_index, const uint8_t data[16]) {
    const uint16_t base = (uint16_t)(0x8000u + tile_index * 16u);
    for (unsigned i = 0; i < 16; ++i) gbmd_write8(b, (uint16_t)(base + i), data[i]);
}

void basicdemo_boot(GBMDBackend *b) {
    write_tile(b, 0, tile_blank);
    write_tile(b, 1, tile_basicdemo);

    /* Clear visible 20x18 section of the 32x32 GB BG map. */
    for (unsigned y = 0; y < 18; ++y) {
        for (unsigned x = 0; x < 20; ++x) {
            gbmd_write8(b, (uint16_t)(0x9800u + y * 32u + x), 0);
        }
    }

    /* First OAM entry: GB stores y+16 and x+8. */
    gbmd_write8(b, 0xFE00u, player_y);
    gbmd_write8(b, 0xFE01u, player_x);
    gbmd_write8(b, 0xFE02u, 1);
    gbmd_write8(b, 0xFE03u, 0);

    gbmd_write8(b, 0xFF42u, 0); /* SCY */
    gbmd_write8(b, 0xFF43u, 0); /* SCX */
    gbmd_write8(b, 0xFF40u, 0x93); /* LCD on, BG + OBJ */
    gbmd_flush_video(b);
}

void basicdemo_frame(GBMDBackend *b) {
    /* Select Game Boy direction group and consume active-low JOYP. */
    gbmd_write8(b, 0xFF00u, 0x20u);
    const uint8_t joy = gbmd_read8(b, 0xFF00u);
    if ((joy & 0x01u) == 0 && player_x < 168) ++player_x; /* right */
    if ((joy & 0x02u) == 0 && player_x > 8)   --player_x; /* left */
    if ((joy & 0x04u) == 0 && player_y > 16)  --player_y; /* up */
    if ((joy & 0x08u) == 0 && player_y < 160) ++player_y; /* down */

    gbmd_write8(b, 0xFE00u, player_y);
    gbmd_write8(b, 0xFE01u, player_x);
    gbmd_flush_video(b);
}
