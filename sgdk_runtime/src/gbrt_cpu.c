#include "gbrt.h"

bool gbrt_test_breakpoint_enabled = false;

/*
 * LR35902 helpers required by GB Recompiled generated code.
 *
 * CPU semantics mirror the upstream GB Recompiled runtime. The SGDK runtime
 * intentionally substitutes the host/hardware layer only; generated CPU
 * operations must not depend on which frontend executes them.
 */

void gb_add8(GBContext *ctx, uint8_t value) {
    uint32_t res = (uint32_t)ctx->a + value;
    ctx->f_z = ((res & 0xFFu) == 0u);
    ctx->f_n = 0;
    ctx->f_h = (((ctx->a & 0x0Fu) + (value & 0x0Fu)) > 0x0Fu);
    ctx->f_c = (res > 0xFFu);
    ctx->a = (uint8_t)res;
}

void gb_adc8(GBContext *ctx, uint8_t value) {
    uint8_t carry = ctx->f_c ? 1u : 0u;
    uint32_t res = (uint32_t)ctx->a + value + carry;
    ctx->f_z = ((res & 0xFFu) == 0u);
    ctx->f_n = 0;
    ctx->f_h = (((ctx->a & 0x0Fu) + (value & 0x0Fu) + carry) > 0x0Fu);
    ctx->f_c = (res > 0xFFu);
    ctx->a = (uint8_t)res;
}

void gb_sub8(GBContext *ctx, uint8_t value) {
    ctx->f_z = (ctx->a == value);
    ctx->f_n = 1;
    ctx->f_h = ((ctx->a & 0x0Fu) < (value & 0x0Fu));
    ctx->f_c = (ctx->a < value);
    ctx->a = (uint8_t)(ctx->a - value);
}

void gb_add16(GBContext *ctx, uint16_t value) {
    uint32_t res = (uint32_t)ctx->hl + value;
    ctx->f_n = 0;
    ctx->f_h = (((ctx->hl & 0x0FFFu) + (value & 0x0FFFu)) > 0x0FFFu);
    ctx->f_c = (res > 0xFFFFu);
    ctx->hl = (uint16_t)res;
}

uint8_t gb_rlc(GBContext *ctx, uint8_t value) {
    ctx->f_c = (uint8_t)(value >> 7);
    value = (uint8_t)((value << 1) | ctx->f_c);
    ctx->f_z = (value == 0u);
    ctx->f_n = 0;
    ctx->f_h = 0;
    return value;
}

uint8_t gb_rrc(GBContext *ctx, uint8_t value) {
    ctx->f_c = (uint8_t)(value & 1u);
    value = (uint8_t)((value >> 1) | (ctx->f_c << 7));
    ctx->f_z = (value == 0u);
    ctx->f_n = 0;
    ctx->f_h = 0;
    return value;
}

uint8_t gb_rl(GBContext *ctx, uint8_t value) {
    uint8_t carry = ctx->f_c;
    ctx->f_c = (uint8_t)(value >> 7);
    value = (uint8_t)((value << 1) | carry);
    ctx->f_z = (value == 0u);
    ctx->f_n = 0;
    ctx->f_h = 0;
    return value;
}

uint8_t gb_rr(GBContext *ctx, uint8_t value) {
    uint8_t carry = ctx->f_c;
    ctx->f_c = (uint8_t)(value & 1u);
    value = (uint8_t)((value >> 1) | (carry << 7));
    ctx->f_z = (value == 0u);
    ctx->f_n = 0;
    ctx->f_h = 0;
    return value;
}

uint8_t gb_sla(GBContext *ctx, uint8_t value) {
    ctx->f_c = (uint8_t)(value >> 7);
    value = (uint8_t)(value << 1);
    ctx->f_z = (value == 0u);
    ctx->f_n = 0;
    ctx->f_h = 0;
    return value;
}

uint8_t gb_sra(GBContext *ctx, uint8_t value) {
    ctx->f_c = (uint8_t)(value & 1u);
    value = (uint8_t)((value >> 1) | (value & 0x80u));
    ctx->f_z = (value == 0u);
    ctx->f_n = 0;
    ctx->f_h = 0;
    return value;
}

uint8_t gb_swap(GBContext *ctx, uint8_t value) {
    value = (uint8_t)((value << 4) | (value >> 4));
    ctx->f_z = (value == 0u);
    ctx->f_n = 0;
    ctx->f_h = 0;
    ctx->f_c = 0;
    return value;
}

uint8_t gb_srl(GBContext *ctx, uint8_t value) {
    ctx->f_c = (uint8_t)(value & 1u);
    value = (uint8_t)(value >> 1);
    ctx->f_z = (value == 0u);
    ctx->f_n = 0;
    ctx->f_h = 0;
    return value;
}

void gb_bit(GBContext *ctx, uint8_t bit, uint8_t value) {
    ctx->f_z = ((value & (uint8_t)(1u << bit)) == 0u);
    ctx->f_n = 0;
    ctx->f_h = 1;
}

void gb_rlca(GBContext *ctx) {
    ctx->a = gb_rlc(ctx, ctx->a);
    ctx->f_z = 0;
}

void gb_rrca(GBContext *ctx) {
    ctx->a = gb_rrc(ctx, ctx->a);
    ctx->f_z = 0;
}

void gb_rla(GBContext *ctx) {
    ctx->a = gb_rl(ctx, ctx->a);
    ctx->f_z = 0;
}

void gb_rra(GBContext *ctx) {
    ctx->a = gb_rr(ctx, ctx->a);
    ctx->f_z = 0;
}

void gb_daa(GBContext *ctx) {
    uint8_t correction = 0u;
    uint8_t carry = ctx->f_c;

    if (!ctx->f_n) {
        if (ctx->f_h || (ctx->a & 0x0Fu) > 9u) correction |= 0x06u;
        if (ctx->f_c || ctx->a > 0x99u) {
            correction |= 0x60u;
            carry = 1u;
        }
        ctx->a = (uint8_t)(ctx->a + correction);
    } else {
        if (ctx->f_h) correction |= 0x06u;
        if (ctx->f_c) correction |= 0x60u;
        ctx->a = (uint8_t)(ctx->a - correction);
    }

    ctx->f_z = (ctx->a == 0u);
    ctx->f_h = 0;
    ctx->f_c = carry;
}

void gb_write16(GBContext *ctx, uint16_t addr, uint16_t value) {
    gb_write8(ctx, addr, (uint8_t)value);
    gb_write8(ctx, (uint16_t)(addr + 1u), (uint8_t)(value >> 8));
}

void gb_stop(GBContext *ctx) {
    if (!ctx) return;

    /*
     * This compact SGDK runtime does not model CGB double-speed STOP yet.
     * The DMG/normal-speed path yields execution until the host resumes it.
     */
    ctx->halted = 1u;
    ctx->stopped = 1u;
}

static void gbrt_sgdk_add_sp_value(GBContext *ctx, int8_t offset) {
    uint16_t sp = ctx->sp;
    uint16_t unsigned_offset = (uint8_t)offset;

    ctx->f_z = 0;
    ctx->f_n = 0;
    ctx->f_h = (((sp & 0x0Fu) + (unsigned_offset & 0x0Fu)) > 0x0Fu);
    ctx->f_c = (((sp & 0xFFu) + (unsigned_offset & 0xFFu)) > 0xFFu);
    ctx->sp = (uint16_t)(sp + offset);
}

void gbrt_timed_add_sp(GBContext *ctx, uint16_t immediate_addr) {
    gb_tick(ctx, 7u);
    const int8_t offset = (int8_t)gb_read8(ctx, immediate_addr);
    gb_tick(ctx, 9u);
    gbrt_sgdk_add_sp_value(ctx, offset);
}

uint8_t gbrt_timed_hl_read_auto(GBContext *ctx, int8_t delta) {
    const uint16_t addr = ctx->hl;

    gb_tick(ctx, 4u);
    ctx->hl = (uint16_t)(ctx->hl + delta);
    gb_tick(ctx, 3u);
    const uint8_t value = gb_read8(ctx, addr);
    gb_tick(ctx, 1u);
    return value;
}

void gbrt_timed_push16(GBContext *ctx, uint16_t value) {
    gb_tick(ctx, 8u);

    ctx->sp--;
    gb_tick(ctx, 3u);
    gb_write8(ctx, ctx->sp, (uint8_t)(value >> 8));
    gb_tick(ctx, 1u);

    ctx->sp--;
    gb_tick(ctx, 3u);
    gb_write8(ctx, ctx->sp, (uint8_t)value);
    gb_tick(ctx, 1u);
}

uint16_t gbrt_timed_pop16(GBContext *ctx) {
    gb_tick(ctx, 4u);

    gb_tick(ctx, 3u);
    const uint8_t low = gb_read8(ctx, ctx->sp++);
    gb_tick(ctx, 1u);

    gb_tick(ctx, 3u);
    const uint8_t high = gb_read8(ctx, ctx->sp++);
    gb_tick(ctx, 1u);

    return (uint16_t)(low | ((uint16_t)high << 8));
}

void gbrt_timed_rst(GBContext *ctx, uint8_t vector, uint16_t return_address) {
    gb_tick(ctx, 8u);

    ctx->sp--;
    gb_tick(ctx, 3u);
    gb_write8(ctx, ctx->sp, (uint8_t)(return_address >> 8));
    gb_tick(ctx, 1u);

    ctx->sp--;
    gb_tick(ctx, 3u);
    gb_write8(ctx, ctx->sp, (uint8_t)return_address);
    ctx->pc = vector;
    gb_tick(ctx, 1u);
}
