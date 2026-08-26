const fs = require('node:fs');
const path = require('node:path');

async function main() {
  const [modulePath, romPath, expectedBanks, expectedMbc] = process.argv.slice(2);
  if (!modulePath || !romPath || !expectedBanks || !expectedMbc) {
    throw new Error('usage: node probe.test.cjs <module.js> <rom> <banks> <mbc-code>');
  }

  const resolvedModulePath = path.resolve(modulePath);
  const wasmPath = path.join(path.dirname(resolvedModulePath), 'gbrecomp_probe.wasm');
  const createModule = require(resolvedModulePath);
  const Module = await createModule({
    wasmBinary: fs.readFileSync(wasmPath),
    locateFile(file) { return path.join(path.dirname(resolvedModulePath), file); },
  });

  const rom = fs.readFileSync(romPath);
  const ptr = Module._malloc(rom.length);
  try {
    Module.HEAPU8.set(rom, ptr);
    const result = Module._gbrecomp_wasm_probe(ptr, rom.length);
    if (result < 0) throw new Error(`probe rejected ROM: ${result}`);
    const banks = result >>> 8;
    const mbc = result & 0xff;
    if (banks !== Number(expectedBanks) || mbc !== Number(expectedMbc)) {
      throw new Error(`unexpected probe result banks=${banks} mbc=0x${mbc.toString(16)}`);
    }
    console.log(`PASS: WASM gbrecomp_core parsed ${romPath}: banks=${banks} mbc=0x${mbc.toString(16)}`);
  } finally {
    Module._free(ptr);
  }
}

main().catch(error => { console.error(error); process.exit(1); });
