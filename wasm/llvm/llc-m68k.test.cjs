#!/usr/bin/env node
const fs = require('fs');
const path = require('path');

async function main() {
  const [jsPath, wasmPath, irPath, outPath] = process.argv.slice(2);
  if (!jsPath || !wasmPath || !irPath || !outPath) {
    throw new Error('usage: llc-m68k.test.cjs llc.js llc.wasm input.ll output.o');
  }

  const factory = require(path.resolve(jsPath));
  const wasmBinary = fs.readFileSync(wasmPath);
  const stderr = [];
  const mod = await factory({
    wasmBinary,
    noInitialRun: true,
    print: (s) => process.stdout.write(String(s) + '\n'),
    printErr: (s) => stderr.push(String(s)),
  });

  mod.FS.writeFile('/input.ll', fs.readFileSync(irPath));
  try {
    const rc = mod.callMain([
      '-mtriple=m68k-unknown-elf',
      '-mcpu=M68000',
      '-filetype=obj',
      '/input.ll',
      '-o',
      '/output.o',
    ]);
    if (rc !== undefined && rc !== 0) throw new Error(`llc returned ${rc}`);
  } catch (err) {
    if (!(err && err.name === 'ExitStatus' && err.status === 0)) throw err;
  }

  if (!mod.FS.analyzePath('/output.o').exists) {
    throw new Error(`llc did not create output.o: ${stderr.join('\n')}`);
  }
  const obj = Buffer.from(mod.FS.readFile('/output.o'));
  fs.writeFileSync(outPath, obj);

  if (obj.length < 52 || obj.slice(0, 4).toString('hex') !== '7f454c46') {
    throw new Error('output is not ELF');
  }
  if (obj[4] !== 1) throw new Error(`expected ELF32, EI_CLASS=${obj[4]}`);
  if (obj[5] !== 2) throw new Error(`expected big-endian ELF, EI_DATA=${obj[5]}`);
  const type = obj.readUInt16BE(16);
  const machine = obj.readUInt16BE(18);
  if (type !== 1) throw new Error(`expected ET_REL=1, got ${type}`);
  if (machine !== 4) throw new Error(`expected EM_68K=4, got ${machine}`);

  console.log(`PASS: llc-m68k.wasm emitted ${obj.length}-byte ELF32 big-endian ET_REL EM_68K object for M68000`);
  if (stderr.length) console.log(`llc diagnostics: ${stderr.join(' | ')}`);
}

main().catch((err) => {
  console.error(err && err.stack ? err.stack : err);
  process.exit(1);
});
