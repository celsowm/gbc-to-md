import { analyzeRom } from './analyzer.js';

const drop = document.querySelector('#drop-zone');
const picker = document.querySelector('#rom-input');
const result = document.querySelector('#result');
const empty = document.querySelector('#empty-state');
const convert = document.querySelector('#convert-button');
const fileName = document.querySelector('#file-name');
const statusPill = document.querySelector('#status-pill');

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

  convert.disabled = true;
  convert.title = 'The browser compiler is not bundled yet. Analysis already runs entirely on-device.';
}

async function handleFile(file) {
  if (!file) return;
  const lower = file.name.toLowerCase();
  if (!lower.endsWith('.gb') && !lower.endsWith('.gbc')) {
    alert('Choose a .gb or .gbc cartridge image.');
    return;
  }
  try {
    const buffer = await file.arrayBuffer();
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
