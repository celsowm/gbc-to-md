#ifndef GBMD_BACKEND_H
#define GBMD_BACKEND_H

#ifdef SGDK_GCC
#include <genesis.h>
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GBMD_GB_VRAM_SIZE 0x2000u
#define GBMD_GB_OAM_SIZE  0x00A0u
#define GBMD_GB_IO_SIZE   0x0080u
#define GBMD_TILE_COUNT   384u
#define GBMD_BG_MAP_TILES 1024u
#define GBMD_MAX_SPRITES  40u

#define GBMD_BTN_UP    (1u << 0)
#define GBMD_BTN_DOWN  (1u << 1)
#define GBMD_BTN_LEFT  (1u << 2)
#define GBMD_BTN_RIGHT (1u << 3)
#define GBMD_BTN_A     (1u << 4)
#define GBMD_BTN_B     (1u << 5)
#define GBMD_BTN_START (1u << 6)
#define GBMD_BTN_SELECT (1u << 7)

typedef struct {
    void (*load_tile)(void *user, uint16_t md_tile_index, const uint8_t md_4bpp_tile[32]);
    void (*set_bg_tile)(void *user, uint16_t x, uint16_t y, uint16_t md_tile_index, uint8_t palette);
    void (*set_sprite)(void *user, uint8_t index, int16_t x, int16_t y,
                       uint16_t md_tile_index, uint8_t palette,
                       bool hflip, bool vflip, bool visible);
    void (*set_scroll)(void *user, int16_t x, int16_t y);
    /* Optional bulk path for dense 32-tile Game Boy map rows. */
    void (*set_bg_row)(void *user, uint16_t y, const uint8_t tiles[32], uint8_t palette);
} GBMDVideoOps;

typedef struct {
    uint8_t vram[GBMD_GB_VRAM_SIZE];
    uint8_t oam[GBMD_GB_OAM_SIZE];
    uint8_t io[GBMD_GB_IO_SIZE];
    uint8_t tile_dirty[(GBMD_TILE_COUNT + 7u) / 8u];
    uint8_t tile_seen[(GBMD_TILE_COUNT + 7u) / 8u];
    uint8_t map_dirty[(GBMD_BG_MAP_TILES + 7u) / 8u];
    uint8_t map_seen[(GBMD_BG_MAP_TILES + 7u) / 8u];
    uint8_t sprite_dirty[(GBMD_MAX_SPRITES + 7u) / 8u];
    uint8_t sprite_seen[(GBMD_MAX_SPRITES + 7u) / 8u];
    uint16_t md_buttons;
    uint8_t joyp_select;
    bool scroll_dirty;
    GBMDVideoOps video;
    void *video_user;
} GBMDBackend;

void gbmd_init(GBMDBackend *b, const GBMDVideoOps *ops, void *video_user);
void gbmd_set_buttons(GBMDBackend *b, uint16_t buttons);
uint8_t gbmd_read8(GBMDBackend *b, uint16_t addr);
void gbmd_write8(GBMDBackend *b, uint16_t addr, uint8_t value);
void gbmd_flush_video(GBMDBackend *b);
void gbmd_convert_tile_2bpp_to_4bpp(const uint8_t gb_tile[16], uint8_t md_tile[32], uint8_t color_base);
size_t gbmd_runtime_ram_bytes(void);

#endif
