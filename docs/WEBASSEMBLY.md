# Browser compilation / WebAssembly

The long-term playground goal is an entirely local conversion pipeline: a user drops a legally obtained `.gb` or `.gbc` file into the browser, the ROM never leaves the device, and the browser produces a Mega Drive `rom.bin`.

## Why this is feasible

GB Recompiled v0.1.0 separates the recompiler into `gbrecomp_core` and its desktop runtime. `gbrecomp_core` contains the ROM parser, decoder, analyzer, bank tracker, IR, optimizer, and C emitter; SDL is not a dependency of that library. The upstream `ROM` API also provides `load_from_buffer`, so a browser adapter can consume uploaded bytes directly rather than relying on a host filesystem.

## Milestones

### W0 — core portability probe

`wasm/build-gbrecomp-probe.sh` builds the pinned upstream `gbrecomp_core` with Emscripten and links a tiny exported function from `wasm/probe.cpp`. The probe receives a ROM byte buffer and returns the parsed ROM-bank count and MBC code.

This is intentionally not the final compiler API. Its purpose is to prove that the exact pinned recompiler core can be compiled for WebAssembly without SDL, a native filesystem, or pthreads.

### W1 — in-memory recompiler API

Expose a browser-oriented API that accepts:

- ROM bytes;
- optional symbols / annotations;
- generation options.

Return generated C/header files as in-memory buffers. Do not expose the desktop CLI or its parallel-worker machinery to the browser.

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
