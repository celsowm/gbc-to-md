# Performance notes

## What is being measured

The current measurements are host-side structural counters and wall-clock microbenchmarks. They are useful for identifying obviously impossible execution models, but they are **not** direct Mega Drive frame-rate measurements.

A real answer requires m68k code generation, disassembly, cycle analysis, and emulator/hardware profiling.

## Host-VBlank collapse

The original cycle-oriented LCD model advances a complete Game Boy frame, approximately 70,224 guest cycles, including large amounts of idle polling in simple fixtures.

For the synthetic polling fixture, host-VBlank scheduling reduces that work to roughly:

```text
cycle-oriented model     ~70,224 guest cycles/frame
                         ~8,700+ tick calls/frame

host-VBlank model        ~464 guest cycles/frame
                         ~58 tick calls/frame
```

The exact x86 wall-clock speedup varies by run and machine. The stable point is the structural reduction in guest work.

## IRQ-driven execution

The VBlank IRQ fixture is smaller still because the guest spends most of its time in `HALT` and wakes only for an interrupt. The integrated `cakegame` fixture includes input, collision, OAM, scrolling, timer animation, and map mutation while staying far below the cost of software scanline traversal.

## Generated-code scaling

Synthetic mapper stress testing showed that banked **native code** grows much more gently than the C representation of a retained binary ROM.

Representative POC measurements were approximately:

```text
fixture       guest ROM     generated core C     generated rom.c     x86 -Os text proxy
MBC1            64 KiB          ~157 KiB             ~410 KiB             ~29 KiB
MBC3             2 MiB          ~264 KiB             ~13 MiB              ~47 KiB
MBC5             8 MiB          ~652 KiB             ~52 MiB             ~100 KiB
```

These numbers motivated the raw `.incbin` target path.

## Why `.incbin` matters

For the 8 MiB fixture, compiling the generated C byte-array representation of the ROM consumed on the order of a gigabyte of host compiler RSS in the POC. Turning the raw binary into a linked blob was effectively instantaneous by comparison and used a tiny fraction of the memory.

The SGDK target therefore never intentionally routes large retained ROMs through a C initializer.

## Work-RAM budget

A 32 KiB guest ERAM array inside `GBContext` is too expensive for a target with 64 KiB of 68000 WRAM once stack, SGDK state, backend shadows, and game state are included.

Routing guest ERAM to cartridge SRAM returns the context+backend host-size proxy to roughly the high-teen KiB range.

## Next measurements

The next meaningful performance work is:

1. produce reproducible `m68k-elf-gcc` ROMs in CI;
2. record linked section sizes;
3. disassemble hot generated paths;
4. estimate 68000 cycles for frame/IRQ/game loops;
5. boot in an emulator with profiler/debugger support;
6. repeat on hardware if practical.
