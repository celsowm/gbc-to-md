import fs from 'node:fs';
import assert from 'node:assert/strict';
import { analyzeRom } from './analyzer.js';

for (const [file, mapper] of [
  ['../fixtures/basicdemo.gb', 'none'],
  ['../fixtures/mbc1test.gb', 'MBC1'],
  ['../fixtures/mbc3test.gb', 'MBC3'],
  ['../fixtures/mbc5test.gb', 'MBC5'],
]) {
  const result = analyzeRom(fs.readFileSync(new URL(file, import.meta.url)));
  assert.equal(result.mapper, mapper, file);
  assert.equal(result.headerChecksumValid, true, file);
}
const mbc5 = analyzeRom(fs.readFileSync(new URL('../fixtures/mbc5test.gb', import.meta.url)));
assert.equal(mbc5.fileSize, 8 * 1024 * 1024);
assert.equal(mbc5.destinationMapper, 'SEGA 512 KiB far-ROM mapper');
console.log('PASS: browser ROM analyzer recognizes ROM-only, MBC1, MBC3, MBC5, checksums, and far-ROM.');
