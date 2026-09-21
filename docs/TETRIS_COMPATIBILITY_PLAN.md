# Game Boy Tetris compatibility: evidence-driven bring-up

## Scope and known failure surfaces

Target: the Game Boy Tetris cartridge, **not** NES Tetris. The existing
gbc-to-md Pages pipeline recompiles GB code to C, compiles C to m68k, and
checks the resulting Mega Drive cartridge header and checksum. This proves
buildability, not that the game runs correctly on a Mega Drive.

The compact runtime previously incremented LY each time FF44 was read, then
forced a frame yield after ten reads. This is incompatible with guest
programs that poll the video line. The first fix replaces this with a
read-independent, guest-cycle-derived line within the host-VBlank frame
and tests repeated LY=148 reads. This is **not** a Tetris end-to-end pass.

## Next gates: capture evidence before game-specific changes

1. **Reproduce using a user-supplied, locally held GB Tetris image.** Record
   cartridge metadata and hash locally (never commit or upload ROM data).
   Capture whether failure is at browser recompilation, target link, cartridge
   boot, title, menu, or gameplay. Preserve compiler diagnostics and
   generated dispatch reports; keep stages distinct in the UI.
2. **Make runtime failure visible.** Add a compact, opt-in, ROM-independent
   trace ring for guest bank:PC, SP, flags, IF/IE/IME, LY/STAT, cycle budget,
   HALT and dispatch-fallback reason. Expose it to headless target tests.
   Avoid unbounded logging in release builds and keep target SRAM/WRAM costs
   explicit.
3. **Replace the uncompiled-address stop with a faithful LR35902 fallback.**
   The current gbrt_execute_dispatch_fallback only sets fallback_hit/stopped.
   An interpreter must preserve the generated GBContext ABI, fetch *live*
   bytes (especially RAM/HRAM), implement complete opcode/CB semantics,
   bus timing, stack, EI delay, HALT and interrupts, and return to compiled
   code at verified dispatchable addresses. Keep the compiled path default.
   The current hard-coded OAM-DMA HRAM stub should become an optional
   optimization checked against generic execution, not the only usable
   dynamic-code path.
4. **Implement a coherent video scheduler.** LY must reflect elapsed cycles,
   not reads. Track LCD enable, 456 guest cycles per line, line-153 LY wrap
   behavior, mode 2/3/0 transitions, LYC coincidence, STAT IRQ edges,
   VBlank IRQ timing and safe LCD-off/reset behavior. Host frame yielding may
   skip ahead only when equivalent guest-visible events are retained. Verify
   CPU timing and LY/STAT reads in deterministic synthetic ROMs. The
   first-stage host-fast-path LY clock is not a substitute for this work.
5. **Build differential checkpoints.** Run the same GB input ROM against an
   established Game Boy reference runtime and the compact runtime with
   identical input scripts. Compare guest PC/bank, registers/flags,
   WRAM/HRAM, VRAM/OAM, I/O, timer and interrupt state at meaningful
   normalized frame/cycle boundaries. Bisect the first divergence by
   instruction/event checkpoints. Do not compare hardware-dependent host
   renderer internals as if they were guest state.
6. **Validate the Mega Drive output as a game.** Use a headless/automatable
   Mega Drive emulator to boot the *generated* cartridge and assert progress
   through title, input/menu, first moving piece, rotation, landing and
   cleared lines. Where practical compare gameplay state and captured frames
   against the reference under deterministic input; the .bin checksum alone
   is not a gameplay oracle.
7. **Publish a support statement only from evidence.** Track whether
   compilation succeeded, whether the MD cartridge booted, and which
   gameplay milestones passed. Avoid marking an entire commercial game
   compatible solely because synthetic fixture tests pass.

## Relevant implementation locations

- `sgdk_runtime/include/gbrt.h`: guest/host timing and context ABI.
- `sgdk_runtime/src/gbrt_sgdk_min.c`: LY reads, timer/IRQ, OAM-DMA, fallback.
- `src/gbmd_backend.c`: VRAM/OAM and I/O shadows.
- `tests/min_runtime/test_min_runtime.c`: host LY regression.
- `wasm/playground.e2e.mjs`: browser compilation/build smoke, not game boot.
- `site/app.js`: browser diagnostic presentation.

## Design references

SegaGenesisRecomp uses evidence-guided static recompilation, interpretation
for uncovered dynamic execution, and differential state checkpoints. Reuse
these **approaches**, not its 68000 decoder or its Genesis host runtime:
https://github.com/mstan/segagenesisrecomp
https://github.com/mstan/segagenesisrecomp/blob/master/docs/Z80_STATIC_RECOMP.md
https://github.com/mstan/segagenesisrecomp/blob/master/runner/cosim.h

No copyrighted commercial ROM, Tetris assets or derived generated code
belong in this repository.
