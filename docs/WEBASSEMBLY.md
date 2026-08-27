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

### W1.5 — prepare generated sources for SGDK in memory

The adapter mirrors the native SGDK preparation:

- removes desktop `stdio` / `stdlib` dependencies and diagnostic-only paths from the generated control source;
- replaces direct retained-ROM header access with `gbrt_sgdk_rom_read8`;
- discards the generated multi-megabyte `*_rom.c` initializer;
- discards the desktop `*_main.c` wrapper;
- creates a `.rodata_binf` assembly blob using `.incbin "browserrom.gb"`.

The caller retains the uploaded ROM bytes and provides them to the target assembler as the virtual `browserrom.gb` binary include.

### W2 — C to Motorola 68000 using WebAssembly tools

Proven in Browser toolchain run `33028822471`.

The main path now uses the pinned `romdev-toolchain-m68k-gcc@0.3.0` package rather than depending on the experimental LLVM path. It provides WebAssembly builds of:

- GCC `cc1` targeting Motorola 68000;
- `m68k-elf-as`;
- `m68k-elf-ld`;
- `m68k-elf-objcopy`;
- Genesis/SGDK build glue and the required target share tree.

A standalone all-WASM SGDK smoke test generated a 524,288-byte Genesis ROM with a valid `SEGA MEGA DRIVE` header and checksum `0x3e95`.

The LLVM/Clang M68k experiments remain under `wasm/llvm/` as research, but they are no longer a dependency of the browser conversion path.

### W3 — GB ROM through both WebAssembly toolchains to `rom.bin`

Also proven in run `33028822471`.

The `basicdemo.gb` synthetic fixture completed this pipeline without a native m68k cross-compiler:

```text
basicdemo.gb
  -> gbrecomp.wasm
  -> 8 SGDK-prepared generated artifacts
  -> generated C + GBRT-SGDK + GBMDBackend + SGDK main glue
  -> m68k-gcc.wasm
  -> m68k-elf-as.wasm
  -> m68k-elf-ld.wasm
  -> m68k-elf-objcopy.wasm
  -> Genesis padding/checksum finalization
  -> rom.bin
```

Measured on that GitHub Actions runner:

- input Game Boy ROM: 32,768 bytes;
- GB Recompiled stage: 93 ms;
- m68k compile + assembly + link + finalization: 6,771 ms;
- total: 6,864 ms;
- output Mega Drive ROM: 524,288 bytes;
- stored/verified Genesis checksum: `0x185c`;
- output SHA-256: `b7465a7125276c1a153518096ff643af3f1f1f155aa5cb26764c4efaeb6f5d1b`.

The CI artifact for that proof is named `gbc-to-md-browser-e2e-rom`.

## Current milestone

### W4 — run the same target toolchain from the actual playground

The repository now contains the browser host layer:

- `site/browser-build.js` — connects prepared GB Recompiled files to the Genesis build driver;
- `site/toolchain-worker.js` — instantiates each GCC/binutils WebAssembly tool in a Web Worker and mounts its virtual files;
- the playground exposes **Build Mega Drive ROM** and downloads the finalized `rom.bin`;
- the Pages build stages the pinned toolchain WebAssembly files, build modules, SGDK share tree, runtime/backend source, share manifest, and third-party notices.

The Pages packaging workflow executes the end-to-end GB-to-Genesis proof before it uploads the site artifact. The remaining deployment issue is repository-level GitHub Pages enablement: the default Actions token can build/upload the artifact but cannot create a previously disabled Pages site through `actions/configure-pages`.

## Current browser boundary

The browser release intentionally rejects retained guest ROMs larger than 4 MiB for now. The native SGDK backend already supports larger ROMs with the official SEGA mapper, but the browser path currently uses the stock SGDK seed bundled with the WebAssembly toolchain. Mapper-enabled seed/config parity must be proven before enabling browser far-ROM output.

This is a packaging/configuration boundary, not a limitation of GB Recompiled or the m68k WebAssembly compiler itself.

## Constraints

- Never upload or persist user ROM bytes.
- Never ship commercial ROMs or generated derivatives in the repository.
- Do not equate a successful build with whole-game compatibility.
- Keep single-threaded GB recompilation as the baseline so the playground does not require cross-origin isolation merely for pthreads.
- Preserve the host and native SGDK CI paths as reference implementations while the browser toolchain evolves.
- Preserve third-party license notices and provenance for WebAssembly compiler/toolchain assets staged into the playground.
