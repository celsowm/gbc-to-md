#!/usr/bin/env node
import fs from 'node:fs';
import path from 'node:path';
import { buildGenesisC, finalizeGenesisRom } from 'romdev-toolchain-m68k-gcc';

const outPath = process.argv[2] || 'build/romdev-wasm-smoke/rom.bin';
const source = `
#include <genesis.h>
int main(void) {
    VDP_drawText("gbc-to-md wasm", 8, 12);
    while (1) {
        SYS_doVBlankProcess();
    }
    return 0;
}
`;

const result = await buildGenesisC({ source, sgdk: true });
if (!result?.ok || !(result.binary instanceof Uint8Array)) {
  const stage = result?.stage ? ` stage=${result.stage}` : '';
  const log = result?.log ? `\n${result.log}` : '';
  throw new Error(`WASM Genesis build failed${stage}${log}`);
}

const rom = finalizeGenesisRom(result.binary);
if (!(rom instanceof Uint8Array) || rom.length < 0x200) {
  throw new Error(`invalid finalized ROM length ${rom?.length ?? 'null'}`);
}

const ascii = Buffer.from(rom.subarray(0x100, 0x110)).toString('ascii');
if (!ascii.startsWith('SEGA')) {
  throw new Error(`missing SEGA cartridge header at 0x100: ${JSON.stringify(ascii)}`);
}

let checksum = 0;
for (let i = 0x200; i + 1 < rom.length; i += 2) {
  checksum = (checksum + ((rom[i] << 8) | rom[i + 1])) & 0xffff;
}
const stored = (rom[0x18e] << 8) | rom[0x18f];
if (checksum !== stored) {
  throw new Error(`Genesis checksum mismatch stored=0x${stored.toString(16)} calculated=0x${checksum.toString(16)}`);
}

fs.mkdirSync(path.dirname(outPath), { recursive: true });
fs.writeFileSync(outPath, rom);
console.log(`PASS: WebAssembly-only SGDK/GCC/binutils pipeline emitted ${rom.length}-byte Genesis ROM`);
console.log(`PASS: header=${JSON.stringify(ascii)} checksum=0x${stored.toString(16).padStart(4, '0')}`);
console.log(outPath);
