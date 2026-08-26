const CARTRIDGE_TYPES = new Map([
  [0x00, ['ROM ONLY', 'none', 'supported']],
  [0x01, ['MBC1', 'MBC1', 'supported']],
  [0x02, ['MBC1+RAM', 'MBC1', 'supported']],
  [0x03, ['MBC1+RAM+BATTERY', 'MBC1', 'supported']],
  [0x05, ['MBC2', 'MBC2', 'unsupported']],
  [0x06, ['MBC2+BATTERY', 'MBC2', 'unsupported']],
  [0x08, ['ROM+RAM', 'none', 'experimental']],
  [0x09, ['ROM+RAM+BATTERY', 'none', 'experimental']],
  [0x0f, ['MBC3+TIMER+BATTERY', 'MBC3', 'supported']],
  [0x10, ['MBC3+TIMER+RAM+BATTERY', 'MBC3', 'supported']],
  [0x11, ['MBC3', 'MBC3', 'supported']],
  [0x12, ['MBC3+RAM', 'MBC3', 'supported']],
  [0x13, ['MBC3+RAM+BATTERY', 'MBC3', 'supported']],
  [0x19, ['MBC5', 'MBC5', 'supported']],
  [0x1a, ['MBC5+RAM', 'MBC5', 'supported']],
  [0x1b, ['MBC5+RAM+BATTERY', 'MBC5', 'supported']],
  [0x1c, ['MBC5+RUMBLE', 'MBC5', 'experimental']],
  [0x1d, ['MBC5+RUMBLE+RAM', 'MBC5', 'experimental']],
  [0x1e, ['MBC5+RUMBLE+RAM+BATTERY', 'MBC5', 'experimental']],
]);

const ROM_SIZES = new Map([
  [0x00, 32 * 1024], [0x01, 64 * 1024], [0x02, 128 * 1024],
  [0x03, 256 * 1024], [0x04, 512 * 1024], [0x05, 1024 * 1024],
  [0x06, 2 * 1024 * 1024], [0x07, 4 * 1024 * 1024], [0x08, 8 * 1024 * 1024],
  [0x52, 72 * 16 * 1024], [0x53, 80 * 16 * 1024], [0x54, 96 * 16 * 1024],
]);

const RAM_SIZES = new Map([
  [0x00, 0], [0x01, 2 * 1024], [0x02, 8 * 1024], [0x03, 32 * 1024],
  [0x04, 128 * 1024], [0x05, 64 * 1024],
]);

const textDecoder = new TextDecoder('ascii');

function formatBytes(bytes) {
  if (!bytes) return '0 B';
  if (bytes >= 1024 * 1024) return `${(bytes / (1024 * 1024)).toFixed(bytes % (1024 * 1024) ? 1 : 0)} MiB`;
  if (bytes >= 1024) return `${(bytes / 1024).toFixed(bytes % 1024 ? 1 : 0)} KiB`;
  return `${bytes} B`;
}

function cleanTitle(bytes) {
  const raw = textDecoder.decode(bytes).replace(/\0.*$/s, '');
  return raw.replace(/[^\x20-\x7e]/g, '').trim() || 'Untitled';
}

function headerChecksum(bytes) {
  let x = 0;
  for (let i = 0x134; i <= 0x14c; i++) x = (x - bytes[i] - 1) & 0xff;
  return x;
}

function globalChecksum(bytes) {
  let sum = 0;
  for (let i = 0; i < bytes.length; i++) {
    if (i === 0x14e || i === 0x14f) continue;
    sum = (sum + bytes[i]) & 0xffff;
  }
  return sum;
}

export function analyzeRom(input) {
  const bytes = input instanceof Uint8Array ? input : new Uint8Array(input);
  if (bytes.length < 0x150) throw new Error('File is too small to contain a valid Game Boy cartridge header.');

  const cgbFlag = bytes[0x143];
  const cartCode = bytes[0x147];
  const romSizeCode = bytes[0x148];
  const ramSizeCode = bytes[0x149];
  const cart = CARTRIDGE_TYPES.get(cartCode) ?? [`Unknown (0x${cartCode.toString(16).padStart(2, '0').toUpperCase()})`, 'unknown', 'unsupported'];
  const declaredRomSize = ROM_SIZES.get(romSizeCode) ?? null;
  const ramSize = RAM_SIZES.get(ramSizeCode) ?? null;
  const isCgb = cgbFlag === 0x80 || cgbFlag === 0xc0;
  const cgbOnly = cgbFlag === 0xc0;
  const expectedHeader = bytes[0x14d];
  const actualHeader = headerChecksum(bytes);
  const expectedGlobal = (bytes[0x14e] << 8) | bytes[0x14f];
  const actualGlobal = globalChecksum(bytes);
  const farRom = bytes.length > 4 * 1024 * 1024;

  const warnings = [];
  if (declaredRomSize !== null && declaredRomSize !== bytes.length) {
    warnings.push(`Header declares ${formatBytes(declaredRomSize)}, but the file is ${formatBytes(bytes.length)}.`);
  }
  if (actualHeader !== expectedHeader) warnings.push('Header checksum does not match.');
  if (actualGlobal !== expectedGlobal) warnings.push('Global checksum does not match.');
  if (isCgb) warnings.push('CGB-specific VRAM banking and palette behavior are not yet fully implemented.');
  if (cart[2] === 'unsupported') warnings.push(`${cart[1]} is not currently implemented by gbc-to-md.`);
  if (cartCode >= 0x1c && cartCode <= 0x1e) warnings.push('MBC5 rumble behavior is not implemented.');
  if ((ramSize ?? 0) > 32 * 1024) warnings.push('Guest cartridge RAM above 32 KiB exceeds the current direct Mega Drive SRAM backend.');
  if (farRom) warnings.push('Retained ROM data above 4 MiB requires the SEGA far-ROM mapper path.');

  let compatibility = cart[2];
  if (compatibility === 'supported' && (isCgb || farRom || warnings.length)) compatibility = 'experimental';
  if (cart[2] === 'unsupported') compatibility = 'unsupported';

  return {
    title: cleanTitle(bytes.slice(0x134, cgbFlag ? 0x143 : 0x144)),
    fileSize: bytes.length,
    fileSizeText: formatBytes(bytes.length),
    declaredRomSize,
    declaredRomSizeText: declaredRomSize === null ? 'Unknown' : formatBytes(declaredRomSize),
    romBanks: Math.ceil(bytes.length / 0x4000),
    ramSize,
    ramSizeText: ramSize === null ? 'Unknown' : formatBytes(ramSize),
    cartridgeCode: cartCode,
    cartridge: cart[0],
    mapper: cart[1],
    cgb: isCgb,
    cgbOnly,
    mode: cgbOnly ? 'Game Boy Color only' : isCgb ? 'Game Boy + Game Boy Color' : 'Game Boy / DMG',
    destinationMapper: farRom ? 'SEGA 512 KiB far-ROM mapper' : 'Direct ROM access',
    headerChecksumValid: actualHeader === expectedHeader,
    globalChecksumValid: actualGlobal === expectedGlobal,
    compatibility,
    warnings,
    browserConversionReady: false,
  };
}

export { formatBytes };
