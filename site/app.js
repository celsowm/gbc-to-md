import { analyzeRom } from './analyzer.js';
import { buildMegaDriveRom } from './browser-build.js';

const drop = document.querySelector('#drop-zone');
const picker = document.querySelector('#rom-input');
const annotationsPicker = document.querySelector('#annotations-input');
const annotationsButton = document.querySelector('#annotations-button');
const annotationsStatus = document.querySelector('#annotations-status');
const result = document.querySelector('#result');
const empty = document.querySelector('#empty-state');
const recompile = document.querySelector('#recompile-button');
const buildRom = document.querySelector('#build-rom-button');
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
const romPanel = document.querySelector('#rom-panel');
const romBuildStatus = document.querySelector('#rom-build-status');
const romBuildSummary = document.querySelector('#rom-build-summary');
const downloadRom = document.querySelector('#download-rom-button');

let selectedFile = null;
let selectedBytes = null;
let selectedAnnotationsFile = null;
let selectedAnnotationsText = '';
let moduleInstance = null;
let generatedFiles = [];
let selectedGenerated = null;
let builtRom = null;

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
  buildRom.disabled = generatedFiles.length === 0 || selectedBytes?.length > 0x400000;
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
      locateFile: (p) => `./wasm/${p}`,
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
    button.innerHTML = `<span></span><small>${file.bytes.toLocaleString()} B</small>`;
    button.querySelector('span').textContent = file.name;
    button.addEventListener('click', () => selectGenerated(file));
    generatedFilesEl.append(button);
  }
}

function downloadBlob(name, data, type = 'application/octet-stream') {
  const blob = new Blob([data], { type });
  const url = URL.createObjectURL(blob);
  const anchor = document.createElement('a');
  anchor.href = url;
  anchor.download = name;
  document.body.append(anchor);
  anchor.click();
  anchor.remove();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}

downloadFile.addEventListener('click', () => {
  if (selectedGenerated) downloadBlob(selectedGenerated.name, selectedGenerated.content, 'text/plain;charset=utf-8');
});
downloadRom.addEventListener('click', () => {
  if (builtRom) downloadBlob('rom.bin', builtRom);
});

async function compileSelectedRom() {
  if (!moduleInstance || !selectedBytes) return;
  compilerPanel.hidden = false;
  romPanel.hidden = true;
  builtRom = null;
  compileStatus.textContent = 'Recompiling…';
  compileSummary.textContent = '';
  generatedFilesEl.replaceChildren();
  sourcePreview.textContent = selectedAnnotationsText
    ? 'Running reachable-only static analysis with supplied annotations…'
    : 'Running static analysis and C generation…';
  previewName.textContent = 'Working';
  downloadFile.disabled = true;
  recompile.disabled = true;
  buildRom.disabled = true;
  await new Promise((resolve) => requestAnimationFrame(resolve));

  const romPtr = moduleInstance._malloc(selectedBytes.length);
  const annotationsBytes = selectedAnnotationsText ? new TextEncoder().encode(selectedAnnotationsText) : null;
  const annotationsPtr = annotationsBytes?.length ? moduleInstance._malloc(annotationsBytes.length) : 0;
  const started = performance.now();
  try {
    moduleInstance.HEAPU8.set(selectedBytes, romPtr);
    let generatedCount;
    if (annotationsBytes?.length) {
      if (typeof moduleInstance._gbrecomp_wasm_compile_annotated !== 'function') {
        throw new Error('This playground build does not expose annotated recompilation yet.');
      }
      moduleInstance.HEAPU8.set(annotationsBytes, annotationsPtr);
      generatedCount = moduleInstance._gbrecomp_wasm_compile_annotated(
        romPtr,
        selectedBytes.length,
        annotationsPtr,
        annotationsBytes.length,
      );
    } else {
      generatedCount = moduleInstance._gbrecomp_wasm_compile(romPtr, selectedBytes.length);
    }
    if (generatedCount < 0) throw new Error(readLastError());
    const preparedCount = moduleInstance._gbrecomp_wasm_prepare_sgdk(selectedBytes.length);
    if (preparedCount < 0) throw new Error(readLastError());

    generatedFiles = collectGeneratedFiles();
    const elapsed = performance.now() - started;
    const totalBytes = generatedFiles.reduce((sum, file) => sum + file.bytes, 0);
    compileStatus.textContent = 'Recompilation complete';
    compileSummary.textContent = `${generatedFiles.length} files · ${totalBytes.toLocaleString()} source bytes · ${elapsed.toFixed(0)} ms${selectedAnnotationsText ? ' · annotated' : ''}`;
    renderGeneratedFiles();
    const preferred = generatedFiles.find((file) => /_funcs_\d+\.c$/.test(file.name)) || generatedFiles.find((file) => file.name.endsWith('.c')) || generatedFiles[0];
    if (preferred) selectGenerated(preferred);
    buildRom.disabled = selectedBytes.length > 0x400000;
  } catch (error) {
    console.error(error);
    generatedFiles = [];
    compileStatus.textContent = 'Recompilation failed';
    compileSummary.textContent = error instanceof Error ? error.message : String(error);
    previewName.textContent = 'Error';
    sourcePreview.textContent = error instanceof Error ? error.stack || error.message : String(error);
  } finally {
    if (annotationsPtr) moduleInstance._free(annotationsPtr);
    moduleInstance._free(romPtr);
    recompile.disabled = false;
  }
}

async function buildSelectedRom() {
  if (!selectedBytes || !generatedFiles.length) return;
  buildRom.disabled = true;
  recompile.disabled = true;
  romPanel.hidden = false;
  downloadRom.disabled = true;
  romBuildStatus.textContent = 'Building Mega Drive ROM…';
  romBuildSummary.textContent = 'Starting Motorola 68000 toolchain…';
  await new Promise((resolve) => requestAnimationFrame(resolve));

  try {
    const output = await buildMegaDriveRom({
      romBytes: selectedBytes,
      generatedFiles,
      onProgress: (message) => { romBuildSummary.textContent = message; },
    });
    builtRom = output.rom;
    romBuildStatus.textContent = 'Mega Drive ROM ready';
    romBuildSummary.textContent = `${output.rom.length.toLocaleString()} bytes · checksum 0x${output.checksum.toString(16).padStart(4, '0')} · ${(output.elapsedMs / 1000).toFixed(2)} s`;
    downloadRom.disabled = false;
  } catch (error) {
    console.error(error);
    builtRom = null;
    romBuildStatus.textContent = 'ROM build failed';
    romBuildSummary.textContent = error instanceof Error ? error.message : String(error);
  } finally {
    recompile.disabled = false;
    buildRom.disabled = !generatedFiles.length || selectedBytes.length > 0x400000;
  }
}

async function handleAnnotationFile(file) {
  if (!file) return;
  if (!file.name.toLowerCase().endsWith('.annotations')) {
    alert('Choose a .annotations sidecar file.');
    return;
  }
  selectedAnnotationsFile = file;
  selectedAnnotationsText = await file.text();
  annotationsStatus.textContent = `${file.name} · ${selectedAnnotationsText.split(/\r?\n/).filter(Boolean).length.toLocaleString()} lines`;
  generatedFiles = [];
  selectedGenerated = null;
  builtRom = null;
  buildRom.disabled = true;
  romPanel.hidden = true;
  if (!compilerPanel.hidden) {
    compileStatus.textContent = 'Annotations changed — recompile required';
    compileSummary.textContent = '';
  }
}

async function handleFile(file, { keepAnnotations = false } = {}) {
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
    if (!keepAnnotations) {
      selectedAnnotationsFile = null;
      selectedAnnotationsText = '';
      annotationsStatus.textContent = 'No sidecar selected';
      annotationsPicker.value = '';
    }
    generatedFiles = [];
    selectedGenerated = null;
    builtRom = null;
    compilerPanel.hidden = true;
    romPanel.hidden = true;
    buildRom.disabled = true;
    render(analyzeRom(buffer), file);
  } catch (error) {
    alert(error instanceof Error ? error.message : String(error));
  }
}

async function handleDroppedFiles(fileList) {
  const files = [...(fileList || [])];
  const rom = files.find((file) => /\.(gb|gbc)$/i.test(file.name));
  const annotations = files.find((file) => /\.annotations$/i.test(file.name));
  if (!rom) {
    alert('Drop a .gb or .gbc cartridge image.');
    return;
  }
  await handleFile(rom, { keepAnnotations: Boolean(annotations) });
  if (annotations) await handleAnnotationFile(annotations);
}

recompile.addEventListener('click', compileSelectedRom);
buildRom.addEventListener('click', buildSelectedRom);
annotationsButton.addEventListener('click', () => annotationsPicker.click());
annotationsPicker.addEventListener('change', () => handleAnnotationFile(annotationsPicker.files?.[0]));
picker.addEventListener('change', () => handleFile(picker.files?.[0]));
drop.addEventListener('click', () => picker.click());
drop.addEventListener('keydown', (event) => {
  if (event.key === 'Enter' || event.key === ' ') { event.preventDefault(); picker.click(); }
});
for (const name of ['dragenter', 'dragover']) drop.addEventListener(name, (event) => { event.preventDefault(); drop.dataset.dragging = 'true'; });
for (const name of ['dragleave', 'drop']) drop.addEventListener(name, (event) => { event.preventDefault(); drop.dataset.dragging = 'false'; });
drop.addEventListener('drop', (event) => handleDroppedFiles(event.dataTransfer?.files));

loadRecompiler();
