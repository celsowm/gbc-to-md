# Roadmap

The order below is based on technical risk rather than feature visibility.

## 1. Make the m68k/SGDK proof reproducible

- Keep the SGDK GitHub Actions build green.
- Archive `rom.bin`, `rom.out`, and symbol output as CI artifacts.
- Record linked section sizes and far-ROM layout checks.
- Add an automated emulator smoke test if a reliable headless Mega Drive emulator can be integrated legally and reproducibly.

## 2. Measure real 68000 cost

- Inspect generated assembly for runtime helpers and generated dispatch.
- Identify high-frequency generic reads/writes and bank dispatches.
- Replace host proxy measurements with m68k size/cycle data.
- Decide which helpers need target-specific specialization or inlining.

## 3. Complete high-value GB/CGB hardware behavior

- Dynamic DMG palette mapping.
- CGB palette RAM.
- CGB VRAM bank handling.
- Window-layer behavior.
- STAT interrupt scheduling and selected raster semantics.
- Joypad interrupt.
- Serial behavior needed by target games, if any.

## 4. Audio

- Decide whether to synthesize GB channels on the Z80, YM2612/PSG, or a hybrid.
- Add deterministic APU fixtures before game-specific audio work.
- Keep audio timing decoupled enough that it does not reintroduce a software-emulation frame bottleneck.

## 5. Cartridge persistence

- Persistent MBC3 RTC/wall-clock policy.
- Save-file format/versioning for guest ERAM backed by Mega Drive SRAM.
- MBC5 rumble semantics where relevant.

## 6. Better code/data discovery for real ROMs

Large synthetic tests already demonstrated that aggressive scanning can classify data bytes as plausible code. A commercial workflow needs stronger entry-point/symbol/trace/annotation support rather than blindly scanning every byte sequence.

## 7. Commercial-ROM compatibility ladder

Only after the m68k build and the missing hardware areas above are under control:

1. small ROM-only homebrew;
2. small mapper homebrew;
3. simple commercial DMG title;
4. larger MBC game;
5. CGB title;
6. eventually, complex targets such as Resident Evil Gaiden.

Each step should add reusable hardware support, not one-off game patches unless those patches are explicitly isolated as compatibility overrides.
