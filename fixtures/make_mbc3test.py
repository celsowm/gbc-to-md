#!/usr/bin/env python3
from pathlib import Path
ROM_SIZE=128*0x4000
rom=bytearray([0])*ROM_SIZE
logo=bytes([0xCE,0xED,0x66,0x66,0xCC,0x0D,0x00,0x0B,0x03,0x73,0x00,0x83,0x00,0x0C,0x00,0x0D,0x00,0x08,0x11,0x1F,0x88,0x89,0x00,0x0E,0xDC,0xCC,0x6E,0xE6,0xDD,0xDD,0xD9,0x99,0xBB,0xBB,0x67,0x63,0x6E,0x0E,0xEC,0xCC,0xDD,0xDC,0x99,0x9F,0xBB,0xB9,0x33,0x3E])
rom[0x100:0x104]=bytes([0x00,0xC3,0x50,0x01]); rom[0x104:0x134]=logo
rom[0x134:0x13F]=b'MBC3SGDKV7\0'; rom[0x147]=0x10; rom[0x148]=0x06; rom[0x149]=0x03
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
for bank in range(1,128):
    off=bank*0x4000; rom[off:off+3]=bytes([0x3E,bank,0xC9])
m=A(0x150); m.e(0x31); m.u16(0xFFFE); m.e(0x21); m.u16(0xC100); m.e(0x0E,0x01) # SP, HL output, C=bank
m.label('banks'); m.e(0x79); m.sta(0x2000); m.call(0x4000); m.e(0x22); m.e(0x0C,0x79,0xFE,0x80); m.jr(0x20,'banks') # LD A,C; ... INC C; CP 128
m.lda(0); m.sta(0x2000); m.call(0x4000); m.sta(0xC180)
m.lda(0x0A); m.sta(0x0000)
for bank,val in enumerate((0xB0,0xB1,0xB2,0xB3)):
    m.lda(bank); m.sta(0x4000); m.lda(val); m.sta(0xA000)
for bank in range(4):
    m.lda(bank); m.sta(0x4000); m.lda_mem(0xA000); m.sta(0xC190+bank)
for reg,val in ((0x08,58),(0x09,59),(0x0A,23),(0x0B,0xFE),(0x0C,0x01)):
    m.lda(reg); m.sta(0x4000); m.lda(val); m.sta(0xA000)
m.lda(0); m.sta(0x6000); m.lda(1); m.sta(0x6000)
for i,reg in enumerate(range(0x08,0x0D)):
    m.lda(reg); m.sta(0x4000); m.lda_mem(0xA000); m.sta(0xC1A0+i)
m.lda(0); m.sta(0x4000); m.lda(0); m.sta(0x0000); m.lda_mem(0xA000); m.sta(0xC1A5)
m.lda(0x99); m.sta(0xC000); m.e(0x76,0x18,0xFD)
code=m.finish(); rom[0x150:0x150+len(code)]=code
chk=0
for i in range(0x134,0x14D): chk=(chk-rom[i]-1)&0xFF
rom[0x14D]=chk; g=(sum(rom)-rom[0x14E]-rom[0x14F])&0xFFFF; rom[0x14E]=g>>8; rom[0x14F]=g&255
base=Path(__file__).parent; (base/'mbc3test.gb').write_bytes(rom)
(base/'mbc3test.annotations').write_text('function 00:0150 Main\n'+''.join(f'function {b:02x}:4000 Bank{b:03d}\n' for b in range(1,128)))
print(f'wrote mbc3test.gb {len(rom)} bytes main={len(code)} annotated_banks=127')
