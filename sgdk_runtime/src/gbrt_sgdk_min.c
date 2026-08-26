#include "gbrt.h"
#include <string.h>
#ifdef GBRT_SGDK_USE_FAR_ROM
#ifndef GBRT_SGDK_FAR_ROM_HOST_TEST
#include <mapper.h>
#endif
#endif
#ifdef GBRT_SGDK_USE_CART_SRAM
#include <sram.h>
#ifndef RAM_SECT
#define RAM_SECT __attribute__((section(".ramprog")))
#endif
#endif

bool gbrt_trace_enabled = false;
bool gbrt_benchmark_fast_tick_enabled = false;


uint8_t gbrt_sgdk_rom_read8(GBContext *ctx, size_t off) {
    if (!ctx || !ctx->rom || off >= ctx->rom_size) return 0xFFu;
#ifndef GBRT_SGDK_USE_FAR_ROM
    return ctx->rom[off];
#else
#ifdef GBRT_SGDK_FAR_ROM_HOST_TEST
    /* Deterministic host model: pretend rom_data starts at address 0.
       It exercises the same 512 KiB bank/cache decisions while still reading
       from normal process memory. */
    const uintptr_t logical=(uintptr_t)off;
    if (logical < 0x300000u) return ctx->rom[off];
    const uint8_t bank=(uint8_t)(logical >> 19);
    if (!ctx->far_rom_bank_valid || ctx->far_rom_bank != bank) {
        ctx->far_rom_bank=bank;
        ctx->far_rom_bank_valid=1u;
        ctx->far_rom_switches++;
    }
    return ctx->rom[off];
#else
    /* SGDK official SEGA mapper. SYS_getFarDataEx(..., true) maps the
       logical physical-ROM page through region 7 (0x380000-0x3FFFFF) and
       reuses SGDK's own bank cache. This remains correct even if another SGDK
       FAR access changed region 7 between guest ROM reads. Region 4 is never
       touched, so cart SRAM at 0x200000 can coexist with far ROM. */
    const uintptr_t logical=(uintptr_t)ctx->rom + (uintptr_t)off;
    if (logical < 0x300000u) return *(const volatile uint8_t *)logical;
    if (logical >= 0x02000000u) return 0xFFu; /* official mapper: 64 x 512 KiB */
    const uint8_t bank=(uint8_t)(logical >> 19);
    if (SYS_getBank(7u) != bank) ctx->far_rom_switches++;
    const uint8_t *mapped=(const uint8_t *)SYS_getFarDataEx((void *)logical, true);
    ctx->far_rom_bank=bank;
    ctx->far_rom_bank_valid=1u;
    return *mapped;
#endif
#endif
}

static void set_zn(GBContext *ctx, uint8_t value) { ctx->f_z = (value == 0); }


static size_t gbrt_sgdk_cart_eram_size(const GBContext *ctx) {
    if (!ctx || !ctx->rom || ctx->rom_size <= 0x149u) return 0u;
    /* This backend intentionally caps direct ERAM at 32 KiB: it maps exactly
       onto the standard SGDK odd-byte SRAM window. Larger MBC5 RAM cartridges
       will need a Mega Drive-side SRAM mapper or another persistence backend. */
    switch (gbrt_sgdk_rom_read8((GBContext *)ctx, 0x149u)) {
        case 0x01u: return 2u * 1024u;
        case 0x02u: return 8u * 1024u;
        case 0x03u: return 32u * 1024u;
        default: return 0u;
    }
}

static bool gbrt_sgdk_is_mbc1(uint8_t type) { return type >= 0x01u && type <= 0x03u; }
static bool gbrt_sgdk_is_mbc3(uint8_t type) { return type >= 0x0Fu && type <= 0x13u; }
static bool gbrt_sgdk_is_mbc5(uint8_t type) { return type >= 0x19u && type <= 0x1Eu; }
static bool gbrt_sgdk_cart_has_rtc(uint8_t type) { return type == 0x0Fu || type == 0x10u; }

static bool gbrt_sgdk_detect_mbc1_multicart(const GBContext *ctx) {
    static const uint8_t logo[48] = {
        0xCE,0xED,0x66,0x66,0xCC,0x0D,0x00,0x0B,0x03,0x73,0x00,0x83,0x00,0x0C,0x00,0x0D,
        0x00,0x08,0x11,0x1F,0x88,0x89,0x00,0x0E,0xDC,0xCC,0x6E,0xE6,0xDD,0xDD,0xD9,0x99,
        0xBB,0xBB,0x67,0x63,0x6E,0x0E,0xEC,0xCC,0xDD,0xDC,0x99,0x9F,0xBB,0xB9,0x33,0x3E
    };
    const size_t off = (0x10u * 0x4000u) + 0x0104u;
    if (!ctx || !ctx->rom || ctx->rom_size < off + sizeof(logo)) return false;
    for (size_t i=0; i<sizeof(logo); ++i) {
        if (gbrt_sgdk_rom_read8((GBContext *)ctx, off+i) != logo[i]) return false;
    }
    return true;
}

static uint16_t gbrt_sgdk_wrap_rom_bank(const GBContext *ctx, uint32_t bank) {
    if (!ctx || ctx->rom_size < 0x4000u) return 0u;
    const uint32_t count=(uint32_t)(ctx->rom_size/0x4000u);
    return count ? (uint16_t)(bank % count) : 0u;
}


#ifdef GBRT_SGDK_USE_CART_SRAM
static RAM_SECT uint8_t gbrt_sgdk_eram_read(const GBContext *ctx, size_t off) {
#else
static uint8_t gbrt_sgdk_eram_read(const GBContext *ctx, size_t off) {
#endif
    if (!ctx || off >= ctx->eram_size) return 0xFFu;
#ifdef GBRT_SGDK_USE_CART_SRAM
    SRAM_enable();
    const uint8_t value=SRAM_readByte((uint32_t)off);
    SRAM_disable();
    return value;
#else
    return ctx->eram[off];
#endif
}

#ifdef GBRT_SGDK_USE_CART_SRAM
static RAM_SECT void gbrt_sgdk_eram_write(GBContext *ctx, size_t off, uint8_t value) {
#else
static void gbrt_sgdk_eram_write(GBContext *ctx, size_t off, uint8_t value) {
#endif
    if (!ctx || off >= ctx->eram_size) return;
#ifdef GBRT_SGDK_USE_CART_SRAM
    SRAM_enable();
    SRAM_writeByte((uint32_t)off,value);
    SRAM_disable();
#else
    ctx->eram[off]=value;
#endif
}

static void gbrt_sgdk_reset_mbc(GBContext *ctx) {
    ctx->ram_enabled=0u;
    ctx->mbc_mode=0u;
    ctx->rom_bank_low=1u;
    ctx->rom_bank_upper=0u;
    ctx->rom_bank=1u;
    ctx->ram_bank=0u;
    if (ctx->rom && ctx->rom_size>0x149u) {
        ctx->mbc_type=gbrt_sgdk_rom_read8(ctx,0x147u);
        ctx->eram_size=gbrt_sgdk_cart_eram_size(ctx);
        ctx->mbc1_multicart=(gbrt_sgdk_is_mbc1(ctx->mbc_type) &&
                             gbrt_sgdk_detect_mbc1_multicart(ctx)) ? 1u : 0u;
    } else {
        ctx->mbc_type=0u; ctx->eram_size=0u; ctx->mbc1_multicart=0u;
    }
    ctx->rtc_mode=0u;
    ctx->rtc_reg=0u;
    ctx->rtc.active=1u;
    ctx->rtc.cycle_remainder=0u;
}

void gbrt_sgdk_context_init(GBContext *ctx, GBMDBackend *md) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->md = md;
    ctx->io = md ? md->io : NULL;
    ctx->wram_bank = 1;
    ctx->ly = md ? md->io[0x44] : 0;
    ctx->host_vblank_sync = 1;
}

size_t gbrt_sgdk_context_ram_bytes(void) { return sizeof(GBContext); }

void gb_context_load_rom(GBContext *ctx, const uint8_t *rom, size_t rom_size) {
    ctx->rom = rom;
    ctx->rom_size = rom_size;
}

void gb_context_reset(GBContext *ctx, bool hard_reset) {
    (void)hard_reset;
    const uint8_t *rom = ctx->rom;
    const size_t rom_size = ctx->rom_size;
    GBMDBackend *md = ctx->md;
    uint8_t *io = ctx->io;
    memset(ctx, 0, sizeof(*ctx));
    ctx->rom = rom;
    ctx->rom_size = rom_size;
    ctx->md = md;
    ctx->io = io;
    ctx->pc = 0x0100;
    ctx->sp = 0xFFFE;
    ctx->wram_bank = 1;
    ctx->ly = md ? md->io[0x44] : 0;
    ctx->host_vblank_sync = 1;
    gbrt_sgdk_reset_mbc(ctx);
    if (md) md->io[0x44] = ctx->ly;
}

uint16_t gb_resolve_rom_bank(const GBContext *ctx, uint16_t addr) {
    if (!ctx || addr >= 0x8000u) return 0u;
    if (gbrt_sgdk_is_mbc1(ctx->mbc_type)) {
        const uint8_t raw_low=(uint8_t)(ctx->rom_bank_low & 0x1Fu);
        const uint8_t shift=ctx->mbc1_multicart ? 4u : 5u;
        const uint16_t high=(uint16_t)(ctx->rom_bank_upper & 0x03u) << shift;
        if (addr < 0x4000u) return gbrt_sgdk_wrap_rom_bank(ctx, ctx->mbc_mode ? high : 0u);
        uint16_t low=ctx->mbc1_multicart ? (uint16_t)(raw_low & 0x0Fu) : raw_low;
        if (raw_low == 0u) low=1u;
        return gbrt_sgdk_wrap_rom_bank(ctx, (uint16_t)(high | low));
    }
    if (addr < 0x4000u) return 0u;
    if (gbrt_sgdk_is_mbc3(ctx->mbc_type)) {
        uint16_t bank=(uint16_t)(ctx->rom_bank_low & 0x7Fu);
        return gbrt_sgdk_wrap_rom_bank(ctx, bank == 0u ? 1u : bank);
    }
    if (gbrt_sgdk_is_mbc5(ctx->mbc_type)) {
        return gbrt_sgdk_wrap_rom_bank(ctx,
            ((uint16_t)(ctx->rom_bank_upper & 0x01u) << 8) | ctx->rom_bank_low);
    }
    return gbrt_sgdk_wrap_rom_bank(ctx, 1u);
}

uint8_t gb_read8(GBContext *ctx, uint16_t addr) {
#ifdef GBRT_SGDK_PROFILE
    ctx->profile_read_calls++;
#endif
    if (addr < 0x8000u) {
        if (!ctx->rom) return 0xFFu;
        const uint16_t bank=gb_resolve_rom_bank(ctx, addr);
        const size_t off=((size_t)bank * 0x4000u) + (size_t)(addr & 0x3FFFu);
        return gbrt_sgdk_rom_read8(ctx,off);
    }
    if (addr < 0xA000u) return gbmd_read8(ctx->md, addr);
    if (addr < 0xC000u) {
        if (!ctx->ram_enabled) return 0xFFu;
        if (gbrt_sgdk_is_mbc3(ctx->mbc_type) && ctx->rtc_mode) {
            switch (ctx->rtc_reg) {
                case 0x08u: return ctx->rtc.latched_s;
                case 0x09u: return ctx->rtc.latched_m;
                case 0x0Au: return ctx->rtc.latched_h;
                case 0x0Bu: return ctx->rtc.latched_dl;
                case 0x0Cu: return ctx->rtc.latched_dh;
                default: return 0xFFu;
            }
        }
        if (ctx->eram_size == 0u) return 0xFFu;
        const size_t off=((size_t)ctx->ram_bank * 0x2000u) + (size_t)(addr - 0xA000u);
        return gbrt_sgdk_eram_read(ctx,off);
    }
    if (addr >= 0xC000u && addr < 0xD000u) return ctx->wram[addr - 0xC000u];
    if (addr >= 0xD000u && addr < 0xE000u) return ctx->wram[0x1000u + (addr - 0xD000u)];
    if (addr >= 0xE000u && addr < 0xFE00u) return gb_read8(ctx, (uint16_t)(addr - 0x2000u));
    if (addr >= 0xFE00u && addr <= 0xFE9Fu) return gbmd_read8(ctx->md, addr);
    if (addr >= 0xFF00u && addr <= 0xFF7Fu) {
        if (addr == 0xFF04u) return (uint8_t)(ctx->div_counter >> 8);
        if (addr == 0xFF07u) return (uint8_t)(0xF8u | (ctx->md ? (ctx->md->io[0x07u] & 0x07u) : 0u));
        if (addr == 0xFF44u) {
            if (ctx->host_vblank_sync) {
                /* First LY read in a host frame observes VBlank. The next one
                   observes active display and yields to the Mega Drive VBlank.
                   This collapses GB busy-wait scanline loops to O(1). */
                if (ctx->host_ly_reads++ == 0u) {
                    ctx->ly = 144u;
                } else {
                    ctx->ly = 0u;
                    ctx->stopped = 1u;
                }
                if (ctx->md) ctx->md->io[0x44] = ctx->ly;
            }
            return ctx->ly;
        }
        return gbmd_read8(ctx->md, addr);
    }
    if (addr >= 0xFF80u && addr <= 0xFFFEu) return ctx->hram[addr - 0xFF80u];
    if (addr == 0xFFFFu) return ctx->ie;
    return 0xFFu;
}

void gb_write8(GBContext *ctx, uint16_t addr, uint8_t value) {
#ifdef GBRT_SGDK_PROFILE
    ctx->profile_write_calls++;
#endif
    if (addr < 0x8000u) {
        if (gbrt_sgdk_is_mbc1(ctx->mbc_type)) {
            if (addr < 0x2000u) ctx->ram_enabled=((value & 0x0Fu) == 0x0Au) ? 1u : 0u;
            else if (addr < 0x4000u) ctx->rom_bank_low=(uint8_t)(value & 0x1Fu);
            else if (addr < 0x6000u) {
                ctx->rom_bank_upper=(uint8_t)(value & 0x03u);
                if (ctx->mbc_mode) ctx->ram_bank=ctx->rom_bank_upper;
            } else {
                ctx->mbc_mode=(uint8_t)(value & 0x01u);
                ctx->ram_bank=ctx->mbc_mode ? ctx->rom_bank_upper : 0u;
            }
            ctx->rom_bank=gb_resolve_rom_bank(ctx,0x4000u);
        } else if (gbrt_sgdk_is_mbc3(ctx->mbc_type)) {
            if (addr < 0x2000u) {
                ctx->ram_enabled=((value & 0x0Fu) == 0x0Au) ? 1u : 0u;
            } else if (addr < 0x4000u) {
                ctx->rom_bank_low=(uint8_t)(value & 0x7Fu);
                ctx->rom_bank=gb_resolve_rom_bank(ctx,0x4000u);
            } else if (addr < 0x6000u) {
                if (value <= 0x03u) { ctx->rtc_mode=0u; ctx->ram_bank=value; }
                else if (value >= 0x08u && value <= 0x0Cu) { ctx->rtc_mode=1u; ctx->rtc_reg=value; }
            } else {
                if (ctx->rtc.latch_state == 0u && value == 0u) ctx->rtc.latch_state=1u;
                else if (ctx->rtc.latch_state == 1u && value == 1u) {
                    ctx->rtc.latch_state=0u;
                    ctx->rtc.latched_s=ctx->rtc.s; ctx->rtc.latched_m=ctx->rtc.m;
                    ctx->rtc.latched_h=ctx->rtc.h; ctx->rtc.latched_dl=ctx->rtc.dl;
                    ctx->rtc.latched_dh=ctx->rtc.dh;
                } else ctx->rtc.latch_state=0u;
            }
        } else if (gbrt_sgdk_is_mbc5(ctx->mbc_type)) {
            if (addr < 0x2000u) ctx->ram_enabled=((value & 0x0Fu) == 0x0Au) ? 1u : 0u;
            else if (addr < 0x3000u) { ctx->rom_bank_low=value; ctx->rom_bank=gb_resolve_rom_bank(ctx,0x4000u); }
            else if (addr < 0x4000u) { ctx->rom_bank_upper=(uint8_t)(value & 0x01u); ctx->rom_bank=gb_resolve_rom_bank(ctx,0x4000u); }
            else if (addr < 0x6000u) ctx->ram_bank=(uint8_t)(value & 0x0Fu);
        }
        return;
    }
    if (addr >= 0x8000u && addr < 0xA000u) { gbmd_write8(ctx->md, addr, value); return; }
    if (addr < 0xC000u) {
        if (!ctx->ram_enabled) return;
        if (gbrt_sgdk_is_mbc3(ctx->mbc_type) && ctx->rtc_mode) {
            switch (ctx->rtc_reg) {
                case 0x08u: ctx->rtc.s=(uint8_t)(value % 60u); break;
                case 0x09u: ctx->rtc.m=(uint8_t)(value % 60u); break;
                case 0x0Au: ctx->rtc.h=(uint8_t)(value % 24u); break;
                case 0x0Bu: ctx->rtc.dl=value; break;
                case 0x0Cu: ctx->rtc.dh=value; ctx->rtc.active=(value & 0x40u) ? 0u : 1u; break;
                default: break;
            }
            return;
        }
        if (ctx->eram_size == 0u) return;
        const size_t off=((size_t)ctx->ram_bank * 0x2000u) + (size_t)(addr - 0xA000u);
        gbrt_sgdk_eram_write(ctx,off,value);
        return;
    }
    if (addr >= 0xC000u && addr < 0xD000u) { ctx->wram[addr - 0xC000u] = value; return; }
    if (addr >= 0xD000u && addr < 0xE000u) { ctx->wram[0x1000u + (addr - 0xD000u)] = value; return; }
    if (addr >= 0xE000u && addr < 0xFE00u) { gb_write8(ctx, (uint16_t)(addr - 0x2000u), value); return; }
    if (addr >= 0xFE00u && addr <= 0xFE9Fu) { gbmd_write8(ctx->md, addr, value); return; }
    if (addr >= 0xFF00u && addr <= 0xFF7Fu) {
        if (addr == 0xFF04u) {
            ctx->div_counter = 0u;
            if (ctx->md) ctx->md->io[0x04u] = 0u;
            return;
        }
        if (addr == 0xFF07u) {
            if (ctx->md) ctx->md->io[0x07u] = (uint8_t)(value & 0x07u);
            return;
        }
        if (addr != 0xFF44u) gbmd_write8(ctx->md, addr, value);
        return;
    }
    if (addr >= 0xFF80u && addr <= 0xFFFEu) { ctx->hram[addr - 0xFF80u] = value; return; }
    if (addr == 0xFFFFu) { ctx->ie = value; return; }
    /* ROM/MBC writes are ignored for this ROM-only POC. */
}

static unsigned gbrt_sgdk_timer_div_bit(uint8_t tac) {
    switch (tac & 0x03u) {
        case 0x00u: return 9u; /* 4096 Hz */
        case 0x01u: return 3u; /* 262144 Hz */
        case 0x02u: return 5u; /* 65536 Hz */
        default:    return 7u; /* 16384 Hz */
    }
}

static void gbrt_sgdk_apply_tima_increments(GBContext *ctx, uint32_t increments) {
    if (!ctx->md || increments == 0u) return;
    uint8_t *io = ctx->md->io;
    uint32_t tima = io[0x05u];
    const uint32_t tma = io[0x06u];

    const uint32_t to_first_overflow = 256u - tima;
    if (increments < to_first_overflow) {
        io[0x05u] = (uint8_t)(tima + increments);
        return;
    }

    increments -= to_first_overflow;
    io[0x0Fu] |= 0x04u; /* IF.Timer */
    const uint32_t overflow_period = 256u - tma; /* always 1..256 */

    /* IF is a level bit, so multiple overflows before the CPU services it
       collapse to one pending interrupt just as they do on the hardware. */
    const uint32_t remainder = increments % overflow_period;
    io[0x05u] = (uint8_t)(tma + remainder);
}

static void gbrt_sgdk_rtc_tick(GBContext *ctx, uint32_t cycles) {
    if (!ctx->rtc.active || !gbrt_sgdk_cart_has_rtc(ctx->mbc_type)) return;
    ctx->rtc.cycle_remainder += cycles;
    while (ctx->rtc.cycle_remainder >= 4194304u) {
        ctx->rtc.cycle_remainder -= 4194304u;
        if (++ctx->rtc.s < 60u) continue;
        ctx->rtc.s=0u;
        if (++ctx->rtc.m < 60u) continue;
        ctx->rtc.m=0u;
        if (++ctx->rtc.h < 24u) continue;
        ctx->rtc.h=0u;
        uint16_t d=(uint16_t)(ctx->rtc.dl | ((uint16_t)(ctx->rtc.dh & 1u) << 8));
        d++;
        ctx->rtc.dl=(uint8_t)d;
        if (d > 0x1FFu) { ctx->rtc.dh |= 0x80u; ctx->rtc.dh &= 0xFEu; }
        else ctx->rtc.dh=(uint8_t)((ctx->rtc.dh & 0xFEu) | ((d >> 8) & 1u));
    }
}

void gbrt_sgdk_advance_host_clock(GBContext *ctx, uint32_t cycles) {
    gbrt_sgdk_rtc_tick(ctx, cycles);
    const uint16_t old_div = ctx->div_counter;
    ctx->div_counter = (uint16_t)(ctx->div_counter + cycles);
    if (!ctx->md) return;

    const uint8_t tac = ctx->md->io[0x07u];
    if ((tac & 0x04u) == 0u) return;

    const unsigned bit = gbrt_sgdk_timer_div_bit(tac);
    const uint32_t period = 1u << (bit + 1u);
    const uint32_t phase = (uint32_t)old_div & (period - 1u);
    const uint32_t increments = (phase + cycles) / period;
    gbrt_sgdk_apply_tima_increments(ctx, increments);
}

void gbrt_sgdk_tick_slow(GBContext *ctx, uint32_t cycles) {
    gbrt_sgdk_advance_host_clock(ctx, cycles);
    uint32_t total = (uint32_t)ctx->line_cycles + cycles;
    while (total >= 456u) {
        total -= 456u;
        const uint8_t old = ctx->ly;
        ctx->ly = (uint8_t)((ctx->ly + 1u) % 154u);
        if (ctx->md) ctx->md->io[0x44] = ctx->ly;
        if (old == 153u && ctx->ly == 0u) {
            ctx->frames++;
            ctx->stopped = 1;
        }
    }
    ctx->line_cycles = (uint16_t)total;
}

void gbrt_sgdk_set_host_vblank_sync(GBContext *ctx, bool enabled) {
    ctx->host_vblank_sync = enabled ? 1u : 0u;
}

static uint8_t gbrt_sgdk_pending_interrupts(const GBContext *ctx) {
    const uint8_t if_reg = (ctx->md != NULL) ? ctx->md->io[0x0Fu] : 0u;
    return (uint8_t)(if_reg & ctx->ie & 0x1Fu);
}

static bool gbrt_sgdk_service_interrupt(GBContext *ctx) {
    if (!ctx->ime) return false;
    const uint8_t pending = gbrt_sgdk_pending_interrupts(ctx);
    if (!pending) return false;

    uint8_t bit;
    uint16_t vector;
    if (pending & 0x01u) { bit = 0x01u; vector = 0x0040u; }
    else if (pending & 0x02u) { bit = 0x02u; vector = 0x0048u; }
    else if (pending & 0x04u) { bit = 0x04u; vector = 0x0050u; }
    else if (pending & 0x08u) { bit = 0x08u; vector = 0x0058u; }
    else { bit = 0x10u; vector = 0x0060u; }

    const uint16_t return_pc = ctx->pc;
    ctx->ime = 0;
    ctx->halted = 0;

    ctx->sp--;
    gb_write8(ctx, ctx->sp, (uint8_t)(return_pc >> 8));
    ctx->sp--;
    gb_write8(ctx, ctx->sp, (uint8_t)return_pc);

    if (ctx->md) gbmd_write8(ctx->md, 0xFF0Fu, (uint8_t)(ctx->md->io[0x0Fu] & (uint8_t)~bit));
    ctx->pc = vector;
    gb_tick(ctx, 20u);
    return true;
}

void gbrt_sgdk_run_frame(GBContext *ctx, void (*run_generated)(GBContext *)) {
    ctx->stopped = 0;
    ctx->host_ly_reads = 0;
#ifdef GBRT_SGDK_PROFILE
    ctx->profile_generated_entries++;
#endif

    if (ctx->host_vblank_sync) {
        ctx->ly = 144u;
        if (ctx->md) {
            ctx->md->io[0x44] = 144u;
            if (ctx->md->io[0x40u] & 0x80u) {
                ctx->md->io[0x0Fu] |= 0x01u; /* LCD on: host VBlank raises IF.VBlank. */
            }
        }

        /* Service all interrupt sources already pending at this host boundary
           in GB priority order. A cap prevents a malformed guest from making
           one Mega Drive frame unbounded. */
        for (unsigned dispatches = 0; dispatches < 8u; ++dispatches) {
            const uint8_t pending = gbrt_sgdk_pending_interrupts(ctx);
            if (ctx->halted && pending) ctx->halted = 0;
            const bool serviced = gbrt_sgdk_service_interrupt(ctx);

            run_generated(ctx);
            if (ctx->stopped) break;

            const uint8_t after = gbrt_sgdk_pending_interrupts(ctx);
            if (ctx->halted && !(ctx->ime && after)) break;
            if (!serviced && !after) break;
            if (!ctx->halted && !(ctx->ime && after)) break;
        }

        /* Let DIV/TIMA elapse during the host frame after guest work has
           established the current TAC/TIMA/TMA state. This matches startup
           behavior much better than advancing before the guest initializes
           its timer. */
        gbrt_sgdk_advance_host_clock(ctx, 70224u);

        /* A low-frequency Timer IRQ that expires during this frame can be
           delivered before returning to the host, even when the guest was in
           HALT. VBlank was already raised at the frame boundary above. */
        for (unsigned timer_dispatches = 0; timer_dispatches < 4u; ++timer_dispatches) {
            const uint8_t pending = gbrt_sgdk_pending_interrupts(ctx);
            if (!(ctx->ime && pending)) break;
            if (ctx->halted) ctx->halted = 0;
            if (!gbrt_sgdk_service_interrupt(ctx)) break;
            ctx->stopped = 0;
            run_generated(ctx);
        }

        ctx->frames++;
    } else {
        const uint32_t before = ctx->frames;
        while (!ctx->stopped && !ctx->halted && ctx->frames == before) run_generated(ctx);
    }

    if (ctx->md) gbmd_flush_video(ctx->md);
}

void gb_and8(GBContext *ctx, uint8_t v) { ctx->a &= v; set_zn(ctx, ctx->a); ctx->f_n=0; ctx->f_h=1; ctx->f_c=0; }
void gb_or8(GBContext *ctx, uint8_t v) { ctx->a |= v; set_zn(ctx, ctx->a); ctx->f_n=ctx->f_h=ctx->f_c=0; }
void gb_xor8(GBContext *ctx, uint8_t v) { ctx->a ^= v; set_zn(ctx, ctx->a); ctx->f_n=ctx->f_h=ctx->f_c=0; }
void gb_cp8(GBContext *ctx, uint8_t v) { uint8_t a=ctx->a; ctx->f_z=(a==v); ctx->f_n=1; ctx->f_h=((a&0xF)<(v&0xF)); ctx->f_c=(a<v); }
void gb_sbc8(GBContext *ctx, uint8_t v) { unsigned c=ctx->f_c?1u:0u, a=ctx->a, r=a-v-c; ctx->a=(uint8_t)r; ctx->f_z=(ctx->a==0); ctx->f_n=1; ctx->f_h=((a&15u)<((v&15u)+c)); ctx->f_c=(a<(unsigned)v+c); }
uint8_t gb_inc8(GBContext *ctx, uint8_t v) { uint8_t r=(uint8_t)(v+1u); ctx->f_z=(r==0); ctx->f_n=0; ctx->f_h=((v&0x0F)==0x0F); return r; }
uint8_t gb_dec8(GBContext *ctx, uint8_t v) { uint8_t r=(uint8_t)(v-1u); ctx->f_z=(r==0); ctx->f_n=1; ctx->f_h=((v&0x0F)==0); return r; }

void gbrt_timed_inc16(GBContext *ctx, uint16_t *v) { *v=(uint16_t)(*v+1u); gb_tick(ctx,8); }
void gbrt_timed_dec16(GBContext *ctx, uint16_t *v) { *v=(uint16_t)(*v-1u); gb_tick(ctx,8); }
void gbrt_timed_hl_write_auto(GBContext *ctx, uint8_t value, int8_t delta) { uint16_t a=ctx->hl; gb_write8(ctx,a,value); ctx->hl=(uint16_t)(ctx->hl+delta); gb_tick(ctx,8); }
void gbrt_timed_jump(GBContext *ctx, uint16_t target, uint8_t instruction_cycles) { ctx->pc=target; gb_tick(ctx,instruction_cycles); }
void gbrt_timed_call(GBContext *ctx, uint16_t target, uint16_t return_address) {
    ctx->sp--; gb_write8(ctx,ctx->sp,(uint8_t)(return_address>>8));
    ctx->sp--; gb_write8(ctx,ctx->sp,(uint8_t)return_address);
    ctx->pc=target; gb_tick(ctx,24u);
}
void gbrt_timed_ld_hl_sp_n(GBContext *ctx, uint16_t immediate_addr) { int8_t e=(int8_t)gb_read8(ctx, immediate_addr); uint16_t sp=ctx->sp; uint16_t u=(uint8_t)e; ctx->hl=(uint16_t)(sp+e); ctx->f_z=ctx->f_n=0; ctx->f_h=((sp&0xFu)+(u&0xFu)>0xFu); ctx->f_c=((sp&0xFFu)+(u&0xFFu)>0xFFu); gb_tick(ctx,12); }

static uint16_t pop16(GBContext *ctx) { uint8_t lo=gb_read8(ctx,ctx->sp++); uint8_t hi=gb_read8(ctx,ctx->sp++); return (uint16_t)(lo|((uint16_t)hi<<8)); }
void gbrt_timed_ret(GBContext *ctx) { ctx->pc=pop16(ctx); gb_tick(ctx,16); }
void gbrt_timed_ret_cc(GBContext *ctx) { ctx->pc=pop16(ctx); gb_tick(ctx,20); }
void gbrt_timed_reti(GBContext *ctx) { ctx->pc=pop16(ctx); ctx->ime=1; ctx->ime_pending=0; gb_tick(ctx,16); }
void gbrt_execute_halt(GBContext *ctx, uint16_t next_pc, uint32_t cycles) {
    ctx->pc = next_pc;
    if (!ctx->ime && gbrt_sgdk_pending_interrupts(ctx)) ctx->halted = 0;
    else ctx->halted = 1;
    gb_tick(ctx, cycles);
}

bool gbrt_try_execute_hram_stub(GBContext *ctx, uint16_t addr) { (void)ctx;(void)addr;return false; }
bool gbrt_try_execute_highmem_stub(GBContext *ctx, uint16_t addr) { (void)ctx;(void)addr;return false; }
bool gbrt_try_execute_ram_stub(GBContext *ctx, uint16_t addr) { (void)ctx;(void)addr;return false; }
void gbrt_execute_dispatch_fallback(GBContext *ctx, uint16_t bank, uint16_t addr, GBDispatchFallbackReason reason, uint16_t variants) { (void)bank;(void)addr;(void)reason;(void)variants; ctx->fallback_hit=1; ctx->stopped=1; }
void gbrt_log_trace(GBContext *ctx, uint16_t bank, uint16_t addr) { (void)ctx;(void)bank;(void)addr; }
