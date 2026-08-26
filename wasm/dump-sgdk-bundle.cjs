const fs = require('node:fs');
const path = require('node:path');

function decodeBytes(Module, ptr, size) {
  if (!ptr || !size) return '';
  return Buffer.from(Module.HEAPU8.subarray(ptr, ptr + size)).toString('utf8');
}

function collectFiles(Module) {
  const files = [];
  const count = Module._gbrecomp_wasm_file_count();
  for (let i = 0; i < count; i++) {
    const name = decodeBytes(Module, Module._gbrecomp_wasm_file_name_ptr(i), Module._gbrecomp_wasm_file_name_size(i));
    const content = decodeBytes(Module, Module._gbrecomp_wasm_file_data_ptr(i), Module._gbrecomp_wasm_file_data_size(i));
    files.push({ name, content });
  }
  return files;
}

async function main() {
  const [modulePath, romPath, outputDir] = process.argv.slice(2);
  if (!modulePath || !romPath || !outputDir) {
    throw new Error('usage: node dump-sgdk-bundle.cjs <module.js> <rom.gb> <stage-dir>');
  }

  const resolvedModulePath = path.resolve(modulePath);
  const createModule = require(resolvedModulePath);
  const Module = await createModule({
    wasmBinary: fs.readFileSync(path.join(path.dirname(resolvedModulePath), 'gbrecomp_probe.wasm')),
  });

  const rom = fs.readFileSync(romPath);
  const ptr = Module._malloc(rom.length);
  try {
    Module.HEAPU8.set(rom, ptr);
    const compiled = Module._gbrecomp_wasm_compile(ptr, rom.length);
    if (compiled < 0) throw new Error(`WASM compile failed: ${compiled}`);
    const prepared = Module._gbrecomp_wasm_prepare_sgdk(rom.length);
    if (prepared < 0) throw new Error(`WASM SGDK preparation failed: ${prepared}`);

    const root = path.resolve(outputDir);
    const src = path.join(root, 'src');
    fs.mkdirSync(src, { recursive: true });
    for (const file of collectFiles(Module)) {
      fs.writeFileSync(path.join(src, file.name), file.content);
    }
    fs.writeFileSync(path.join(root, 'browserrom.gb'), rom);
    console.log(`WASM bundle materialized: ${prepared} generated files + ${rom.length} ROM bytes`);
  } finally {
    Module._free(ptr);
  }
}

main().catch(error => { console.error(error); process.exit(1); });
