# Architecture

## Overview

`gbc-to-md` is a target-runtime experiment around static recompilation. `gbrecomp` performs Game Boy CPU-code discovery and emits C. This repository supplies a compact runtime and Mega Drive backend for that generated code.

The important architectural boundary is:

```text
GB/GBC CPU semantics       -> generated C + GBContext helpers
GB/GBC hardware state     -> GBRT-SGDK runtime
Mega Drive presentation   -> GBMDBackend + SGDK callbacks
```

The target is intentionally framebuffer-free.

## Main components

### `sgdk_runtime/`

`GBContext` and helpers required by generated `gbrecomp` output. The target runtime owns guest-visible state such as registers, WRAM/HRAM, selected cartridge banks, interrupt state, timer state, ROM metadata, and optional profiling counters.

The target runtime deliberately omits desktop-only systems such as SDL, ImGui, a software LCD framebuffer, generic persistence UI, and normal runtime profiling/logging.

### `src/gbmd_backend.*`

A host-testable hardware translation layer. It stores compact Game Boy video/OAM shadows and exposes callbacks for:

- tile upload;
- background tile update;
- background row batching;
- sprite update;
- scroll update.

The SGDK application maps those callbacks to VDP/JOY operations.

### `sgdk/template/src/main.c`

A clean application template. `scripts/build-sgdk.sh` renders the selected generated game prefix into a disposable staging tree under `build/sgdk/<game>/`.

Tracked sources are never rewritten during a cartridge build.

## Video model

### Tiles

A Game Boy tile is 8x8 pixels at 2 bits per pixel. A Mega Drive tile is 8x8 at 4 bits per pixel. The backend expands the guest tile format into a Mega Drive-compatible tile while preserving the guest palette index semantics needed by the current fixtures.

### Background map

Sparse guest map writes become individual tilemap updates. Dense updates can be batched by row, which avoids turning a 32x32 map refresh into 1,024 separate SGDK calls.

### Sprites

Guest OAM entries are translated to Mega Drive sprite positions and tile attributes. The SGDK adapter maintains a linked hardware sprite list even for sparse guest OAM updates.

## Frame/timing model

The project has two timing concepts:

1. **Guest elapsed time**, which is still needed for timers and interrupt-visible behavior.
2. **Host frame scheduling**, which is driven by the Mega Drive VBlank for the fast path.

The host-VBlank mode avoids traversing all 154 Game Boy scanlines in software for conventional frame-driven programs. The timer can still advance by a virtual Game Boy frame interval while generated CPU code is halted.

This model is not intended to be cycle-accurate for raster tricks, STAT effects, or games that depend on exact LCD timing.

## Interrupt model

The compact runtime currently supports the tested VBlank and Timer paths:

```text
host VBlank / timer event
        |
        v
raise IF bit
        |
   IME + IE eligible?
        |
        v
push guest return PC
clear IME / clear serviced IF
jump to guest interrupt vector
        |
        v
recompiled ISR -> RETI -> HALT/game loop
```

The IRQ-driven fixtures validate stack restoration and return to `HALT`.

## Cartridge memory

### MBC1

The runtime models RAM enable, low ROM-bank bits, upper bank/RAM bits, mode selection, bank-zero remapping, lower-window mode behavior, wrapping, and the tested MBC1M behavior.

### MBC3

The runtime models ROM/RAM banking plus the tested RTC-register/latch subset used by the synthetic fixture.

### MBC5

The runtime models the 9-bit ROM bank number, including banks 256-511, and tested RAM-bank behavior.

## Guest ERAM on Mega Drive SRAM

With `GBRT_SGDK_USE_CART_SRAM`, guest ERAM is not stored in the 68000 work RAM context. The target accessors execute from RAM-safe code and briefly enable cartridge SRAM for each byte access.

This protects the Mega Drive work-RAM budget and naturally provides a future persistence path.

## Far retained ROM data

There are two different bank-switch layers and they must not be conflated.

### Guest bank selection

The Game Boy MBC chooses which guest bank is visible at `$4000-$7FFF`. `gbrecomp` can emit multiple native functions for the same guest address, keyed by guest ROM bank.

### Physical Mega Drive bank selection

The Mega Drive's official mapper exposes 512 KiB physical pages. The first 512 KiB region is fixed; higher regions can be remapped.

Recompiled code usually does not require physical mapping because it already exists as native code in the output image. Physical mapping is needed for retained **data bytes** from the original guest ROM.

The target funnels retained byte reads through `gbrt_sgdk_rom_read8()`. For far data the SGDK path uses the high far-data mapper region so the cartridge SRAM window remains separate.

## Raw ROM blob

Large guest ROMs are linked with assembler `.incbin` rather than generated C byte arrays:

```asm
.section .rodata
rom_size:
    .long <size>
rom_data:
    .incbin "src/game.gb"
```

This avoids feeding tens of megabytes of hexadecimal initializer source through m68k GCC.

`rom_size` intentionally precedes `rom_data` so startup can read the size without first mapping a symbol placed after a large blob.

## Post-link safety checks

For far-ROM builds, `scripts/build-sgdk.sh` inspects the linked ELF and rejects unsafe layouts when:

- executable text reaches the reserved high far-ROM mapping window;
- `rom_size` itself is placed in the far range;
- retained ROM data would exceed the official mapper's 32 MiB logical ceiling.

These checks turn linker-layout assumptions into explicit build failures.
