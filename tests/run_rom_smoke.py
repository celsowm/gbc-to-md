#!/usr/bin/env python3
"""Tiny LR35902 subset runner for basicdemo.gb.

This is only a ROM sanity test. It is intentionally not the SGDK backend or a
replacement for upstream gbrecomp.
"""
from pathlib import Path

rom = Path(__file__).parents[1].joinpath('fixtures/basicdemo.gb').read_bytes()
mem = bytearray(65536)
mem[:len(rom)] = rom
A=B=C=D=E=H=L=0
SP=0
PC=0x100
Z=False
joy_select=0x30
ly=0
RIGHT=True

def read(addr):
    global ly
    if addr == 0xFF44:
        v=ly
        ly=(ly+1)%154
        return v
    if addr == 0xFF00:
        low=0x0F
        if (joy_select & 0x10)==0 and RIGHT:
            low &= ~0x01
        return 0xC0 | joy_select | low
    return mem[addr]

def write(addr, v):
    global joy_select
    v &= 0xFF
    if addr == 0xFF00:
        joy_select = v & 0x30
        mem[addr] = 0xC0 | joy_select | 0x0F
    elif addr < 0x8000:
        pass
    else:
        mem[addr]=v

def u16_at(pc): return mem[pc] | (mem[pc+1]<<8)
def s8(v): return v-256 if v & 0x80 else v

def set_bc(v):
    global B,C
    B=(v>>8)&0xFF; C=v&0xFF

def get_bc(): return (B<<8)|C

def set_de(v):
    global D,E
    D=(v>>8)&0xFF; E=v&0xFF

def get_de(): return (D<<8)|E

def set_hl(v):
    global H,L
    H=(v>>8)&0xFF; L=v&0xFF

def get_hl(): return (H<<8)|L

steps=0
x_start=None
x_after=None
collision_seen=False
COLLISION_TILE_ADDR=0x990D
while steps < 300000:
    steps += 1
    op=mem[PC]; PC=(PC+1)&0xFFFF
    if op==0x00: pass
    elif op==0xC3: PC=u16_at(PC)
    elif op==0x31: SP=u16_at(PC); PC+=2
    elif op==0xF3: pass
    elif op==0xF0:
        off=mem[PC]; PC+=1; A=read(0xFF00|off)
    elif op==0xFE:
        n=mem[PC]; PC+=1; Z=(A==n)
    elif op==0x20:
        d=s8(mem[PC]); PC+=1
        if not Z: PC=(PC+d)&0xFFFF
    elif op==0x28:
        d=s8(mem[PC]); PC+=1
        if Z: PC=(PC+d)&0xFFFF
    elif op==0x18:
        d=s8(mem[PC]); PC=(PC+1+d)&0xFFFF
    elif op==0x3E: A=mem[PC]; PC+=1
    elif op==0xE0:
        off=mem[PC]; PC+=1; write(0xFF00|off,A)
    elif op==0x21: set_hl(u16_at(PC)); PC+=2
    elif op==0x11: set_de(u16_at(PC)); PC+=2
    elif op==0x16: D=mem[PC]; PC+=1
    elif op==0x01: set_bc(u16_at(PC)); PC+=2
    elif op==0x1A: A=read(get_de())
    elif op==0x22:
        hl=get_hl(); write(hl,A); set_hl((hl+1)&0xFFFF)
    elif op==0x13: set_de((get_de()+1)&0xFFFF)
    elif op==0x0B: set_bc((get_bc()-1)&0xFFFF)
    elif op==0x78: A=B
    elif op==0x7A: A=D
    elif op==0xB1: A=(A|C)&0xFF; Z=(A==0)
    elif op==0xAF: A=0; Z=True
    elif op==0xEA:
        addr=u16_at(PC); PC+=2; write(addr,A)
        if addr==0xC001 and x_start is None: x_start=A
        elif addr==0xC001 and x_after is None and A != x_start: x_after=A
        if addr==COLLISION_TILE_ADDR and A==3: collision_seen=True
    elif op==0xFA:
        addr=u16_at(PC); PC+=2; A=read(addr)
    elif op==0xE6:
        n=mem[PC]; PC+=1; A &= n; Z=(A==0)
    elif op==0x3C: A=(A+1)&0xFF; Z=(A==0)
    elif op==0x3D: A=(A-1)&0xFF; Z=(A==0)
    else:
        raise SystemExit(f'unsupported opcode 0x{op:02X} at 0x{(PC-1)&0xFFFF:04X}')
    if collision_seen:
        break

assert mem[0x8000:0x8010] == bytes(16), 'blank tile copy failed'
assert mem[0x8010:0x8020] == rom[0x1010:0x1020], 'basicdemo tile copy failed'
assert mem[0xFE02] == 1, 'player sprite tile index not initialized'
assert mem[0xFE06] == 3, 'collision sprite tile index not initialized'
assert mem[0xFE0A] == 3, 'third sprite tile index not initialized'
assert x_start == 88, x_start
assert x_after == 89, x_after
assert mem[0xC001] == 104, mem[0xC001]
assert mem[COLLISION_TILE_ADDR] == 3, mem[COLLISION_TILE_ADDR]
assert collision_seen
print(f'PASS: basicdemo.gb booted, moved X {x_start}->104, stopped on sprite collision, and changed BG tile in {steps} LR35902 steps')
