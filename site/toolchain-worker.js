const factoryCache = new Map();

function ensureDir(FS, dir) {
  const parts = dir.split('/').filter(Boolean);
  let cur = '';
  for (const part of parts) {
    cur += '/' + part;
    try { FS.mkdir(cur); } catch {}
  }
}

function dirname(p) {
  const i = p.lastIndexOf('/');
  return i <= 0 ? '/' : p.slice(0, i);
}

function fromBase64(text) {
  const raw = atob(text);
  const out = new Uint8Array(raw.length);
  for (let i = 0; i < raw.length; i += 1) out[i] = raw.charCodeAt(i);
  return out;
}

function toBase64(bytes) {
  const CHUNK = 0x8000;
  let text = '';
  for (let i = 0; i < bytes.length; i += CHUNK) {
    const part = bytes.subarray(i, Math.min(bytes.length, i + CHUNK));
    let s = '';
    for (let j = 0; j < part.length; j += 1) s += String.fromCharCode(part[j]);
    text += s;
  }
  return btoa(text);
}

async function loadFactory(glueFile) {
  if (factoryCache.has(glueFile)) return factoryCache.get(glueFile);
  const url = new URL(`./toolchain/wasm/${glueFile}`, import.meta.url);
  const factory = (await import(url.href)).default;
  factoryCache.set(glueFile, factory);
  return factory;
}

async function runJob(job) {
  const factory = await loadFactory(job.glueFile);
  let log = '';
  let capturedExit = null;
  const print = (msg) => { log += `${msg}\n`; };

  const mod = await factory({
    noInitialRun: true,
    print,
    printErr: print,
    locateFile: (name) => new URL(`./toolchain/wasm/${name}`, import.meta.url).href,
    quit: (status, error) => {
      capturedExit = status;
      throw error || new Error(`exit ${status}`);
    },
    onExit: (status) => { capturedExit = status; },
  });

  for (const file of job.inputFiles || []) {
    ensureDir(mod.FS, dirname(file.vfsPath));
    const data = file.encoding === 'base64' ? fromBase64(file.data) : file.data;
    mod.FS.writeFile(file.vfsPath, data);
  }
  for (const file of job.outputFiles || []) ensureDir(mod.FS, dirname(file.vfsPath));

  let exitCode = 0;
  try {
    mod.callMain(job.argv || []);
  } catch (error) {
    if (error && typeof error === 'object' && 'status' in error) exitCode = error.status;
    else if (capturedExit !== null) exitCode = capturedExit;
    else {
      log += `[browser-worker] ${error?.stack || error}\n`;
      exitCode = 1;
    }
  }
  if (capturedExit !== null && exitCode === 0) exitCode = capturedExit;

  const outputs = {};
  for (const file of job.outputFiles || []) {
    try {
      const bytes = mod.FS.readFile(file.vfsPath);
      outputs[file.vfsPath] = file.encoding === 'base64'
        ? toBase64(bytes)
        : new TextDecoder().decode(bytes);
    } catch {
      outputs[file.vfsPath] = '';
    }
  }
  return { exitCode, log, outputs };
}

self.onmessage = async (event) => {
  const { id, job } = event.data || {};
  try {
    const result = await runJob(job);
    self.postMessage({ id, result });
  } catch (error) {
    self.postMessage({ id, error: error?.stack || error?.message || String(error) });
  }
};
