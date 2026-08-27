#!/usr/bin/env node
const fs = require('fs');
const path = require('path');

async function main() {
  const [jsPath, wasmPath, sourcePath, outPath] = process.argv.slice(2);
  if (!jsPath || !wasmPath || !sourcePath || !outPath) {
    throw new Error('usage: clang-m68k.test.cjs clang.js clang.wasm input.c output.o');
  }

  const factory = require(path.resolve(jsPath));
  const wasmBinary = fs.readFileSync(wasmPath);
  const stderr = [];
  const stdout = [];
  const mod = await factory({
    wasmBinary,
    noInitialRun: true,
    print: (s) => stdout.push(String(s)),
    printErr: (s) => stderr.push(String(s)),
  });

  mod.FS.writeFile('/input.c', fs.readFileSync(sourcePath));

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
    '/input.c',
    '-o', '/output.o',
  ];

  try {
    const rc = mod.callMain(args);
    if (rc !== undefined && rc !== 0) throw new Error(`clang returned ${rc}`);
  } catch (err) {
    if (!(err && err.name === 'ExitStatus' && err.status === 0)) {
      throw new Error(`${err && err.message ? err.message : err}\n${stderr.join('\n')}`);
    }
  }

  if (!mod.FS.analyzePath('/output.o').exists) {
    throw new Error(`clang did not create output.o:\n${stderr.join('\n')}`);
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

  console.log(`PASS: clang-m68k.wasm compiled C directly to ${obj.length}-byte ELF32 big-endian ET_REL EM_68K object`);
  if (stdout.length) console.log(`clang stdout: ${stdout.join(' | ')}`);
  if (stderr.length) console.log(`clang diagnostics: ${stderr.join(' | ')}`);
}

main().catch((err) => {
  console.error(err && err.stack ? err.stack : err);
  process.exit(1);
});
