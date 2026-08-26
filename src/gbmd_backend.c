#include "gbmd_backend.h"

#include <string.h>

#define REG_JOYP 0x00u
#define REG_LCDC 0x40u
#define REG_SCY  0x42u
#define REG_SCX  0x43u
#define REG_LY   0x44u

static void bit_set(uint8_t *bits, unsigned index) {
    bits[index >> 3] |= (uint8_t)(1u << (index & 7u));
}

static bool bit_test(const uint8_t *bits, unsigned index) {
    return (bits[index >> 3] & (uint8_t)(1u << (index & 7u))) != 0;
}

static void bit_clear(uint8_t *bits, unsigned index) {
    bits[index >> 3] &= (uint8_t)~(1u << (index & 7u));
}

void gbmd_convert_tile_2bpp_to_4bpp(const uint8_t gb_tile[16], uint8_t md_tile[32], uint8_t color_base) {
    memset(md_tile, 0, 32);
    for (unsigned y = 0; y < 8; ++y) {
        const uint8_t lo = gb_tile[y * 2u + 0u];
        const uint8_t hi = gb_tile[y * 2u + 1u];
        for (unsigned x = 0; x < 8; ++x) {
            const unsigned bit = 7u - x;
            const uint8_t gb_color = (uint8_t)(((lo >> bit) & 1u) | (((hi >> bit) & 1u) << 1u));
            const uint8_t md_color = (uint8_t)((color_base + gb_color) & 0x0Fu);
            uint8_t *dst = &md_tile[y * 4u + (x >> 1u)];
            if ((x & 1u) == 0) *dst = (uint8_t)(md_color << 4u);
            else *dst |= md_color;
        }
    }
}

void gbmd_init(GBMDBackend *b, const GBMDVideoOps *ops, void *video_user) {
    memset(b, 0, sizeof(*b));
    if (ops) b->video = *ops;
    b->video_user = video_user;
    b->joyp_select = 0x30u;
    b->io[REG_JOYP] = 0xCFu;
    b->io[REG_LY] = 144u; /* POC: host publishes VBlank at flush boundary. */
}

void gbmd_set_buttons(GBMDBackend *b, uint16_t buttons) {
    b->md_buttons = buttons;
}

static uint8_t gbmd_read_joyp(const GBMDBackend *b) {
    uint8_t low = 0x0Fu;
    /* Game Boy inputs are active-low. P14 low selects directions. */
    if ((b->joyp_select & 0x10u) == 0) {
        if (b->md_buttons & GBMD_BTN_RIGHT) low &= (uint8_t)~0x01u;
        if (b->md_buttons & GBMD_BTN_LEFT)  low &= (uint8_t)~0x02u;
        if (b->md_buttons & GBMD_BTN_UP)    low &= (uint8_t)~0x04u;
        if (b->md_buttons & GBMD_BTN_DOWN)  low &= (uint8_t)~0x08u;
    }
    /* P15 low selects A/B/Select/Start. */
    if ((b->joyp_select & 0x20u) == 0) {
        if (b->md_buttons & GBMD_BTN_A)      low &= (uint8_t)~0x01u;
        if (b->md_buttons & GBMD_BTN_B)      low &= (uint8_t)~0x02u;
        if (b->md_buttons & GBMD_BTN_SELECT) low &= (uint8_t)~0x04u;
        if (b->md_buttons & GBMD_BTN_START)  low &= (uint8_t)~0x08u;
    }
    return (uint8_t)(0xC0u | (b->joyp_select & 0x30u) | low);
}

uint8_t gbmd_read8(GBMDBackend *b, uint16_t addr) {
    if (addr >= 0x8000u && addr <= 0x9FFFu) return b->vram[addr - 0x8000u];
    if (addr >= 0xFE00u && addr <= 0xFE9Fu) return b->oam[addr - 0xFE00u];
    if (addr >= 0xFF00u && addr <= 0xFF7Fu) {
        const uint8_t reg = (uint8_t)(addr - 0xFF00u);
        if (reg == REG_JOYP) return gbmd_read_joyp(b);
        return b->io[reg];
    }
    return 0xFFu;
}

void gbmd_write8(GBMDBackend *b, uint16_t addr, uint8_t value) {
    if (addr >= 0x8000u && addr <= 0x9FFFu) {
        const unsigned off = (unsigned)(addr - 0x8000u);
        if (off < 0x1800u) {
            const unsigned tile = off / 16u;
            const bool first_sync = !bit_test(b->tile_seen, tile);
            if (first_sync) bit_set(b->tile_seen, tile);
            if (!first_sync && b->vram[off] == value) return;
            b->vram[off] = value;
            bit_set(b->tile_dirty, tile);
        } else {
            const unsigned map_index = off - 0x1800u;
            if (map_index < GBMD_BG_MAP_TILES) {
                const bool first_sync = !bit_test(b->map_seen, map_index);
                if (first_sync) bit_set(b->map_seen, map_index);
                if (!first_sync && b->vram[off] == value) return;
                b->vram[off] = value;
                bit_set(b->map_dirty, map_index);
            } else {
                b->vram[off] = value;
            }
        }
        return;
    }
    if (addr >= 0xFE00u && addr <= 0xFE9Fu) {
        const unsigned off = (unsigned)(addr - 0xFE00u);
        const unsigned sprite = off / 4u;
        const bool first_sync = !bit_test(b->sprite_seen, sprite);
        if (first_sync) bit_set(b->sprite_seen, sprite);
        if (!first_sync && b->oam[off] == value) return;
        b->oam[off] = value;
        bit_set(b->sprite_dirty, sprite);
        return;
    }
    if (addr >= 0xFF00u && addr <= 0xFF7Fu) {
        const uint8_t reg = (uint8_t)(addr - 0xFF00u);
        if (reg == REG_JOYP) {
            b->joyp_select = (uint8_t)(value & 0x30u);
            b->io[reg] = (uint8_t)(0xC0u | b->joyp_select | 0x0Fu);
            return;
        }
        if (b->io[reg] == value) return;
        b->io[reg] = value;
        if (reg == REG_SCX || reg == REG_SCY) b->scroll_dirty = true;
    }
}

static void flush_tiles(GBMDBackend *b) {
    if (!b->video.load_tile) return;
    uint8_t converted[32];
    for (unsigned tile = 0; tile < GBMD_TILE_COUNT; ++tile) {
        if (!bit_test(b->tile_dirty, tile)) continue;
        gbmd_convert_tile_2bpp_to_4bpp(&b->vram[tile * 16u], converted, 0);
        b->video.load_tile(b->video_user, (uint16_t)tile, converted);
        bit_clear(b->tile_dirty, tile);
    }
}

static void flush_map(GBMDBackend *b) {
    if (!b->video.set_bg_tile && !b->video.set_bg_row) return;
    const unsigned map_base = 0x1800u; /* GB map 0x9800 */

    unsigned dirty_count = 0;
    for (unsigned index = 0; index < GBMD_BG_MAP_TILES; ++index)
        if (bit_test(b->map_dirty, index)) dirty_count++;

    /* Dense updates (initial clears/full room redraws) are emitted a row at a
       time. On SGDK this becomes at most 32 queued tilemap transfers instead
       of 1024 VDP_setTileMapXY calls. */
    if (b->video.set_bg_row && dirty_count >= 64u) {
        for (unsigned y = 0; y < 32u; ++y) {
            bool row_dirty = false;
            const unsigned row = y * 32u;
            for (unsigned x = 0; x < 32u; ++x) {
                if (bit_test(b->map_dirty, row + x)) { row_dirty = true; break; }
            }
            if (!row_dirty) continue;
            b->video.set_bg_row(b->video_user, (uint16_t)y, &b->vram[map_base + row], 0);
            for (unsigned x = 0; x < 32u; ++x) bit_clear(b->map_dirty, row + x);
        }
        return;
    }

    if (!b->video.set_bg_tile) return;
    for (unsigned index = 0; index < GBMD_BG_MAP_TILES; ++index) {
        if (!bit_test(b->map_dirty, index)) continue;
        const uint8_t tile = b->vram[map_base + index];
        b->video.set_bg_tile(b->video_user,
                             (uint16_t)(index & 31u),
                             (uint16_t)(index >> 5u),
                             tile, 0);
        bit_clear(b->map_dirty, index);
    }
}

static void flush_sprites(GBMDBackend *b) {
    if (!b->video.set_sprite) return;
    for (unsigned i = 0; i < GBMD_MAX_SPRITES; ++i) {
        if (!bit_test(b->sprite_dirty, i)) continue;
        const uint8_t *o = &b->oam[i * 4u];
        const int16_t y = (int16_t)o[0] - 16;
        const int16_t x = (int16_t)o[1] - 8;
        const uint8_t tile = o[2];
        const uint8_t flags = o[3];
        const bool visible = o[0] != 0 && o[1] != 0 && x > -8 && x < 320 && y > -16 && y < 224;
        b->video.set_sprite(b->video_user, (uint8_t)i, x, y, tile,
                            (uint8_t)((flags >> 4u) & 1u),
                            (flags & 0x20u) != 0,
                            (flags & 0x40u) != 0,
                            visible);
        bit_clear(b->sprite_dirty, i);
    }
}

void gbmd_flush_video(GBMDBackend *b) {
    flush_tiles(b);
    flush_map(b);
    flush_sprites(b);
    if (b->scroll_dirty && b->video.set_scroll) {
        b->video.set_scroll(b->video_user, (int16_t)b->io[REG_SCX], (int16_t)b->io[REG_SCY]);
        b->scroll_dirty = false;
    }
}

size_t gbmd_runtime_ram_bytes(void) {
    return sizeof(GBMDBackend);
}
