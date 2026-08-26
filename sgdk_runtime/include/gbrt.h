#ifndef GBRT_SGDK_MIN_H
#define GBRT_SGDK_MIN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "gbmd_backend.h"

typedef enum {
    GB_MODEL_DMG = 0,
    GB_MODEL_CGB = 1
} GBModel;

typedef struct {
    GBModel model;
    bool cgb_compatibility_mode;
    bool cartridge_supports_cgb;
    bool cartridge_requires_cgb;
    bool enable_bootrom;
    bool enable_audio;
    bool enable_serial;
    unsigned speed_percent;
    bool native_presentation_enabled;
} GBConfig;

typedef enum {
    GB_DISPATCH_FALLBACK_ADDRESS_NOT_COMPILED = 1,
    GB_DISPATCH_FALLBACK_BANK_NOT_COMPILED = 2,
    GB_DISPATCH_FALLBACK_PAGE_NOT_COMPILED = 3,
    GB_DISPATCH_FALLBACK_WRITABLE_HRAM = 4,
} GBDispatchFallbackReason;

typedef struct GBContext {
    union { struct { uint8_t f, a; }; uint16_t af; };
    union { struct { uint8_t c, b; }; uint16_t bc; };
    union { struct { uint8_t e, d; }; uint16_t de; };
    union { struct { uint8_t l, h; }; uint16_t hl; };
    uint16_t sp;
    uint16_t pc;
    uint8_t f_z, f_n, f_h, f_c;
    uint8_t ime, ime_pending, halted, stopped, single_step_mode;
    uint8_t mbc_type;
    uint8_t ram_enabled;
    uint8_t mbc_mode;
    uint8_t rom_bank_low;
    uint8_t rom_bank_upper;
    uint8_t ram_bank;
    uint8_t mbc1_multicart;
    uint8_t rtc_mode;
    uint8_t rtc_reg;
    uint16_t rom_bank;
    uint8_t wram_bank;
    struct { uint8_t active; } dma;
    uint16_t div_counter;
    uint8_t trace_entries_enabled;
    struct {
        uint8_t s, m, h, dl, dh;
        uint8_t latched_s, latched_m, latched_h, latched_dl, latched_dh;
        uint8_t latch_state;
        uint8_t active;
        uint32_t cycle_remainder;
    } rtc;

    const uint8_t *rom;
    size_t rom_size;
    uint8_t wram[0x2000];
    /* Host/reference builds can keep MBC1 ERAM in 68000-style work RAM for
       deterministic tests. Real SGDK builds can define GBRT_SGDK_USE_CART_SRAM
       and move these 32 KiB entirely into cartridge SRAM. */
#ifndef GBRT_SGDK_USE_CART_SRAM
    uint8_t eram[0x8000];
#endif
    size_t eram_size;
    uint8_t hram[0x7F];
    uint8_t *io;
    uint8_t ie;

    GBMDBackend *md;
    uint16_t line_cycles;
    uint8_t ly;
    uint32_t frames;
    uint8_t fallback_hit;

    /* Sega 512 KiB mapper cache for guest ROM data above the directly
       addressable range. Region 7 (0x380000-0x3FFFFF) is used by the
       GBRT-SGDK far-ROM accessor; region 4 remains available to cart SRAM. */
    uint8_t far_rom_bank;
    uint8_t far_rom_bank_valid;
    uint16_t far_rom_reserved;
    uint32_t far_rom_switches;

    /* Mega Drive host-vblank mode: avoid emulating the GB PPU clock. */
    uint8_t host_vblank_sync;
    uint8_t host_ly_reads;
    uint16_t host_reserved;
#ifdef GBRT_SGDK_PROFILE
    uint64_t total_cycles;
    uint32_t profile_tick_calls;
    uint32_t profile_read_calls;
    uint32_t profile_write_calls;
    uint32_t profile_generated_entries;
#endif
} GBContext;

extern bool gbrt_trace_enabled;
extern bool gbrt_benchmark_fast_tick_enabled;

void gbrt_sgdk_context_init(GBContext *ctx, GBMDBackend *md);
void gbrt_sgdk_run_frame(GBContext *ctx, void (*run_generated)(GBContext *));
void gbrt_sgdk_set_host_vblank_sync(GBContext *ctx, bool enabled);
size_t gbrt_sgdk_context_ram_bytes(void);

void gb_context_load_rom(GBContext *ctx, const uint8_t *rom, size_t rom_size);
void gb_context_reset(GBContext *ctx, bool hard_reset);
uint16_t gb_resolve_rom_bank(const GBContext *ctx, uint16_t addr);
uint8_t gbrt_sgdk_rom_read8(GBContext *ctx, size_t offset);
uint8_t gb_read8(GBContext *ctx, uint16_t addr);
void gb_write8(GBContext *ctx, uint16_t addr, uint8_t value);
void gbrt_sgdk_tick_slow(GBContext *ctx, uint32_t cycles);
void gbrt_sgdk_advance_host_clock(GBContext *ctx, uint32_t cycles);

/* Hot path used by generated code. In the Mega Drive host-vblank mode this
   stays entirely inline and never enters the scanline scheduler. */
static inline void gb_tick(GBContext *ctx, uint32_t cycles) {
#ifdef GBRT_SGDK_PROFILE
    ctx->profile_tick_calls++;
    ctx->total_cycles += cycles;
#endif
    if (ctx->ime_pending) { ctx->ime = 1; ctx->ime_pending = 0; }
    /* Host-VBlank mode advances DIV/TIMA from host frame time, not from the
       small number of guest instructions that remain after PPU waits are
       collapsed. This keeps timers running while the guest is HALTed. */
    if (ctx->host_vblank_sync) return;
    gbrt_sgdk_tick_slow(ctx, cycles);
}

void gb_and8(GBContext *ctx, uint8_t value);
void gb_or8(GBContext *ctx, uint8_t value);
void gb_xor8(GBContext *ctx, uint8_t value);
void gb_cp8(GBContext *ctx, uint8_t value);
void gb_sbc8(GBContext *ctx, uint8_t value);
uint8_t gb_inc8(GBContext *ctx, uint8_t value);
uint8_t gb_dec8(GBContext *ctx, uint8_t value);

void gbrt_timed_inc16(GBContext *ctx, uint16_t *value);
void gbrt_timed_dec16(GBContext *ctx, uint16_t *value);
void gbrt_timed_hl_write_auto(GBContext *ctx, uint8_t value, int8_t delta);
void gbrt_timed_ld_hl_sp_n(GBContext *ctx, uint16_t immediate_addr);
void gbrt_timed_jump(GBContext *ctx, uint16_t target, uint8_t instruction_cycles);
void gbrt_timed_call(GBContext *ctx, uint16_t target, uint16_t return_address);
void gbrt_timed_ret(GBContext *ctx);
void gbrt_timed_ret_cc(GBContext *ctx);
void gbrt_timed_reti(GBContext *ctx);
void gbrt_execute_halt(GBContext *ctx, uint16_t next_pc, uint32_t cycles);

static inline bool gbrt_generated_safepoint(GBContext *ctx) { return ctx->stopped != 0; }
static inline void gbrt_note_generated_direct_transition(GBContext *ctx) { (void)ctx; }
static inline void gbrt_note_generated_indirect_dispatch(GBContext *ctx) { (void)ctx; }
static inline void gbrt_note_generated_generic_read(GBContext *ctx) { (void)ctx; }
static inline void gbrt_note_generated_generic_write(GBContext *ctx) { (void)ctx; }
static inline void gbrt_note_generated_specialized_read(GBContext *ctx) { (void)ctx; }
static inline void gbrt_note_generated_specialized_write(GBContext *ctx) { (void)ctx; }

bool gbrt_try_execute_hram_stub(GBContext *ctx, uint16_t addr);
bool gbrt_try_execute_highmem_stub(GBContext *ctx, uint16_t addr);
bool gbrt_try_execute_ram_stub(GBContext *ctx, uint16_t addr);
void gbrt_execute_dispatch_fallback(GBContext *ctx, uint16_t bank, uint16_t addr,
                                    GBDispatchFallbackReason reason,
                                    uint16_t compiled_bank_variants);
void gbrt_log_trace(GBContext *ctx, uint16_t bank, uint16_t addr);

#endif
