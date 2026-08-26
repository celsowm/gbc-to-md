# Contributing

Thanks for helping with `gbc-to-md`. This is an experimental systems project, so compatibility claims need reproducible evidence.

## Setup

On Ubuntu/Debian:

```bash
sudo apt-get update
sudo apt-get install -y build-essential binutils curl git python3 ca-certificates openjdk-17-jre-headless
make doctor
make bootstrap
```

For real Mega Drive builds:

```bash
make bootstrap-sgdk
```

See `docs/BUILDING.md` for details.

## Development workflow

Start with the smallest test that covers your change. Mapper changes should use mapper fixtures; interrupt changes should use IRQ/timer fixtures; video changes should extend the backend fixture before being tested against a larger game loop.

Before submitting a change:

```bash
make verify-ci
```

If your change affects SGDK staging, far-ROM, SRAM, or linker assumptions and you have the cross-toolchain installed:

```bash
make sgdk
make sgdk-mbc5
```

## Legal and test-data policy

Do not add commercial ROMs or copyrighted game assets. New regression ROMs must be generated from source in `fixtures/` and must be safe to redistribute.

## Reporting compatibility

When reporting a successful game or feature, include the exact ROM metadata you are legally allowed to share, the `gbrecomp` version, the target commit, which automated tests pass, whether the result was only host-tested or also linked for m68k, and whether it was booted in an emulator or on hardware.
