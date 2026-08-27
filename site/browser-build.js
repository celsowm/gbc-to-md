let toolchainPromise = null;
let shareIndexPromise = null;
const fetchCache = new Map();
const sdkCache = new Map();
let nextJobId = 1;
let toolWorker = null;
const pendingToolJobs = new Map();

async function loadToolchain() {
  if (!toolchainPromise) {
    toolchainPromise = import('./toolchain/build/genesis-c/genesis-c.js');
  }
  return toolchainPromise;
}

async function loadShareIndex() {
  if (!shareIndexPromise) {
    shareIndexPromise = fetch('./toolchain/share-manifest.json').then(async (response) => {
      if (!response.ok) throw new Error(`Failed to load toolchain share manifest: HTTP ${response.status}`);
      return response.json();
    });
  }
  return shareIndexPromise;
}

async function fetchBytes(url) {
  if (!fetchCache.has(url)) {
    fetchCache.set(url, fetch(url).then(async (response) => {
      if (!response.ok) throw new Error(`Failed to load ${url}: HTTP ${response.status}`);
      return new Uint8Array(await response.arrayBuffer());
    }));
  }
  return new Uint8Array(await fetchCache.get(url));
}

async function fetchText(url) {
  return new TextDecoder().decode(await fetchBytes(url));
}

function makeShare() {
  return {
    async list(prefix) {
      const entries = await loadShareIndex();
      const normalized = prefix.replace(/^\/+|\/+$/g, '');
      const start = normalized ? `${normalized}/` : '';
      return entries.filter((name) => name.startsWith(start));
    },
    async bytes(relPath) {
      return fetchBytes(`./toolchain/share/${relPath}`);
    },
    async text(relPath) {
      return fetchText(`./toolchain/share/${relPath}`);
    },
  };
}

function concatBytes(chunks) {
  const size = chunks.reduce((sum, chunk) => sum + chunk.length, 0);
  const out = new Uint8Array(size);
  let offset = 0;
  for (const chunk of chunks) {
    out.set(chunk, offset);
    offset += chunk.length;
  }
  return out;
}

async function hashSources(sourceMap) {
  const encoder = new TextEncoder();
  const nul = new Uint8Array([0]);
  const chunks = [];
  for (const name of Object.keys(sourceMap).sort()) {
    const value = sourceMap[name];
    const bytes = typeof value === 'string' ? encoder.encode(value) : new Uint8Array(value);
    chunks.push(encoder.encode(name), nul, bytes, nul);
  }
  const digest = new Uint8Array(await crypto.subtle.digest('SHA-256', concatBytes(chunks)));
  return [...digest].map((byte) => byte.toString(16).padStart(2, '0')).join('');
}

function rejectPendingToolJobs(error) {
  for (const { reject, timer } of pendingToolJobs.values()) {
    clearTimeout(timer);
    reject(error);
  }
  pendingToolJobs.clear();
}

function resetToolWorker(error = null) {
  if (toolWorker) toolWorker.terminate();
  toolWorker = null;
  if (error) rejectPendingToolJobs(error);
}

function getToolWorker() {
  if (toolWorker) return toolWorker;

  const worker = new Worker(new URL('./toolchain-worker.js', import.meta.url), { type: 'module' });
  worker.onmessage = (event) => {
    const id = event.data?.id;
    if (!pendingToolJobs.has(id)) return;
    const pending = pendingToolJobs.get(id);
    pendingToolJobs.delete(id);
    clearTimeout(pending.timer);
    if (event.data.error) pending.reject(new Error(event.data.error));
    else pending.resolve(event.data.result);
  };
  worker.onerror = (event) => {
    const error = new Error(event.message || 'Browser toolchain worker failed');
    if (toolWorker === worker) resetToolWorker(error);
  };
  toolWorker = worker;
  return worker;
}

function runTool(job) {
  return new Promise((resolve, reject) => {
    const id = nextJobId++;
    const worker = getToolWorker();
    const timer = setTimeout(() => {
      pendingToolJobs.delete(id);
      reject(new Error(`Timed out running browser tool ${job.tool}`));
      resetToolWorker();
    }, 120000);
    pendingToolJobs.set(id, { resolve, reject, timer });
    worker.postMessage({ id, job });
  });
}

async function loadGlue(file) {
  return (await import(new URL(`./toolchain/wasm/${file}`, import.meta.url).href)).default;
}

async function loadRuntimeBundle() {
  const [gbrtC, gbrtH, backendC, backendH, mainTemplate] = await Promise.all([
    fetchText('./runtime/gbrt_sgdk_min.c'),
    fetchText('./runtime/gbrt.h'),
    fetchText('./runtime/gbmd_backend.c'),
    fetchText('./runtime/gbmd_backend.h'),
    fetchText('./runtime/main.c'),
  ]);
  return { gbrtC, gbrtH, backendC, backendH, mainTemplate };
}

function validateGenesisRom(rom) {
  if (!(rom instanceof Uint8Array) || rom.length < 0x200) throw new Error('Generated cartridge image is too small');
  const system = new TextDecoder().decode(rom.subarray(0x100, 0x110));
  if (!system.startsWith('SEGA')) throw new Error(`Generated image has no SEGA header: ${JSON.stringify(system)}`);
  let checksum = 0;
  for (let i = 0x200; i + 1 < rom.length; i += 2) {
    checksum = (checksum + ((rom[i] << 8) | rom[i + 1])) & 0xffff;
  }
  const stored = (rom[0x18e] << 8) | rom[0x18f];
  if (checksum !== stored) throw new Error(`Genesis checksum mismatch: stored ${stored.toString(16)}, calculated ${checksum.toString(16)}`);
  return { system, checksum };
}

export async function buildMegaDriveRom({ romBytes, generatedFiles, onProgress = () => {} }) {
  if (!(romBytes instanceof Uint8Array)) throw new TypeError('romBytes must be Uint8Array');
  if (!Array.isArray(generatedFiles) || generatedFiles.length === 0) throw new Error('Recompile the Game Boy ROM before building the Mega Drive ROM');

  onProgress('Loading browser m68k GCC/binutils and SGDK assets…');
  const [{ buildGenesisC, finalizeGenesisRom }, runtime] = await Promise.all([
    loadToolchain(),
    loadRuntimeBundle(),
    loadShareIndex(),
  ]);

  const sources = {};
  const headers = {};
  for (const file of generatedFiles) {
    if (/\.(c|s|asm)$/i.test(file.name)) sources[file.name] = file.content;
    else if (/\.h$/i.test(file.name)) headers[file.name] = file.content;
  }
  sources['gbrt_sgdk_min.c'] = runtime.gbrtC;
  sources['gbmd_backend.c'] = runtime.backendC;
  sources['main.c'] = runtime.mainTemplate.replaceAll('cakegame', 'browserrom');
  headers['gbrt.h'] = runtime.gbrtH;
  headers['gbmd_backend.h'] = runtime.backendH;

  const env = {
    runTool,
    loadGlue,
    share: makeShare(),
    hash: hashSources,
    sdkCache: {
      get: (key) => sdkCache.get(key) || null,
      put: (key, bytes) => sdkCache.set(key, new Uint8Array(bytes)),
    },
  };

  const cc1Options = ['-O2', '-DGBRT_SGDK_USE_CART_SRAM'];
  if (romBytes.length > 0x400000) cc1Options.push('-DGBRT_SGDK_USE_FAR_ROM');

  onProgress(`Compiling ${Object.keys(sources).length} source files for Motorola 68000${romBytes.length > 0x400000 ? ' with SEGA far-ROM mapper' : ''}…`);
  const started = performance.now();
  const result = await buildGenesisC({
    sources,
    headers,
    binaryIncludes: { 'browserrom.gb': romBytes },
    sgdk: true,
    cc1Options,
    env,
  });

  if (!result?.ok || !(result.binary instanceof Uint8Array)) {
    const stage = result?.stage ? ` at ${result.stage}` : '';
    throw new Error(`Mega Drive build failed${stage}\n${result?.log || ''}`);
  }

  onProgress('Finalizing cartridge padding and checksum…');
  const rom = finalizeGenesisRom(result.binary);
  const validation = validateGenesisRom(rom);
  return {
    rom,
    log: result.log || '',
    symbols: result.symbols || '',
    elapsedMs: performance.now() - started,
    checksum: validation.checksum,
    system: validation.system,
  };
}
