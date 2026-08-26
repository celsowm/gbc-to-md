# Compatibility status

This document distinguishes **implemented/tested behavior** from work that is still missing. A passing synthetic fixture is evidence for that behavior, not a claim that every commercial game using the feature is compatible.

## CPU/recompiler integration

| Area | Status | Evidence |
|---|---|---|
| Real `gbrecomp` generated C | Tested | All generated fixtures |
| Bank-qualified generated functions | Tested | MBC1/MBC3/MBC5 fixtures |
| Banked `CALL` helper | Tested | MBC1+ fixtures |
| Generic fallback-free tested paths | Tested | Fixture assertions require `fallback=0` |
| Full LR35902/game coverage | Not claimed | Requires commercial compatibility work |

## Video and input

| Area | Status | Notes |
|---|---|---|
| 8x8 2bpp tiles | Tested | Expanded to Mega Drive 4bpp tile data |
| Background map writes | Tested | Sparse updates and dense row batching |
| OAM sprites | Tested | Position/tile/flip/visibility path |
| Sprite list linking | Tested by adapter/syntax | Sparse updates preserve hardware list reachability |
| Scroll X/Y | Tested | `cakegame` and `basicdemo` fixtures |
| D-pad/buttons | Tested | Active-low GB JOYP model -> SGDK JOY bits |
| Dynamic DMG palette remapping | Missing | Current SGDK demo palette is fixed greyscale |
| CGB palette RAM | Missing | Not yet implemented |
| CGB VRAM bank behavior | Missing | Not yet implemented |
| Window layer | Incomplete | No dedicated compatibility fixture yet |
| Raster/STAT effects | Missing | Host-VBlank model is not scanline-accurate |

## Interrupts and timing

| Area | Status | Notes |
|---|---|---|
| VBlank IRQ | Tested | Interrupt vector, stack, `RETI`, `HALT` |
| Timer IRQ | Tested subset | Frame-time progression and four TAC selectors |
| `DIV/TIMA/TMA/TAC` basic behavior | Tested subset | Not exact edge/glitch behavior |
| STAT IRQ | Missing | Required for raster-sensitive games |
| Serial IRQ | Missing | Not implemented |
| Joypad IRQ | Missing | Polling path exists; IRQ scheduling does not |
| Exact LCD timing | Not claimed | Fast path intentionally collapses idle scanline traversal |
| Exact timer reload/glitch cases | Missing | Current model is intentionally simpler |

## Cartridge mappers

| Mapper/feature | Status | Notes |
|---|---|---|
| ROM-only | Tested | Base fixtures |
| MBC1 | Tested | Low/high bits, mode 0/1, RAM banking, wrapping |
| MBC1M | Tested math/selection path | Synthetic coverage |
| MBC3 ROM/RAM | Tested | 128-bank synthetic fixture |
| MBC3 RTC/latch | Tested subset | Synthetic register/latch behavior; no wall-clock persistence |
| MBC5 ROM | Tested | 512 banks / 9-bit bank number |
| MBC5 RAM | Tested subset | Four ERAM banks in fixture |
| MBC5 rumble | Missing | Not implemented |
| ERAM >32 KiB | Missing | Current target SRAM strategy is designed around 32 KiB fixture coverage |

## Mega Drive cartridge backing

| Feature | Status | Notes |
|---|---|---|
| Guest ERAM in 68000 WRAM | Tested | Useful reference path, expensive in RAM |
| Guest ERAM in cart SRAM | Tested with host stub / SGDK syntax | Target path uses SGDK SRAM APIs |
| Retained ROM <=4 MiB | Implemented | Direct path where addressable |
| Retained ROM >4 MiB | Implemented/tested at host target branch | Uses SGDK official SEGA mapper model |
| 8 MiB MBC5 retained-ROM sweep | Tested | Includes target-branch mapper stub |
| Official mapper logical ceiling | Guarded | Build check rejects retained range beyond 32 MiB |

## Audio

Audio is not implemented in the target runtime. This is one of the largest remaining subsystems before broad game compatibility can be claimed.

## Memory

The current host-size proxy for `GBContext + GBMDBackend` is roughly in the high-teen KiB range when guest ERAM is moved to cartridge SRAM. Exact 68000 layout must be measured from a real m68k build because host pointer/alignment sizes differ.

## Current proof boundary

The project has strong host-side evidence for the architecture and mapper/timing fixtures. The next evidence level is a reproducible m68k/SGDK link plus emulator/hardware boot and real 68000 size/cycle measurements. The GitHub SGDK workflow exists to make that proof reproducible.
