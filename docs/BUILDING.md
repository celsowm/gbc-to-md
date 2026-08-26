# Building

## Supported automated environment

The repository automation is developed around Linux x86_64. Host tests are standard C/Python/Bash and may work elsewhere, but the provided cross-toolchain bootstrap is currently Linux x86_64 specific.

Pinned versions live in `versions.env`:

- GB Recompiled `v0.1.0`;
- SGDK `v2.11`;
- m68k-elf GCC toolchain release `v13.2.0_latest` from `iratahack/m68k-elf-gcc`.

## Host prerequisites

Ubuntu/Debian:

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential binutils curl git python3 ca-certificates \
  openjdk-17-jre-headless
```

Check the environment:

```bash
make doctor
```

## GB Recompiled bootstrap

```bash
make bootstrap
```

The Linux archive is downloaded into `.deps/downloads/`, verified against the SHA-256 pinned in `versions.env`, and extracted under `.deps/gb-recompiled/`.

To remove it:

```bash
make distclean
```

## Host build and tests

Core smoke:

```bash
make test
```

Full host compatibility suite:

```bash
make verify-ci
```

Full suite plus generated-code size report:

```bash
make verify
```

## SGDK + m68k toolchain bootstrap

```bash
make bootstrap-sgdk
```

This performs two independent actions:

1. clones SGDK at the pinned tag into `.deps/SGDK/<version>`;
2. downloads and extracts the pinned Linux m68k-elf toolchain into `.deps/m68k-elf/<version>`.

The bootstrap never writes third-party binaries into tracked repository paths.

## Building `cakegame`

```bash
make sgdk
```

Equivalent manual invocation after dependencies are present:

```bash
GAME=cakegame \
SGDK_BUILD=release \
./scripts/build-sgdk.sh
```

Output:

```text
build/artifacts/cakegame/rom.bin
build/artifacts/cakegame/rom.out
build/artifacts/cakegame/symbol.txt
```

## Building the 8 MiB MBC5 fixture

The retained guest ROM is larger than the directly visible Mega Drive cartridge window, so SGDK's official mapper support must be enabled in the local SGDK checkout.

The convenience target does this only in the ignored `.deps/` checkout:

```bash
make sgdk-mbc5
```

Manual flow:

```bash
make bootstrap-sgdk
SGDK="$PWD/.deps/SGDK/v2.11" ./scripts/enable-sgdk-bank-switch.sh
GAME=mbc5test ./scripts/build-sgdk.sh
```

No tracked repository file is modified.

## Using an existing SGDK/toolchain installation

You can bypass `.deps/` and point the build at your own installation:

```bash
export SGDK=/path/to/SGDK
export PATH=/path/to/m68k-toolchain/bin:$PATH
export PREFIX=m68k-elf-
export SGDK_BUILD=release
GAME=cakegame ./scripts/build-sgdk.sh
```

For far-ROM builds, the selected SGDK checkout must have `ENABLE_BANK_SWITCH` set to `1` in `inc/config.h`.

## Staging behavior

Every SGDK build creates a disposable tree:

```text
build/sgdk/<game>/
```

It contains copies of:

- the backend;
- the compact runtime;
- sanitized generated C;
- the raw synthetic guest ROM;
- the generated assembler `.incbin` blob;
- a rendered `main.c` for that generated prefix.

This keeps the tracked working tree clean.

## Build variants

`GAME` currently accepts:

```text
basicdemo
irqtest
timertest
cakegame
mbc1test
mbc3test
mbc5test
```

`SGDK_BUILD` defaults to `release`. Debug builds may require additional SGDK host tools such as `convsym`, depending on the SGDK checkout.

## Common failures

### `gbrecomp` is missing

Run:

```bash
make bootstrap
```

### `m68k-elf-gcc` is missing

Run:

```bash
make bootstrap-sgdk
```

### Far-ROM build says bank switching is disabled

Run against the SGDK checkout you intend to use:

```bash
SGDK=/path/to/SGDK ./scripts/enable-sgdk-bank-switch.sh
```

### Post-link far-ROM layout check fails

Do not disable the check. The failure means executable text or retained ROM data overlaps an address range that the mapper path assumes it can remap safely. The correct fix is to change the linker/data strategy or reserve different mapper regions.
