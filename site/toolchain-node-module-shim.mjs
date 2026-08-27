// Emscripten-generated ES modules can retain a Node createRequire import even
// when their runtime branch is the browser. The staged compiler glue rewrites
// that bare `module` specifier to this file so browsers can resolve the module.
// If createRequire is ever actually called in a browser, fail explicitly.
export function createRequire() {
  return function browserRequire(name) {
    throw new Error(`Node require(${JSON.stringify(name)}) is unavailable in the gbc-to-md browser toolchain`);
  };
}

export default { createRequire };
