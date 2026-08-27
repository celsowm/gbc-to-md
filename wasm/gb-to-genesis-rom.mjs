#!/usr/bin/env node
import fs from 'node:fs';
import path from 'node:path';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import { buildGenesisC, finalizeGenesisRom } from 'romdev-toolchain-m68k-gcc';

const require = createRequire(import.meta.url);
const __dirname = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(__dirname, '..');

function decodeBytes(mod, ptr, size) {
  if (!ptr || !size) return '';
  return Buffer.from(mod.HEAPU8.subarray(ptr, ptr + size)).toString('utf8');
}

function collectGenerated(mod) {
  const files = new Map();
  const count = mod._gbrecomp_wasm_file_count();
  for (let i = 0; i < count; i += 1) {
    const name = decodeBytes(mod, mod._gbrecomp_wasm_file_name_ptr(i), mod._gbrecomp_wasm_file_name_size(i));
    const content = decodeBytes(mod, mod._gbrecomp_wasm_file_data_ptr(i), mod._gbrecomp_wasm_file_data_size(i));
    files.set(name, content);
  }
  return files;
}

function lastError(mod) {
  return decodeBytes(mod, mod._gbrecomp_wasm_error_ptr(), mod._gbrecomp_wasm_error_size()) || 'unknown GB Recompiled error';
}

function findAnnotations(romPath) {
  const parsed = path.parse(romPath);
  const candidates = [
    path.join(parsed.dir, `${parsed.name}.annotations`),
    `${romPath}.annotations`,
  ];
  for (const candidate of candidates) {
    if (fs.existsSync(candidate)) {
      return { path: candidate, text: fs.readFileSync(candidate, 'utf8') };
    }
  }
  return null;
}

async function recompile(gbrecompJs, gbrecompWasm, rom, annotationsText = '') {
  const factory = require(path.resolve(gbrecompJs));
  const mod = await factory({
    wasmBinary: fs.readFileSync(gbrecompWasm),
    print: () => {},
    printErr: (s) => process.stderr.write(`[gbrecomp] ${s}\n`),
  });
  const romPtr = mod._malloc(rom.length);
  const annotationsBytes = annotationsText ? Buffer.from(annotationsText, 'utf8') : null;
  const annotationsPtr = annotationsBytes?.length ? mod._malloc(annotationsBytes.length) : 0;
  try {
    mod.HEAPU8.set(rom, romPtr);
    let compiled;
    if (annotationsBytes?.length) {
      if (typeof mod._gbrecomp_wasm_compile_annotated !== 'function') {
        throw new Error('GB Recompiled WebAssembly module does not expose annotated compilation');
      }
      mod.HEAPU8.set(annotationsBytes, annotationsPtr);
      compiled = mod._gbrecomp_wasm_compile_annotated(
        romPtr,
        rom.length,
        annotationsPtr,
        annotationsBytes.length,
      );
    } else {
      compiled = mod._gbrecomp_wasm_compile(romPtr, rom.length);
    }
    if (compiled < 0) throw new Error(`GB Recompiled failed (${compiled}): ${lastError(mod)}`);
    const prepared = mod._gbrecomp_wasm_prepare_sgdk(rom.length);
    if (prepared < 0) throw new Error(`SGDK preparation failed (${prepared}): ${lastError(mod)}`);
    const files = collectGenerated(mod);
    if (!files.has('browserrom.c') || !files.has('browserrom.h') || !files.has('browserrom_rom.s')) {
      throw new Error(`incomplete SGDK bundle: ${[...files.keys()].sort().join(', ')}`);
    }
    return files;
  } finally {
    if (annotationsPtr) mod._free(annotationsPtr);
    mod._free(romPtr);
  }
}

function makeBuildInputs(generated, rom) {
  const sources = {};
  const headers = {};

  for (const [name, content] of generated) {
    if (/\.(c|s|asm)$/i.test(name)) sources[name] = content;
    else if (/\.h$/i.test(name)) headers[name] = content;
  }

  sources['gbrt_sgdk_min.c'] = fs.readFileSync(path.join(root, 'sgdk_runtime/src/gbrt_sgdk_min.c'), 'utf8');
  sources['gbmd_backend.c'] = fs.readFileSync(path.join(root, 'src/gbmd_backend.c'), 'utf8');
  sources['main.c'] = fs.readFileSync(path.join(root, 'sgdk/template/src/main.c'), 'utf8')
    .replaceAll('cakegame', 'browserrom');

  headers['gbrt.h'] = fs.readFileSync(path.join(root, 'sgdk_runtime/include/gbrt.h'), 'utf8');
  headers['gbmd_backend.h'] = fs.readFileSync(path.join(root, 'src/gbmd_backend.h'), 'utf8');

  return {
    sources,
    headers,
    binaryIncludes: { 'browserrom.gb': new Uint8Array(rom) },
  };
}

function validateGenesisRom(rom) {
  if (!(rom instanceof Uint8Array) || rom.length < 0x200) throw new Error(`invalid Genesis ROM length ${rom?.length ?? 'null'}`);
  const system = Buffer.from(rom.subarray(0x100, 0x110)).toString('ascii');
  if (!system.startsWith('SEGA')) throw new Error(`missing SEGA header at 0x100: ${JSON.stringify(system)}`);

  let checksum = 0;
  for (let i = 0x200; i + 1 < rom.length; i += 2) checksum = (checksum + ((rom[i] << 8) | rom[i + 1])) & 0xffff;
  const stored = (rom[0x18e] << 8) | rom[0x18f];
  if (checksum !== stored) {
    throw new Error(`checksum mismatch stored=0x${stored.toString(16)} calculated=0x${checksum.toString(16)}`);
  }
  return { system, checksum };
}

async function main() {
  const [gbrecompJs, gbrecompWasm, romPath, outPath = 'build/browser-e2e/rom.bin'] = process.argv.slice(2);
  if (!gbrecompJs || !gbrecompWasm || !romPath) {
    throw new Error('usage: gb-to-genesis-rom.mjs gbrecomp.js gbrecomp.wasm input.gb output.bin');
  }

  const rom = fs.readFileSync(romPath);
  const annotations = findAnnotations(romPath);
  const started = performance.now();
  const generated = await recompile(gbrecompJs, gbrecompWasm, rom, annotations?.text || '');
  const recompileDone = performance.now();
  const inputs = makeBuildInputs(generated, rom);
  const cc1Options = ['-O2', '-DGBRT_SGDK_USE_CART_SRAM'];
  if (rom.length > 0x400000) cc1Options.push('-DGBRT_SGDK_USE_FAR_ROM');

  console.log(`GB Recompiled WASM: ${generated.size} prepared artifacts`);
  console.log(`Annotations: ${annotations ? annotations.path : 'none'}`);
  console.log(`Genesis inputs: ${Object.keys(inputs.sources).length} sources, ${Object.keys(inputs.headers).length} headers, ${rom.length} ROM bytes`);
  console.log(`far-ROM accessor: ${rom.length > 0x400000 ? 'enabled' : 'not required'}`);

  const result = await buildGenesisC({
    ...inputs,
    sgdk: true,
    cc1Options,
  });
  if (!result?.ok || !(result.binary instanceof Uint8Array)) {
    const stage = result?.stage ? ` stage=${result.stage}` : '';
    throw new Error(`Genesis WASM build failed${stage}\n${result?.log || ''}`);
  }

  const finalRom = finalizeGenesisRom(result.binary);
  const validation = validateGenesisRom(finalRom);
  fs.mkdirSync(path.dirname(outPath), { recursive: true });
  fs.writeFileSync(outPath, finalRom);

  const finished = performance.now();
  console.log('PASS: GB/GBC -> gbrecomp.wasm -> m68k GCC/binutils WASM -> Genesis ROM');
  console.log(`PASS: ${finalRom.length} bytes; header=${JSON.stringify(validation.system)}; checksum=0x${validation.checksum.toString(16).padStart(4, '0')}`);
  console.log(`Timing: recompile ${(recompileDone - started).toFixed(0)} ms; m68k+link ${(finished - recompileDone).toFixed(0)} ms; total ${(finished - started).toFixed(0)} ms`);
  console.log(outPath);
}

main().catch((err) => {
  console.error(err && err.stack ? err.stack : err);
  process.exit(1);
});
