#!/usr/bin/env python3
"""Generate an IRQ-driven mini-game for the GBRecomp -> SGDK POC v5.

The main loop HALTs. VBlank sets a flag that authorizes exactly one gameplay
update; Timer IRQ only increments an animation counter. This deliberately
checks that Timer wakeups do not accidentally advance gameplay twice.
"""
from pathlib import Path

class Asm:
    def __init__(self, base=0x150):
        self.base=base; self.code=bytearray(); self.labels={}; self.rel=[]; self.abs=[]
    @property
    def pc(self): return self.base+len(self.code)
    def label(self,n): self.labels[n]=self.pc
    def emit(self,*bs): self.code.extend(b&0xff for b in bs)
    def u16(self,v): self.emit(v&0xff,(v>>8)&0xff)
    def ld_a_imm(self,v): self.emit(0x3E,v)
    def ld_a_mem(self,a): self.emit(0xFA); self.u16(a)
    def ld_mem_a(self,a): self.emit(0xEA); self.u16(a)
    def ldh_a(self,o): self.emit(0xF0,o)
    def ldh_write_a(self,o): self.emit(0xE0,o)
    def jr(self,op,label): self.emit(op,0); self.rel.append((len(self.code)-1,label))
    def jp(self,label): self.emit(0xC3,0,0); self.abs.append((len(self.code)-2,label))
    def finish(self):
        for i,l in self.rel:
            nxt=self.base+i+1; d=self.labels[l]-nxt
            if not -128<=d<=127: raise ValueError(f'JR {l} out of range {d}')
            self.code[i]=d&0xff
        for i,l in self.abs:
            v=self.labels[l]; self.code[i]=v&0xff; self.code[i+1]=(v>>8)&0xff
        return bytes(self.code)

ROM_SIZE=32768; rom=bytearray(ROM_SIZE)
logo=bytes([
0xCE,0xED,0x66,0x66,0xCC,0x0D,0x00,0x0B,0x03,0x73,0x00,0x83,0x00,0x0C,0x00,0x0D,
0x00,0x08,0x11,0x1F,0x88,0x89,0x00,0x0E,0xDC,0xCC,0x6E,0xE6,0xDD,0xDD,0xD9,0x99,
0xBB,0xBB,0x67,0x63,0x6E,0x0E,0xEC,0xCC,0xDD,0xDC,0x99,0x9F,0xBB,0xB9,0x33,0x3E])
rom[0x100:0x104]=bytes([0x00,0xC3,0x50,0x01]); rom[0x104:0x134]=logo
rom[0x134:0x13F]=b"CAKEIRQV5\0\0"; rom[0x143]=0; rom[0x147]=0; rom[0x148]=0; rom[0x149]=0
for vec in [0x00,0x08,0x10,0x18,0x20,0x28,0x30,0x38]: rom[vec]=0xC9
for vec in [0x48,0x58,0x60]: rom[vec]=0xD9

blank=bytes(16)
cake_a=bytes([0x00,0x18,0x3C,0x24,0x7E,0x42,0x7E,0x5A,0x3C,0x24,0x3C,0x3C,0x18,0x18,0,0])
cake_b=bytes([0x18,0x00,0x24,0x3C,0x42,0x7E,0x5A,0x7E,0x24,0x3C,0x3C,0x3C,0x18,0x18,0,0])
bg=bytes([0xAA,0,0x55,0,0xAA,0,0x55,0,0xAA,0,0x55,0,0xAA,0,0x55,0])
block=bytes([0xFF,0x81,0x81,0xBD,0x81,0xA5,0x81,0xA5,0x81,0xBD,0x81,0x81,0xFF,0xFF,0,0])
for i,t in enumerate((blank,cake_a,cake_b,bg,block)): rom[0x1000+i*16:0x1010+i*16]=t

Y=0xC000; X=0xC001; J=0xC002; SX=0xC003; SY=0xC004; HIT=0xC005
ANIM=0xC006; FRAME=0xC007; VBLFLAG=0xC008
MAXX=104; MARK=0x990D

a=Asm(); a.jp('start')
a.label('vblank_isr')
a.ld_a_imm(1); a.ld_mem_a(VBLFLAG)
a.ld_a_mem(FRAME); a.emit(0x3C); a.ld_mem_a(FRAME); a.emit(0xD9)
a.label('timer_isr')
a.ld_a_mem(ANIM); a.emit(0x3C); a.ld_mem_a(ANIM); a.emit(0xD9)
a.label('start')
a.emit(0x31); a.u16(0xFFFE); a.emit(0xF3)
a.ld_a_imm(0); a.ldh_write_a(0x40)  # LCD off
# tiles 5*16
a.emit(0x21); a.u16(0x8000); a.emit(0x11); a.u16(0x1000); a.emit(0x01); a.u16(80)
a.label('copy'); a.emit(0x1A,0x22,0x13,0x0B,0x78,0xB1); a.jr(0x20,'copy')
# full map tile3
a.emit(0x21); a.u16(0x9800); a.emit(0x01); a.u16(1024); a.emit(0x16,3)
a.label('fill'); a.emit(0x7A,0x22,0x0B,0x78,0xB1); a.jr(0x20,'fill')
# palettes
for off in (0x47,0x48): a.ld_a_imm(0xE4); a.ldh_write_a(off)
# vars clear / positions
a.ld_a_imm(88); a.ld_mem_a(Y); a.ld_mem_a(X)
a.ld_a_imm(0)
for addr in (SX,SY,HIT,ANIM,FRAME,VBLFLAG): a.ld_mem_a(addr)
# sprites player, obstacle, decoration
for addr,val in [(0xFE00,88),(0xFE01,88),(0xFE02,1),(0xFE03,0),
                 (0xFE04,88),(0xFE05,112),(0xFE06,4),(0xFE07,0),
                 (0xFE08,104),(0xFE09,152),(0xFE0A,4),(0xFE0B,0)]:
    a.ld_a_imm(val); a.ld_mem_a(addr)
# clear IF/DIV/TIMA/TMA then enable 4096Hz timer
a.ld_a_imm(0); a.ldh_write_a(0x0F); a.ldh_write_a(0x04); a.ldh_write_a(0x05); a.ldh_write_a(0x06)
a.ld_a_imm(0x04); a.ldh_write_a(0x07)
# LCD BG+sprites, IE vblank+timer, EI delay NOP
a.ld_a_imm(0x93); a.ldh_write_a(0x40)
a.ld_a_imm(0x05); a.ld_mem_a(0xFFFF)
a.emit(0xFB,0x00)

a.label('halt_loop'); a.emit(0x76)
# Timer IRQ also wakes HALT; only VBlank flag may run gameplay.
a.ld_a_mem(VBLFLAG); a.emit(0xB7); a.jr(0x28,'halt_loop')
a.ld_a_imm(0); a.ld_mem_a(VBLFLAG)
# Read directions
a.ld_a_imm(0x20); a.ldh_write_a(0x00); a.ldh_a(0x00); a.ld_mem_a(J)
# RIGHT
a.ld_a_mem(J); a.emit(0xE6,1); a.jr(0x20,'skip_r')
a.ld_a_mem(X); a.emit(0xFE,MAXX); a.jr(0x28,'hit_r'); a.emit(0x3C); a.ld_mem_a(X)
a.ld_a_mem(SX); a.emit(0x3C); a.ld_mem_a(SX); a.jr(0x18,'skip_r')
a.label('hit_r'); a.ld_a_imm(1); a.ld_mem_a(HIT); a.ld_a_imm(4); a.ld_mem_a(MARK)
a.label('skip_r')
# LEFT
a.ld_a_mem(J); a.emit(0xE6,2); a.jr(0x20,'skip_l'); a.ld_a_mem(X); a.emit(0x3D); a.ld_mem_a(X); a.ld_a_mem(SX); a.emit(0x3D); a.ld_mem_a(SX)
a.label('skip_l')
# UP
a.ld_a_mem(J); a.emit(0xE6,4); a.jr(0x20,'skip_u'); a.ld_a_mem(Y); a.emit(0x3D); a.ld_mem_a(Y); a.ld_a_mem(SY); a.emit(0x3D); a.ld_mem_a(SY)
a.label('skip_u')
# DOWN
a.ld_a_mem(J); a.emit(0xE6,8); a.jr(0x20,'skip_d'); a.ld_a_mem(Y); a.emit(0x3C); a.ld_mem_a(Y); a.ld_a_mem(SY); a.emit(0x3C); a.ld_mem_a(SY)
a.label('skip_d')
# OAM position
a.ld_a_mem(Y); a.ld_mem_a(0xFE00); a.ld_a_mem(X); a.ld_mem_a(0xFE01)
# animation tile: Timer parity chooses cake 1/2
a.ld_a_mem(ANIM); a.emit(0xE6,1); a.jr(0x28,'anim_a'); a.ld_a_imm(2); a.jr(0x18,'anim_store')
a.label('anim_a'); a.ld_a_imm(1)
a.label('anim_store'); a.ld_mem_a(0xFE02)
# scroll
a.ld_a_mem(SY); a.ldh_write_a(0x42); a.ld_a_mem(SX); a.ldh_write_a(0x43)
a.jp('halt_loop')

code=a.finish(); rom[0x150:0x150+len(code)]=code
# vectors jump to generated handlers
for vec,label in [(0x40,'vblank_isr'),(0x50,'timer_isr')]:
    dst=a.labels[label]; rom[vec:vec+3]=bytes([0xC3,dst&0xff,dst>>8])
checksum=0
for i in range(0x134,0x14D): checksum=(checksum-rom[i]-1)&0xff
rom[0x14D]=checksum; g=(sum(rom)-rom[0x14E]-rom[0x14F])&0xffff; rom[0x14E]=g>>8; rom[0x14F]=g&0xff
out=Path(__file__).with_name('cakegame.gb'); out.write_bytes(rom)
print(f'wrote {out} {len(rom)} bytes code={len(code)}')
for n in ('start','vblank_isr','timer_isr','halt_loop','hit_r'): print(f'{n}=${a.labels[n]:04X}')
