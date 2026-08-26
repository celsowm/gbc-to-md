#include "gbrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
    GBContext c; memset(&c,0,sizeof(c)); c.mbc_type=0x01; c.rom_size=128u*0x4000u; c.rom_bank_low=0; c.rom_bank_upper=1;
    assert(gb_resolve_rom_bank(&c,0x0000)==0u);
    assert(gb_resolve_rom_bank(&c,0x4000)==0x21u); /* low 0 maps to 1, high bits retained */
    c.mbc_mode=1;
    assert(gb_resolve_rom_bank(&c,0x0000)==0x20u);
    assert(gb_resolve_rom_bank(&c,0x4000)==0x21u);
    c.rom_bank_low=2; assert(gb_resolve_rom_bank(&c,0x4000)==0x22u);
    c.rom_bank_upper=3; c.rom_bank_low=31; assert(gb_resolve_rom_bank(&c,0x4000)==0x7Fu);
    /* MBC1M uses a 4-bit low register and shifts upper bits by four. */
    c.rom_size=64u*0x4000u; c.mbc1_multicart=1; c.rom_bank_upper=2; c.rom_bank_low=3; c.mbc_mode=1;
    assert(gb_resolve_rom_bank(&c,0x0000)==0x20u);
    assert(gb_resolve_rom_bank(&c,0x4000)==0x23u);
    c.rom_bank_low=0; assert(gb_resolve_rom_bank(&c,0x4000)==0x21u);
    /* Register semantics: mode0 fixes RAM bank 0; mode1 follows upper register. */
    c.mbc1_multicart=0; c.rom_size=4u*0x4000u; c.mbc_mode=0; c.ram_bank=0;
    gb_write8(&c,0x4000,3); assert(c.ram_bank==0);
    gb_write8(&c,0x6000,1); assert(c.ram_bank==3);
    gb_write8(&c,0x6000,0); assert(c.ram_bank==0);
    puts("PASS: MBC1 upper-bank, mode-1 low window, wrap and MBC1M formulas");
    return 0;
}
