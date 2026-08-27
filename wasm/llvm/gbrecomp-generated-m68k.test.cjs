#!/usr/bin/env node
const fs = require('fs');
const path = require('path');

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

function assertM68kElf(obj, label) {
  if (obj.length < 52 || obj.slice(0, 4).toString('hex') !== '7f454c46') {
    throw new Error(`${label}: output is not ELF`);
  }
  if (obj[4] !== 1) throw new Error(`${label}: expected ELF32, EI_CLASS=${obj[4]}`);
  if (obj[5] !== 2) throw new Error(`${label}: expected big-endian ELF, EI_DATA=${obj[5]}`);
  const type = obj.readUInt16BE(16);
  const machine = obj.readUInt16BE(18);
  if (type !== 1) throw new Error(`${label}: expected ET_REL=1, got ${type}`);
  if (machine !== 4) throw new Error(`${label}: expected EM_68K=4, got ${machine}`);
}

async function loadGBRecomp(jsPath, wasmPath) {
  const factory = require(path.resolve(jsPath));
  return factory({ wasmBinary: fs.readFileSync(wasmPath) });
}

async function generateSources(jsPath, wasmPath, romPath) {
  const mod = await loadGBRecomp(jsPath, wasmPath);
  const rom = fs.readFileSync(romPath);
  const ptr = mod._malloc(rom.length);
  try {
    mod.HEAPU8.set(rom, ptr);
    const rc = mod._gbrecomp_wasm_compile(ptr, rom.length);
    if (rc < 0) {
      const message = decodeBytes(mod, mod._gbrecomp_wasm_error_ptr(), mod._gbrecomp_wasm_error_size());
      throw new Error(`gbrecomp compile failed ${rc}: ${message}`);
    }
    const prepared = mod._gbrecomp_wasm_prepare_sgdk(rom.length);
    if (prepared < 0) {
      const message = decodeBytes(mod, mod._gbrecomp_wasm_error_ptr(), mod._gbrecomp_wasm_error_size());
      throw new Error(`SGDK preparation failed ${prepared}: ${message}`);
    }
    return { files: collectGenerated(mod), rom };
  } finally {
    mod._free(ptr);
  }
}

function loadSupportHeaders(root) {
  const files = new Map();
  for (const name of ['stdint.h', 'stddef.h', 'stdbool.h']) {
    files.set(name, fs.readFileSync(path.join(root, 'wasm/sysroot/include', name)));
  }
  files.set('gbrt.h', fs.readFileSync(path.join(root, 'sgdk_runtime/include/gbrt.h')));
  files.set('gbmd_backend.h', fs.readFileSync(path.join(root, 'src/gbmd_backend.h')));
  return files;
}

async function compileOne(factory, wasmBinary, generated, support, sourceName) {
  const stderr = [];
  const mod = await factory({
    wasmBinary,
    noInitialRun: true,
    noExitRuntime: true,
    print: () => {},
    printErr: (s) => stderr.push(String(s)),
  });

  mod.FS.mkdir('/work');
  mod.FS.mkdir('/include');
  for (const [name, content] of generated) {
    mod.FS.writeFile(`/work/${name}`, Buffer.from(content));
  }
  for (const [name, content] of support) {
    mod.FS.writeFile(`/include/${name}`, content);
  }

  const output = `/work/${sourceName.replace(/\.c$/, '.o')}`;
  const args = [
    '-cc1',
    '-triple', 'm68k-unknown-elf',
    '-emit-obj',
    '-x', 'c',
    '-std=c11',
    '-ffreestanding',
    '-fno-builtin',
    '-O2',
    '-target-cpu', 'M68000',
    '-DGBRT_SGDK_USE_CART_SRAM=1',
    '-I', '/work',
    '-I', '/include',
    `/work/${sourceName}`,
    '-o', output,
  ];

  try {
    const rc = mod.callMain(args);
    if (rc !== undefined && rc !== 0) throw new Error(`clang returned ${rc}`);
  } catch (err) {
    if (!(err && err.name === 'ExitStatus' && err.status === 0)) {
      throw new Error(`${sourceName}: ${err && err.message ? err.message : err}\n${stderr.join('\n')}`);
    }
  }

  if (!mod.FS.analyzePath(output).exists) {
    throw new Error(`${sourceName}: clang produced no object\n${stderr.join('\n')}`);
  }
  const obj = Buffer.from(mod.FS.readFile(output));
  assertM68kElf(obj, sourceName);
  return { obj, diagnostics: stderr };
}

async function main() {
  const [gbJs, gbWasm, clangJs, clangWasm, romPath, outDir] = process.argv.slice(2);
  if (![gbJs, gbWasm, clangJs, clangWasm, romPath, outDir].every(Boolean)) {
    throw new Error('usage: gbrecomp-generated-m68k.test.cjs gbrecomp.js gbrecomp.wasm clang.js clang.wasm rom.gb out-dir');
  }

  const root = path.resolve(__dirname, '../..');
  const { files } = await generateSources(gbJs, gbWasm, romPath);
  const cFiles = [...files.keys()].filter((name) => name.endsWith('.c')).sort();
  if (!cFiles.length) throw new Error('GB Recompiled produced no C translation units');

  const clangFactory = require(path.resolve(clangJs));
  const clangBinary = fs.readFileSync(clangWasm);
  const support = loadSupportHeaders(root);
  fs.mkdirSync(outDir, { recursive: true });

  let total = 0;
  for (const sourceName of cFiles) {
    const { obj, diagnostics } = await compileOne(clangFactory, clangBinary, files, support, sourceName);
    const hostPath = path.join(outDir, sourceName.replace(/\.c$/, '.o'));
    fs.writeFileSync(hostPath, obj);
    total += obj.length;
    console.log(`PASS: ${sourceName} -> ${path.basename(hostPath)} (${obj.length} bytes, EM_68K)`);
    if (diagnostics.length) console.log(`  diagnostics: ${diagnostics.join(' | ')}`);
  }

  console.log(`PASS: browser pipeline compiled ${cFiles.length} GB Recompiled translation units to ${total} bytes of M68k ELF objects`);
}

main().catch((err) => {
  console.error(err && err.stack ? err.stack : err);
  process.exit(1);
});
