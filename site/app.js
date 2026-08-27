import { analyzeRom } from './analyzer.js';

const drop = document.querySelector('#drop-zone');
const picker = document.querySelector('#rom-input');
const result = document.querySelector('#result');
const empty = document.querySelector('#empty-state');
const recompile = document.querySelector('#recompile-button');
const fileName = document.querySelector('#file-name');
const statusPill = document.querySelector('#status-pill');
const compilerPanel = document.querySelector('#compiler-panel');
const compileStatus = document.querySelector('#compile-status');
const compileSummary = document.querySelector('#compile-summary');
const generatedFilesEl = document.querySelector('#generated-files');
const previewName = document.querySelector('#preview-name');
const sourcePreview = document.querySelector('#source-preview');
const downloadFile = document.querySelector('#download-file');
const wasmState = document.querySelector('#wasm-state');

let selectedFile = null;
let selectedBytes = null;
let moduleInstance = null;
let generatedFiles = [];
let selectedGenerated = null;

function setText(id, value) { document.querySelector(id).textContent = value; }
function yesNo(value) { return value ? 'Yes' : 'No'; }

function render(analysis, file) {
  empty.hidden = true;
  result.hidden = false;
  fileName.textContent = file.name;
  setText('#title', analysis.title);
  setText('#mode', analysis.mode);
  setText('#cartridge', analysis.cartridge);
  setText('#mapper', analysis.mapper);
  setText('#rom-size', `${analysis.fileSizeText} (${analysis.romBanks} banks)`);
  setText('#ram-size', analysis.ramSizeText);
  setText('#far-rom', analysis.destinationMapper);
  setText('#header-checksum', analysis.headerChecksumValid ? 'Valid' : 'Mismatch');
  setText('#global-checksum', analysis.globalChecksumValid ? 'Valid' : 'Mismatch');
  setText('#cgb', yesNo(analysis.cgb));

  statusPill.dataset.state = analysis.compatibility;
  statusPill.textContent = analysis.compatibility.toUpperCase();

  const list = document.querySelector('#warnings');
  list.replaceChildren();
  if (!analysis.warnings.length) {
    const li = document.createElement('li');
    li.textContent = 'No analyzer warnings for the currently implemented cartridge features.';
    li.className = 'ok';
    list.append(li);
  } else {
    for (const warning of analysis.warnings) {
      const li = document.createElement('li');
      li.textContent = warning;
      list.append(li);
    }
  }

  recompile.disabled = moduleInstance === null;
  recompile.title = moduleInstance ? 'Run GB Recompiled locally in WebAssembly.' : 'WebAssembly recompiler is still loading.';
}

function loadClassicScript(src) {
  return new Promise((resolve, reject) => {
    const script = document.createElement('script');
    script.src = src;
    script.onload = resolve;
    script.onerror = () => reject(new Error(`Failed to load ${src}`));
    document.head.append(script);
  });
}

async function loadRecompiler() {
  try {
    await loadClassicScript('./wasm/gbrecomp_probe.js');
    if (typeof globalThis.createGBRecompProbe !== 'function') throw new Error('createGBRecompProbe was not exported');
    moduleInstance = await globalThis.createGBRecompProbe({
      locateFile: (path) => `./wasm/${path}`,
      print: () => {},
      printErr: (text) => console.error(text),
    });
    wasmState.textContent = 'recompiler ready';
    wasmState.dataset.ready = 'true';
    if (selectedFile) recompile.disabled = false;
  } catch (error) {
    console.error(error);
    wasmState.textContent = 'recompiler unavailable';
    wasmState.dataset.ready = 'false';
    recompile.title = error instanceof Error ? error.message : String(error);
  }
}

function readWasmString(ptr, size) {
  if (!ptr || !size) return '';
  return new TextDecoder().decode(moduleInstance.HEAPU8.slice(ptr, ptr + size));
}

function readLastError() {
  return readWasmString(moduleInstance._gbrecomp_wasm_error_ptr(), moduleInstance._gbrecomp_wasm_error_size()) || 'Unknown recompiler error';
}

function collectGeneratedFiles() {
  const count = moduleInstance._gbrecomp_wasm_file_count();
  const files = [];
  for (let i = 0; i < count; i += 1) {
    const name = readWasmString(moduleInstance._gbrecomp_wasm_file_name_ptr(i), moduleInstance._gbrecomp_wasm_file_name_size(i));
    const content = readWasmString(moduleInstance._gbrecomp_wasm_file_data_ptr(i), moduleInstance._gbrecomp_wasm_file_data_size(i));
    files.push({ name, content, bytes: new TextEncoder().encode(content).length });
  }
  return files;
}

function selectGenerated(file) {
  selectedGenerated = file;
  previewName.textContent = file.name;
  sourcePreview.textContent = file.content;
  downloadFile.disabled = false;
  document.querySelectorAll('.generated-file').forEach((node) => node.classList.toggle('selected', node.dataset.name === file.name));
}

function renderGeneratedFiles() {
  generatedFilesEl.replaceChildren();
  for (const file of generatedFiles) {
    const button = document.createElement('button');
    button.className = 'generated-file';
    button.dataset.name = file.name;
    const name = document.createElement('span');
    name.textContent = file.name;
    const size = document.createElement('small');
    size.textContent = `${file.bytes.toLocaleString()} B`;
    button.append(name, size);
    button.addEventListener('click', () => selectGenerated(file));
    generatedFilesEl.append(button);
  }
}

function downloadText(name, content) {
  const blob = new Blob([content], { type: 'text/plain;charset=utf-8' });
  const url = URL.createObjectURL(blob);
  const anchor = document.createElement('a');
  anchor.href = url;
  anchor.download = name;
  document.body.append(anchor);
  anchor.click();
  anchor.remove();
  setTimeout(() => URL.revokeObjectURL(url), 500);
}

downloadFile.addEventListener('click', () => {
  if (selectedGenerated) downloadText(selectedGenerated.name, selectedGenerated.content);
});

async function compileSelectedRom() {
  if (!moduleInstance || !selectedBytes) return;

  compilerPanel.hidden = false;
  compileStatus.textContent = 'Recompiling…';
  compileSummary.textContent = '';
  generatedFilesEl.replaceChildren();
  sourcePreview.textContent = 'Running static analysis and C generation…';
  previewName.textContent = 'Working';
  downloadFile.disabled = true;
  recompile.disabled = true;

  await new Promise((resolve) => requestAnimationFrame(() => resolve()));

  const ptr = moduleInstance._malloc(selectedBytes.length);
  const started = performance.now();
  try {
    moduleInstance.HEAPU8.set(selectedBytes, ptr);
    const generatedCount = moduleInstance._gbrecomp_wasm_compile(ptr, selectedBytes.length);
    if (generatedCount < 0) throw new Error(readLastError());

    const preparedCount = moduleInstance._gbrecomp_wasm_prepare_sgdk(selectedBytes.length);
    if (preparedCount < 0) throw new Error(readLastError());

    generatedFiles = collectGeneratedFiles();
    const elapsed = performance.now() - started;
    const totalBytes = generatedFiles.reduce((sum, file) => sum + file.bytes, 0);
    compileStatus.textContent = 'Recompilation complete';
    compileSummary.textContent = `${generatedFiles.length} files · ${totalBytes.toLocaleString()} source bytes · ${elapsed.toFixed(0)} ms`;
    renderGeneratedFiles();

    const preferred = generatedFiles.find((file) => file.name.endsWith('_functions.c'))
      || generatedFiles.find((file) => file.name.endsWith('.c'))
      || generatedFiles[0];
    if (preferred) selectGenerated(preferred);
  } catch (error) {
    console.error(error);
    compileStatus.textContent = 'Recompilation failed';
    compileSummary.textContent = error instanceof Error ? error.message : String(error);
    previewName.textContent = 'Error';
    sourcePreview.textContent = error instanceof Error ? error.stack || error.message : String(error);
  } finally {
    moduleInstance._free(ptr);
    recompile.disabled = false;
  }
}

recompile.addEventListener('click', compileSelectedRom);

async function handleFile(file) {
  if (!file) return;
  const lower = file.name.toLowerCase();
  if (!lower.endsWith('.gb') && !lower.endsWith('.gbc')) {
    alert('Choose a .gb or .gbc cartridge image.');
    return;
  }
  try {
    const buffer = await file.arrayBuffer();
    selectedFile = file;
    selectedBytes = new Uint8Array(buffer);
    generatedFiles = [];
    selectedGenerated = null;
    compilerPanel.hidden = true;
    render(analyzeRom(buffer), file);
  } catch (error) {
    alert(error instanceof Error ? error.message : String(error));
  }
}

picker.addEventListener('change', () => handleFile(picker.files?.[0]));
drop.addEventListener('click', () => picker.click());
drop.addEventListener('keydown', event => {
  if (event.key === 'Enter' || event.key === ' ') { event.preventDefault(); picker.click(); }
});
for (const name of ['dragenter', 'dragover']) {
  drop.addEventListener(name, event => { event.preventDefault(); drop.dataset.dragging = 'true'; });
}
for (const name of ['dragleave', 'drop']) {
  drop.addEventListener(name, event => { event.preventDefault(); drop.dataset.dragging = 'false'; });
}
drop.addEventListener('drop', event => handleFile(event.dataTransfer?.files?.[0]));

loadRecompiler();
