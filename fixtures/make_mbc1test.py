#!/usr/bin/env python3
from pathlib import Path

ROM_SIZE=4*0x4000
rom=bytearray([0x00])*ROM_SIZE
logo=bytes([
0xCE,0xED,0x66,0x66,0xCC,0x0D,0x00,0x0B,0x03,0x73,0x00,0x83,0x00,0x0C,0x00,0x0D,
0x00,0x08,0x11,0x1F,0x88,0x89,0x00,0x0E,0xDC,0xCC,0x6E,0xE6,0xDD,0xDD,0xD9,0x99,
0xBB,0xBB,0x67,0x63,0x6E,0x0E,0xEC,0xCC,0xDD,0xDC,0x99,0x9F,0xBB,0xB9,0x33,0x3E])
rom[0x100:0x104]=bytes([0x00,0xC3,0x50,0x01]); rom[0x104:0x134]=logo
rom[0x134:0x13F]=b"MBC1SGDKV6\0"
rom[0x143]=0x00
rom[0x147]=0x03 # MBC1 + RAM + BATTERY
rom[0x148]=0x01 # 64 KiB ROM / 4 banks
rom[0x149]=0x03 # 32 KiB external RAM / 4 banks

class A:
    def __init__(self,base): self.base=base; self.b=bytearray(); self.labels={}; self.rel=[]
    @property
    def pc(self): return self.base+len(self.b)
    def e(self,*x): self.b.extend(v&255 for v in x)
    def u16(self,v): self.e(v&255,v>>8)
    def lda(self,v): self.e(0x3E,v)
    def sta(self,a): self.e(0xEA); self.u16(a)
    def lda_mem(self,a): self.e(0xFA); self.u16(a)
    def call(self,a): self.e(0xCD); self.u16(a)
    def label(self,n): self.labels[n]=self.pc
    def jr(self,op,n): self.e(op,0); self.rel.append((len(self.b)-1,n))
    def finish(self):
        for i,n in self.rel:
            d=self.labels[n]-(self.base+i+1)
            if not -128<=d<=127: raise ValueError((n,d))
            self.b[i]=d&255
        return bytes(self.b)

# banked functions at CPU address 0x4000 in banks 1,2,3
for bank,(val,dst) in enumerate(((0x11,0xC010),(0x22,0xC011),(0x33,0xC012)),start=1):
    off=bank*0x4000
    rom[off:off+7]=bytes([0x3E,val,0xEA,dst&255,dst>>8,0xC9,0x00])

m=A(0x150)
m.e(0x31); m.u16(0xFFFE) # SP
# clear result area C000..C02F
m.e(0x21); m.u16(0xC000); m.e(0x06,0x30); m.e(0xAF) # HL, B=48, XOR A
m.label('clr'); m.e(0x22,0x05); m.jr(0x20,'clr') # LDI (HL),A ; DEC B ; JR NZ
# enable ERAM
m.lda(0x0A); m.sta(0x0000)
# bank 1 -> call
m.lda(1); m.sta(0x2000); m.call(0x4000)
# bank 2
m.lda(2); m.sta(0x2000); m.call(0x4000)
# selecting 0 must map to bank 1. bank1 writes 0x11 to C010 again.
m.lda(0); m.sta(0x2000); m.call(0x4000)
# bank 3
m.lda(3); m.sta(0x2000); m.call(0x4000)
# ERAM bank0 in mode0
m.lda(0xA0); m.sta(0xA000)
# mode1 selects RAM bank using upper register
m.lda(1); m.sta(0x6000)
for bank,val in ((1,0xA1),(2,0xA2),(3,0xA3)):
    m.lda(bank); m.sta(0x4000); m.lda(val); m.sta(0xA000)
# read back bank0..3 into C020..C023
for bank,dst in ((0,0xC020),(1,0xC021),(2,0xC022),(3,0xC023)):
    m.lda(bank); m.sta(0x4000); m.lda_mem(0xA000); m.sta(dst)
# RAM disable => read FF
m.lda(0); m.sta(0x0000); m.lda_mem(0xA000); m.sta(0xC024)
# restore mode0 and select ROM bank2; read literal byte from bank2:4006 (0)
m.lda(0); m.sta(0x6000); m.lda(2); m.sta(0x2000); m.lda_mem(0x4000); m.sta(0xC025) # should be LD A opcode 0x3E
# completion marker
m.lda(0x99); m.sta(0xC000)
# HALT forever
m.e(0x76,0x18,0xFD)
code=m.finish(); rom[0x150:0x150+len(code)]=code
# Header checksum
chk=0
for i in range(0x134,0x14D): chk=(chk-rom[i]-1)&0xFF
rom[0x14D]=chk
g=(sum(rom)-rom[0x14E]-rom[0x14F])&0xFFFF; rom[0x14E]=g>>8; rom[0x14F]=g&255
out=Path(__file__).with_name('mbc1test.gb'); out.write_bytes(rom)
print(f'wrote {out} {len(rom)} bytes main={len(code)}')
