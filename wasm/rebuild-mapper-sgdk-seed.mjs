#!/usr/bin/env node
import { buildGenesisC } from 'romdev-toolchain-m68k-gcc';

const source = `
#include <genesis.h>
int main(bool hard) {
    (void)hard;
    while (TRUE) SYS_doVBlankProcess();
    return 0;
}
`;

const started = performance.now();
const result = await buildGenesisC({
  source,
  sgdk: true,
  rebuildSdk: true,
  writeSeed: true,
  cc1Options: ['-O2'],
});

if (!result?.ok) {
  throw new Error(`mapper-enabled SGDK seed rebuild failed at ${result?.stage || 'unknown'}\n${result?.log || ''}`);
}
console.log(`PASS: rebuilt and persisted mapper-enabled SGDK seed in ${((performance.now() - started) / 1000).toFixed(2)} s`);
console.log(result.log || '');
