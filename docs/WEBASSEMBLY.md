# Browser compilation / WebAssembly

The long-term playground goal is an entirely local conversion pipeline: a user drops a legally obtained `.gb` or `.gbc` file into the browser, the ROM never leaves the device, and the browser produces a Mega Drive `rom.bin`.

## Proven milestones

### W0 — `gbrecomp_core` runs as WebAssembly

Proven by GitHub Actions run `33004253746`.

The pinned GB Recompiled v0.1.0 core compiles under Emscripten without linking the desktop SDL runtime or pthreads. The exported probe accepts ROM bytes directly and correctly parsed both a ROM-only fixture and an MBC1+RAM+battery fixture.

### W1 — ROM to generated C entirely in memory

Also proven by run `33004253746`.

The browser-oriented adapter performs:

```
ROM bytes
  -> ROM::load_from_buffer
  -> analyze
  -> IRBuilder::build
  -> codegen::generate_output
  -> in-memory generated files
```

For `basicdemo.gb`, the proof generated 9 C/header/metadata files totaling 363,488 text bytes. No temporary filesystem is needed for the input ROM or generated source exchange.

The proven artifact sizes for that run are:

- `gbrecomp_probe.js`: 33,123 bytes
- `gbrecomp_probe.wasm`: 509,860 bytes
- WASM SHA-256: `c84ffefd9707e842d02794d7e4563c1755b794bd0907026f077f8d050bdf5278`

The uploaded artifact is `gbrecomp-wasm-probe`, artifact ID `9619984635`.

## Current milestone

### W1.5 — prepare generated sources for SGDK in memory

The browser adapter mirrors the native SGDK preparation:

- remove the desktop `stdio` / `stdlib` dependencies and diagnostic paths from the generated control source;
- replace direct `rom_data[0x147]` access with `gbrt_sgdk_rom_read8`;
- discard the generated multi-megabyte `*_rom.c` initializer;
- discard the desktop `*_main.c` wrapper;
- create a `.rodata_binf` assembly blob using `.incbin "browserrom.gb"`.

The caller retains the original uploaded ROM bytes and will later provide them to the browser-side M68k compiler virtual filesystem under that name.

## Next milestones

### W2 — C to Motorola 68000 in the browser

Compile only the per-game generated C in the browser. Stable target components should be prebuilt in CI:

- GBRT-SGDK;
- GBMDBackend;
- SGDK boot/runtime objects;
- fixed libraries required by the target.

The browser compiler should therefore handle much less source than a complete SGDK build.

Candidate direction: a deliberately reduced LLVM/Clang WebAssembly build containing the M68k backend, or another browser-capable C-to-m68k compiler if it produces compatible ELF objects.

### W3 — browser linker and cartridge finalization

Link generated m68k objects, prebuilt target objects, and the retained guest-ROM blob. Apply the Mega Drive checksum/padding step in JavaScript/WebAssembly and enforce the same far-ROM layout guards as `scripts/build-sgdk.sh`.

### W4 — playground integration

Enable the existing **Convert to Mega Drive** button only when W1-W3 are present. All processing remains local to the browser.

## Constraints

- Never upload or persist user ROM bytes.
- Never ship commercial ROMs or generated derivatives in the repository.
- Do not equate a supported cartridge header with whole-game compatibility.
- Keep single-threaded browser recompilation as the baseline so GitHub Pages does not require cross-origin isolation merely to support pthreads.
- Preserve the host and real SGDK CI paths as ground truth while the browser toolchain matures.
