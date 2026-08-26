#!/usr/bin/env python3
"""Generate a compact ROM-only DMG homebrew for the GBRecomp->SGDK POC.

v4 deliberately exercises more than movement:
- 4 background/sprite tiles
- a 32x32 scrolling background
- three OAM sprites
- D-pad movement
- a deterministic right-side collision against sprite #1
- a sparse runtime tilemap mutation when the collision happens

No external assembler is required.
"""
from pathlib import Path


class Asm:
    def __init__(self, base=0x150):
        self.base = base
        self.code = bytearray()
        self.labels = {}
        self.fixups = []

    @property
    def pc(self):
        return self.base + len(self.code)

    def label(self, name):
        if name in self.labels:
            raise ValueError(f"duplicate label {name}")
        self.labels[name] = self.pc

    def emit(self, *bs):
        self.code.extend(b & 0xFF for b in bs)

    def u16(self, value):
        self.emit(value & 0xFF, (value >> 8) & 0xFF)

    def ld_a_imm(self, value):
        self.emit(0x3E, value)

    def ld_a_mem(self, addr):
        self.emit(0xFA)
        self.u16(addr)

    def ld_mem_a(self, addr):
        self.emit(0xEA)
        self.u16(addr)

    def ldh_a(self, off):
        self.emit(0xF0, off)

    def ldh_write_a(self, off):
        self.emit(0xE0, off)

    def jr(self, opcode, label):
        self.emit(opcode, 0)
        self.fixups.append((len(self.code) - 1, label))

    def finish(self):
        for operand_index, label in self.fixups:
            if label not in self.labels:
                raise ValueError(f"unknown label {label}")
            next_pc = self.base + operand_index + 1
            delta = self.labels[label] - next_pc
            if not -128 <= delta <= 127:
                raise ValueError(f"JR {label} out of range: {delta}")
            self.code[operand_index] = delta & 0xFF
        return bytes(self.code)


ROM_SIZE = 32768
rom = bytearray(ROM_SIZE)
logo = bytes([
    0xCE,0xED,0x66,0x66,0xCC,0x0D,0x00,0x0B,0x03,0x73,0x00,0x83,0x00,0x0C,0x00,0x0D,
    0x00,0x08,0x11,0x1F,0x88,0x89,0x00,0x0E,0xDC,0xCC,0x6E,0xE6,0xDD,0xDD,0xD9,0x99,
    0xBB,0xBB,0x67,0x63,0x6E,0x0E,0xEC,0xCC,0xDD,0xDC,0x99,0x9F,0xBB,0xB9,0x33,0x3E
])
rom[0x100:0x104] = bytes([0x00, 0xC3, 0x50, 0x01])
rom[0x104:0x134] = logo
rom[0x134:0x13F] = b"BASICDEMO\0\0"
rom[0x143] = 0x00  # DMG
rom[0x147] = 0x00  # ROM only
rom[0x148] = 0x00  # 32 KiB
rom[0x149] = 0x00  # no cart RAM

blank = bytes(16)
basicdemo = bytes([
    0x00,0x18, 0x3C,0x24, 0x7E,0x42, 0x7E,0x5A,
    0x3C,0x24, 0x3C,0x3C, 0x18,0x18, 0x00,0x00
])
bg_pattern = bytes([
    0xAA,0x00, 0x55,0x00, 0xAA,0x00, 0x55,0x00,
    0xAA,0x00, 0x55,0x00, 0xAA,0x00, 0x55,0x00
])
block = bytes([
    0xFF,0x81, 0x81,0xBD, 0x81,0xA5, 0x81,0xA5,
    0x81,0xBD, 0x81,0x81, 0xFF,0xFF, 0x00,0x00
])
for index, tile in enumerate((blank, basicdemo, bg_pattern, block)):
    start = 0x1000 + index * 16
    rom[start:start + 16] = tile

YVAR = 0xC000
XVAR = 0xC001
JTMP = 0xC002
SXVAR = 0xC003
SYVAR = 0xC004
HITVAR = 0xC005
PLAYER_MAX_X = 104
COLLISION_TILE_ADDR = 0x990D  # row 8, col 13 in $9800 map

a = Asm()
a.emit(0x31); a.u16(0xFFFE)  # LD SP,$FFFE
a.emit(0xF3)                  # DI

a.label('wait_vblank_boot')
a.ldh_a(0x44); a.emit(0xFE, 144); a.jr(0x20, 'wait_vblank_boot')

a.ld_a_imm(0); a.ldh_write_a(0x40)  # LCD off

# Copy 4 tiles (64 bytes) from ROM $1000 -> VRAM $8000.
a.emit(0x21); a.u16(0x8000)
a.emit(0x11); a.u16(0x1000)
a.emit(0x01); a.u16(64)
a.label('copy_tiles')
a.emit(0x1A, 0x22, 0x13, 0x0B, 0x78, 0xB1)
a.jr(0x20, 'copy_tiles')

# Fill 32x32 BG map at $9800 with tile 2.
a.emit(0x21); a.u16(0x9800)
a.emit(0x01); a.u16(1024)
a.emit(0x16, 2)
a.label('fill_map')
a.emit(0x7A, 0x22, 0x0B, 0x78, 0xB1)
a.jr(0x20, 'fill_map')

# Palettes.
a.ld_a_imm(0xE4); a.ldh_write_a(0x47); a.ldh_write_a(0x48)

# Runtime state.
a.ld_a_imm(88); a.ld_mem_a(YVAR)
a.ld_a_imm(88); a.ld_mem_a(XVAR)
a.ld_a_imm(0);  a.ld_mem_a(SXVAR); a.ld_mem_a(SYVAR); a.ld_mem_a(HITVAR)

# OAM sprite 0: player.
a.ld_a_imm(88); a.ld_mem_a(0xFE00)
a.ld_a_imm(88); a.ld_mem_a(0xFE01)
a.ld_a_imm(1);  a.ld_mem_a(0xFE02)
a.ld_a_imm(0);  a.ld_mem_a(0xFE03)

# OAM sprite 1: collision block. OAM x=112 => screen x=104.
a.ld_a_imm(88);  a.ld_mem_a(0xFE04)
a.ld_a_imm(112); a.ld_mem_a(0xFE05)
a.ld_a_imm(3);   a.ld_mem_a(0xFE06)
a.ld_a_imm(0);   a.ld_mem_a(0xFE07)

# OAM sprite 2: another visible object to exercise a sparse sprite chain.
a.ld_a_imm(104); a.ld_mem_a(0xFE08)
a.ld_a_imm(152); a.ld_mem_a(0xFE09)
a.ld_a_imm(3);   a.ld_mem_a(0xFE0A)
a.ld_a_imm(0);   a.ld_mem_a(0xFE0B)

# LCD on, BG + sprites, tile data at $8000.
a.ld_a_imm(0x93); a.ldh_write_a(0x40)

a.label('main')
a.label('wait_vblank')
a.ldh_a(0x44); a.emit(0xFE,144); a.jr(0x20,'wait_vblank')

# Select directions and snapshot JOYP.
a.ld_a_imm(0x20); a.ldh_write_a(0x00)
a.ldh_a(0x00); a.ld_mem_a(JTMP)

# RIGHT bit0 active-low. Stop at PLAYER_MAX_X and mutate the map once hit.
a.ld_a_mem(JTMP); a.emit(0xE6,0x01); a.jr(0x20,'skip_right')
a.ld_a_mem(XVAR); a.emit(0xFE, PLAYER_MAX_X); a.jr(0x28, 'hit_right')
a.emit(0x3C); a.ld_mem_a(XVAR)
a.ld_a_mem(SXVAR); a.emit(0x3C); a.ld_mem_a(SXVAR)
a.jr(0x18, 'skip_right')
a.label('hit_right')
a.ld_a_imm(1); a.ld_mem_a(HITVAR)
a.ld_a_imm(3); a.ld_mem_a(COLLISION_TILE_ADDR)
a.label('skip_right')

# LEFT bit1.
a.ld_a_mem(JTMP); a.emit(0xE6,0x02); a.jr(0x20,'skip_left')
a.ld_a_mem(XVAR); a.emit(0x3D); a.ld_mem_a(XVAR)
a.ld_a_mem(SXVAR); a.emit(0x3D); a.ld_mem_a(SXVAR)
a.label('skip_left')

# UP bit2.
a.ld_a_mem(JTMP); a.emit(0xE6,0x04); a.jr(0x20,'skip_up')
a.ld_a_mem(YVAR); a.emit(0x3D); a.ld_mem_a(YVAR)
a.ld_a_mem(SYVAR); a.emit(0x3D); a.ld_mem_a(SYVAR)
a.label('skip_up')

# DOWN bit3.
a.ld_a_mem(JTMP); a.emit(0xE6,0x08); a.jr(0x20,'skip_down')
a.ld_a_mem(YVAR); a.emit(0x3C); a.ld_mem_a(YVAR)
a.ld_a_mem(SYVAR); a.emit(0x3C); a.ld_mem_a(SYVAR)
a.label('skip_down')

# OAM player + scroll update.
a.ld_a_mem(YVAR); a.ld_mem_a(0xFE00)
a.ld_a_mem(XVAR); a.ld_mem_a(0xFE01)
a.ld_a_mem(SYVAR); a.ldh_write_a(0x42)
a.ld_a_mem(SXVAR); a.ldh_write_a(0x43)

# Do not update again on the same LY=144 line.
a.label('wait_leave_vblank_line')
a.ldh_a(0x44); a.emit(0xFE,144); a.jr(0x28,'wait_leave_vblank_line')
a.emit(0xC3); a.u16(a.labels['main'])

code = a.finish()
rom[0x150:0x150 + len(code)] = code
for vec in [0x00,0x08,0x10,0x18,0x20,0x28,0x30,0x38]:
    rom[vec] = 0xC9
for vec in [0x40,0x48,0x50,0x58,0x60]:
    rom[vec] = 0xD9

checksum = 0
for i in range(0x134, 0x14D):
    checksum = (checksum - rom[i] - 1) & 0xFF
rom[0x14D] = checksum
global_sum = (sum(rom) - rom[0x14E] - rom[0x14F]) & 0xFFFF
rom[0x14E] = (global_sum >> 8) & 0xFF
rom[0x14F] = global_sum & 0xFF

out = Path(__file__).with_name('basicdemo.gb')
out.write_bytes(rom)
print(f'wrote {out} ({len(rom)} bytes)')
print(f'code: {len(code)} bytes @ $0150-$%04X' % (0x150 + len(code) - 1))
print(f'header checksum=0x{checksum:02X}, global=0x{global_sum:04X}')
for key in ['main','wait_vblank','hit_right','skip_right','skip_left','skip_up','skip_down']:
    print(f'{key}: ${a.labels[key]:04X}')
print(f'collision marker map address: ${COLLISION_TILE_ADDR:04X}')
