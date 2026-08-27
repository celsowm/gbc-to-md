// The published m68k toolchain was linked by Emscripten with ENVIRONMENT=node.
// The compiler/linker WASM itself is still usable in a Web Worker when the
// host supplies wasmBinary + MEMFS; only the generated JS bootstrap expects
// createRequire("module") and a few Node facades. Pages rewrites that import
// to this file.

function dirname(p) {
  const value = String(p).replace(/\\/g, '/');
  const i = value.lastIndexOf('/');
  return i <= 0 ? '/' : value.slice(0, i);
}

function basename(p) {
  const value = String(p).replace(/\\/g, '/');
  const i = value.lastIndexOf('/');
  return i < 0 ? value : value.slice(i + 1);
}

const pathShim = {
  normalize: (p) => String(p).replace(/\\/g, '/'),
  dirname,
  basename,
  join: (...parts) => parts.filter(Boolean).join('/').replace(/\/+/g, '/'),
  resolve: (...parts) => ('/' + parts.filter(Boolean).join('/')).replace(/\/+/g, '/'),
  sep: '/',
  delimiter: ':',
};

const fsShim = {
  readFileSync() {
    throw new Error('Unexpected host fs.readFileSync: browser worker must pass wasmBinary explicitly');
  },
  readFile(_path, callback) {
    const error = new Error('Unexpected host fs.readFile: browser worker must pass wasmBinary explicitly');
    if (typeof callback === 'function') callback(error);
    else throw error;
  },
};

const urlShim = {
  fileURLToPath(value) {
    const url = value instanceof URL ? value : new URL(String(value), self.location?.href);
    return decodeURIComponent(url.pathname);
  },
  pathToFileURL(value) {
    return new URL(String(value).replace(/^\/?/, '/'), self.location?.origin || 'http://localhost');
  },
};

export function createRequire() {
  return function browserRequire(name) {
    switch (name) {
      case 'fs':
      case 'node:fs':
        return fsShim;
      case 'path':
      case 'node:path':
        return pathShim;
      case 'url':
      case 'node:url':
        return urlShim;
      case 'module':
      case 'node:module':
        return { createRequire };
      default:
        throw new Error(`Unsupported Node require(${JSON.stringify(name)}) in gbc-to-md browser toolchain`);
    }
  };
}

export default { createRequire };
