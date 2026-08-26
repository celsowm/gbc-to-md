const fs = require('node:fs');
const path = require('node:path');

function decodeBytes(Module, ptr, size) {
  if (!ptr || !size) return '';
  return Buffer.from(Module.HEAPU8.subarray(ptr, ptr + size)).toString('utf8');
}

async function main() {
  const [modulePath, romPath] = process.argv.slice(2);
  if (!modulePath || !romPath) {
    throw new Error('usage: node compiler.test.cjs <module.js> <rom>');
  }

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
      const error = decodeBytes(
        Module,
        Module._gbrecomp_wasm_error_ptr(),
        Module._gbrecomp_wasm_error_size(),
      );
      throw new Error(`compile failed ${result}: ${error}`);
    }

    const count = Module._gbrecomp_wasm_file_count();
    if (count !== result || count < 4) throw new Error(`unexpected generated file count ${count}`);

    const files = new Map();
    for (let i = 0; i < count; i++) {
      const name = decodeBytes(
        Module,
        Module._gbrecomp_wasm_file_name_ptr(i),
        Module._gbrecomp_wasm_file_name_size(i),
      );
      const content = decodeBytes(
        Module,
        Module._gbrecomp_wasm_file_data_ptr(i),
        Module._gbrecomp_wasm_file_data_size(i),
      );
      files.set(name, content);
    }

    const required = [
      'browserrom.h',
      'browserrom.c',
      'browserrom_internal.h',
    ];
    for (const name of required) {
      if (!files.has(name)) throw new Error(`missing generated file ${name}; got ${[...files.keys()].join(', ')}`);
    }
    if (![...files.keys()].some(name => /^browserrom_funcs_\d+\.c$/.test(name))) {
      throw new Error('missing generated function chunk');
    }
    if (![...files.keys()].some(name => /^browserrom_dispatch_chunk_\d+\.c$/.test(name))) {
      throw new Error('missing generated dispatch chunk');
    }
    if (!files.get('browserrom.c').includes('void browserrom_init')) {
      throw new Error('browserrom.c does not contain browserrom_init');
    }

    const total = [...files.values()].reduce((sum, value) => sum + Buffer.byteLength(value), 0);
    console.log(`PASS: WASM generated ${count} C/header files from ${romPath} (${total} text bytes)`);
    console.log([...files.keys()].sort().join('\n'));
  } finally {
    Module._free(ptr);
  }
}

main().catch(error => { console.error(error); process.exit(1); });
