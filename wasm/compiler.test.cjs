const fs = require('node:fs');
const path = require('node:path');

function decodeBytes(Module, ptr, size) {
  if (!ptr || !size) return '';
  return Buffer.from(Module.HEAPU8.subarray(ptr, ptr + size)).toString('utf8');
}

function collectFiles(Module) {
  const files = new Map();
  const count = Module._gbrecomp_wasm_file_count();
  for (let i = 0; i < count; i++) {
    const name = decodeBytes(Module, Module._gbrecomp_wasm_file_name_ptr(i), Module._gbrecomp_wasm_file_name_size(i));
    const content = decodeBytes(Module, Module._gbrecomp_wasm_file_data_ptr(i), Module._gbrecomp_wasm_file_data_size(i));
    files.set(name, content);
  }
  return files;
}

async function main() {
  const [modulePath, romPath] = process.argv.slice(2);
  if (!modulePath || !romPath) throw new Error('usage: node compiler.test.cjs <module.js> <rom>');

  const resolvedModulePath = path.resolve(modulePath);
  const wasmPath = path.join(path.dirname(resolvedModulePath), 'gbrecomp_probe.wasm');
  const createModule = require(resolvedModulePath);
  const Module = await createModule({ wasmBinary: fs.readFileSync(wasmPath) });
  const rom = fs.readFileSync(romPath);
  const ptr = Module._malloc(rom.length);

  try {
    Module.HEAPU8.set(rom, ptr);
    const result = Module._gbrecomp_wasm_compile(ptr, rom.length);
    if (result < 0) {
      const error = decodeBytes(Module, Module._gbrecomp_wasm_error_ptr(), Module._gbrecomp_wasm_error_size());
      throw new Error(`compile failed ${result}: ${error}`);
    }

    let files = collectFiles(Module);
    if (files.size !== result || files.size < 4) throw new Error(`unexpected generated file count ${files.size}`);

    for (const name of ['browserrom.h', 'browserrom.c', 'browserrom_internal.h']) {
      if (!files.has(name)) throw new Error(`missing generated file ${name}; got ${[...files.keys()].join(', ')}`);
    }
    if (![...files.keys()].some(name => /^browserrom_funcs_\d+\.c$/.test(name))) throw new Error('missing generated function chunk');
    if (![...files.keys()].some(name => /^browserrom_dispatch_chunk_\d+\.c$/.test(name))) throw new Error('missing generated dispatch chunk');
    if (!files.get('browserrom.c').includes('void browserrom_init')) throw new Error('browserrom.c does not contain browserrom_init');

    const generatedBytes = [...files.values()].reduce((sum, value) => sum + Buffer.byteLength(value), 0);
    console.log(`PASS: WASM generated ${files.size} C/header files from ${romPath} (${generatedBytes} text bytes)`);

    const preparedCount = Module._gbrecomp_wasm_prepare_sgdk(rom.length);
    if (preparedCount < 0) {
      const error = decodeBytes(Module, Module._gbrecomp_wasm_error_ptr(), Module._gbrecomp_wasm_error_size());
      throw new Error(`SGDK preparation failed ${preparedCount}: ${error}`);
    }

    files = collectFiles(Module);
    if (files.has('browserrom_rom.c')) throw new Error('C ROM initializer survived SGDK preparation');
    if (files.has('browserrom_main.c')) throw new Error('desktop main wrapper survived SGDK preparation');
    if (!files.has('browserrom_rom.s')) throw new Error('SGDK ROM blob assembly is missing');

    const mainSource = files.get('browserrom.c');
    for (const token of ['fprintf', 'stderr', 'exit(']) {
      if (mainSource.includes(token)) throw new Error(`desktop token survived SGDK preparation: ${token}`);
    }
    if (mainSource.includes('rom_data[0x147]')) throw new Error('direct ROM header access survived SGDK preparation');
    if (!mainSource.includes('gbrt_sgdk_rom_read8(ctx, 0x147u)')) throw new Error('SGDK ROM read rewrite is missing');

    const romAsm = files.get('browserrom_rom.s');
    for (const token of ['.section .rodata_binf,"a"', `.long ${rom.length}`, '.incbin "browserrom.gb"']) {
      if (!romAsm.includes(token)) throw new Error(`ROM assembly is missing ${token}`);
    }

    console.log(`PASS: WASM prepared ${files.size} SGDK source artifacts in memory`);
    console.log([...files.keys()].sort().join('\n'));
  } finally {
    Module._free(ptr);
  }
}

main().catch(error => { console.error(error); process.exit(1); });
