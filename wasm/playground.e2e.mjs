#!/usr/bin/env node
import fs from 'node:fs';
import http from 'node:http';
import path from 'node:path';
import { chromium } from 'playwright-core';

const root = path.resolve(process.argv[2] || 'site');
const fixture = path.resolve(process.argv[3] || 'fixtures/basicdemo.gb');
const annotations = process.argv[4] ? path.resolve(process.argv[4]) : null;
const port = Number(process.env.PLAYGROUND_PORT || 4173);
const fixtureBytes = fs.statSync(fixture).size;
const buildTimeoutMs = fixtureBytes > 0x400000 ? 240000 : 90000;

const mime = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.mjs': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.json': 'application/json',
  '.wasm': 'application/wasm',
  '.a': 'application/octet-stream',
  '.bin': 'application/octet-stream',
};

function safePath(urlPath) {
  const rel = decodeURIComponent(urlPath.split('?')[0]).replace(/^\/+/, '') || 'index.html';
  const file = path.resolve(root, rel);
  if (!file.startsWith(root + path.sep) && file !== path.join(root, 'index.html')) return null;
  return file;
}

const server = http.createServer((req, res) => {
  const file = safePath(req.url || '/');
  if (!file) { res.writeHead(403).end('forbidden'); return; }
  fs.stat(file, (statErr, stat) => {
    let target = file;
    if (!statErr && stat.isDirectory()) target = path.join(file, 'index.html');
    fs.readFile(target, (err, data) => {
      if (err) { res.writeHead(404).end('not found'); return; }
      res.setHeader('Content-Type', mime[path.extname(target)] || 'application/octet-stream');
      res.setHeader('Cache-Control', 'no-store');
      res.end(data);
    });
  });
});

function validateRom(buf) {
  if (buf.length < 0x200) throw new Error(`downloaded ROM too small: ${buf.length}`);
  const system = buf.subarray(0x100, 0x110).toString('ascii');
  if (!system.startsWith('SEGA')) throw new Error(`missing SEGA header: ${JSON.stringify(system)}`);
  let sum = 0;
  for (let i = 0x200; i + 1 < buf.length; i += 2) sum = (sum + ((buf[i] << 8) | buf[i + 1])) & 0xffff;
  const stored = (buf[0x18e] << 8) | buf[0x18f];
  if (stored !== sum) throw new Error(`checksum mismatch stored=${stored.toString(16)} calculated=${sum.toString(16)}`);
  return { system, checksum: stored };
}

let browser;
try {
  await new Promise((resolve) => server.listen(port, '127.0.0.1', resolve));
  const chrome = process.env.CHROME_BIN || '/usr/bin/google-chrome';
  browser = await chromium.launch({ headless: true, executablePath: chrome, args: ['--no-sandbox'] });
  const page = await browser.newPage({ acceptDownloads: true });
  page.on('console', (msg) => console.log(`[browser ${msg.type()}] ${msg.text()}`));
  page.on('pageerror', (err) => console.error(`[browser error] ${err.stack || err}`));

  await page.goto(`http://127.0.0.1:${port}/`, { waitUntil: 'networkidle' });
  await page.waitForSelector('#wasm-state[data-ready="true"]', { timeout: 60000 });
  await page.setInputFiles('#rom-input', fixture);
  if (annotations) {
    await page.setInputFiles('#annotations-input', annotations);
    await page.waitForFunction(() => {
      const text = document.querySelector('#annotations-status')?.textContent || '';
      return !text.includes('No sidecar');
    }, null, { timeout: 10000 });
  }
  await page.waitForSelector('#recompile-button:not([disabled])');
  await page.click('#recompile-button');
  await page.waitForFunction(() => {
    const text = document.querySelector('#compile-status')?.textContent || '';
    return text === 'Recompilation complete' || text === 'Recompilation failed';
  }, null, { timeout: 60000 });
  const compileState = await page.locator('#compile-status').textContent();
  if (compileState !== 'Recompilation complete') {
    throw new Error(`playground recompilation failed: ${await page.locator('#compile-summary').textContent()}`);
  }

  await page.waitForSelector('#build-rom-button:not([disabled])');
  await page.click('#build-rom-button');
  await page.waitForFunction(() => {
    const text = document.querySelector('#rom-build-status')?.textContent || '';
    return text === 'Mega Drive ROM ready' || text === 'ROM build failed';
  }, null, { timeout: buildTimeoutMs });

  const status = await page.locator('#rom-build-status').textContent();
  if (status !== 'Mega Drive ROM ready') {
    const summary = await page.locator('#rom-build-summary').textContent();
    throw new Error(`playground ROM build failed: ${summary}`);
  }
  await page.waitForSelector('#download-rom-button:not([disabled])');

  const [download] = await Promise.all([
    page.waitForEvent('download'),
    page.click('#download-rom-button'),
  ]);
  const downloadPath = await download.path();
  const rom = fs.readFileSync(downloadPath);
  const check = validateRom(rom);
  console.log(`PASS: real Chromium playground produced ${rom.length}-byte rom.bin`);
  console.log(`PASS: header=${JSON.stringify(check.system)} checksum=0x${check.checksum.toString(16).padStart(4, '0')}`);
  console.log(`annotations=${annotations || 'none'}`);
  console.log(`UI: ${await page.locator('#rom-build-summary').textContent()}`);
} finally {
  if (browser) await browser.close();
  await new Promise((resolve) => server.close(resolve));
}
