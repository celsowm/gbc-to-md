# Changelog

All notable project changes will be documented here.

## Unreleased

### Added

- Public repository structure with reproducible dependency bootstrap.
- GitHub Actions host CI and SGDK cross-build workflow.
- Repository-wide `AGENTS.md`, contribution guide, architecture/build/testing documentation, and dependency provenance.
- Clean generated-output staging under `build/`.

## 0.1.0-alpha - 2026-08-26

### Added

- Framebuffer-free Game Boy to Mega Drive backend prototype.
- VBlank IRQ, Timer IRQ, `HALT`/`RETI`, input, scrolling, sprites, collision, and dynamic tilemap support.
- MBC1/MBC1M, MBC3+RTC, and MBC5 bank dispatch tests.
- Mega Drive cartridge SRAM backend for guest ERAM.
- Far-ROM access through the official SEGA 512 KiB mapper model for retained ROM data above 4 MiB.
- Synthetic stress fixtures up to 8 MiB / 512 MBC5 banks.
