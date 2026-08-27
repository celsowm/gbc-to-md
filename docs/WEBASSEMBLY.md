# Browser compilation / WebAssembly

The playground target is an entirely local conversion pipeline: a user drops a legally obtained `.gb` or `.gbc` file into the browser, the ROM never leaves the device, and the browser produces a Mega Drive / Genesis `rom.bin`.

## Proven milestones

### W0 — `gbrecomp_core` runs as WebAssembly

The pinned GB Recompiled v0.1.0 core builds under Emscripten without the desktop SDL runtime or pthreads. Its exported adapter accepts ROM bytes directly.

### W1 — ROM to generated C entirely in memory

The browser-oriented adapter performs:

```text
ROM bytes
  -> ROM::load_from_buffer
  -> analyze
  -> IRBuilder::build
  -> codegen::generate_output
  -> in-memory generated files
```

The generated files are exposed directly through the WebAssembly API; no temporary host filesystem is needed for the ROM-to-C exchange.

The WebAssembly adapter also accepts optional GB Recompiled annotations entirely in memory. `function`, `label`, and `data` annotations can provide explicit entry points/code-data boundaries and switch analysis to the conservative reachable-only path used by the large mapper stress fixtures.

### W1.5 — prepare generated sources for SGDK in memory

The adapter mirrors the native SGDK preparation:

- removes desktop `stdio` / `stdlib` dependencies and diagnostic-only paths from the generated control source;
- replaces direct retained-ROM header access with `gbrt_sgdk_rom_read8`;
- discards the generated multi-megabyte `*_rom.c` initializer;
- discards the desktop `*_main.c` wrapper;
- creates a `.rodata_binf` assembly blob using `.incbin "browserrom.gb"`.

The caller retains the uploaded ROM bytes and provides them to the target assembler as the virtual `browserrom.gb` binary include.

### W2 — C to Motorola 68000 using WebAssembly tools

The main path uses the pinned `romdev-toolchain-m68k-gcc@0.3.0` package. It provides WebAssembly builds of:

- GCC `cc1` targeting Motorola 68000;
- `m68k-elf-as`;
- `m68k-elf-ld`;
- `m68k-elf-objcopy`;
- Genesis/SGDK build glue and the required target share tree.

The LLVM/Clang M68k experiments remain under `wasm/llvm/` as research, but they are no longer a dependency of the browser conversion path.

### W3 — GB ROM through both WebAssembly toolchains to `rom.bin`

The `basicdemo.gb` synthetic fixture completes this pipeline without a native m68k cross-compiler:

```text
basicdemo.gb
  -> gbrecomp.wasm
  -> SGDK-prepared generated artifacts
  -> generated C + GBRT-SGDK + GBMDBackend + SGDK main glue
  -> m68k-gcc.wasm
  -> m68k-elf-as.wasm
  -> m68k-elf-ld.wasm
  -> m68k-elf-objcopy.wasm
  -> Genesis padding/checksum finalization
  -> rom.bin
```

With the mapper-enabled SGDK seed used by the current Pages build, the basic fixture produces:

- input Game Boy ROM: 32,768 bytes;
- output Mega Drive ROM: 524,288 bytes;
- header prefix: `SEGA SSF`;
- stored/verified Genesis checksum: `0x185c`.

### W3.5 — 8 MiB MBC5 / far-ROM through the WebAssembly target toolchain

Proven by Browser toolchain run `33031532872`.

The Pages/toolchain path rebuilds SGDK with `ENABLE_BANK_SWITCH=1`, persists the matching `libmd.seed.a` and source hash, and enables `GBRT_SGDK_USE_FAR_ROM` for retained guest ROMs above 4 MiB.

The annotated MBC5 stress fixture proves:

- input GB ROM: 8,388,608 bytes / 512 banks;
- all 511 switchable executable banks represented;
- official SGDK/SEGA far-ROM mapper path enabled;
- C compilation, assembly, link, and objcopy performed through WebAssembly tools;
- output Mega Drive ROM: 8,912,896 bytes;
- header prefix: `SEGA SSF`;
- checksum: `0xbce4`;
- SHA-256: `e8a31e8a301091c987db1811b16794352439b499c587e588cb9865a093305527`;
- measured total on that Actions runner: about 16.3 seconds.

This also demonstrates why annotations are first-class: explicit bank entry points prevented the analyzer from treating an immediate/data byte at `18:4001` as a separate function.

### W4 — actual Chromium playground produces a cartridge

Proven in real Google Chrome 151 by the Pages E2E harness.

The test opens the packaged `site/`, selects `basicdemo.gb`, clicks **Recompile to C**, clicks **Build Mega Drive ROM**, downloads the result, then independently checks the SEGA header and checksum.

Observed result:

- downloaded ROM: 524,288 bytes;
- header: `SEGA SSF`;
- checksum: `0x185c`;
- browser UI build time: about 10.19 seconds.

This is not a Node simulation of the UI path: the real page, Web Worker host, MEMFS, compiler/linker WebAssembly modules, finalizer, and browser download are exercised by Chromium.

## Current optimization / validation gate

The same real-Chromium test is also exercised with the 8 MiB MBC5 fixture and its `.annotations` sidecar. Correctness is already proven by the all-WebAssembly Node host, but the first browser attempt exceeded a 90-second UI-test timeout because the page created a fresh Worker and repeatedly reloaded/compiled the GCC WebAssembly module for each generated C translation unit.

The browser host now uses a persistent toolchain Worker and caches each compiled `WebAssembly.Module`, while still instantiating a fresh Emscripten runtime per compiler invocation (`EXIT_RUNTIME=1`). This removes repeated WASM compilation from multi-file builds. The large Chromium gate remains the final browser performance/packaging validation before the Pages artifact is deployed.

## Playground packaging

The Pages artifact contains:

- `gbrecomp_core.wasm` adapter;
- m68k GCC/binutils WebAssembly modules;
- browser host/Worker glue;
- mapper-enabled SGDK source tree and matching persisted seed;
- GBRT-SGDK + GBMDBackend sources;
- SGDK share manifest;
- third-party provenance/license notices.

The playground accepts `.gb` / `.gbc` directly and an optional `.annotations` sidecar. Both remain local to browser memory.

## Constraints

- Never upload or persist user ROM bytes.
- Never ship commercial ROMs or generated derivatives in the repository.
- Do not equate a successful build with whole-game compatibility.
- Keep single-threaded GB recompilation as the baseline so the playground does not require cross-origin isolation merely for pthreads.
- Preserve the host and native SGDK CI paths as reference implementations while the browser toolchain evolves.
- Preserve third-party license notices and provenance for WebAssembly compiler/toolchain assets staged into the playground.
