#!/usr/bin/env node
// Installs a packed tarball into a fresh, empty project and uses it the way a consumer would:
// catches files missing from `files`, broken `exports`, or a wasm that is not found at runtime.
//   node scripts/release/smoke-test-package.mjs docscan-0.1.0.tgz

import { spawnSync } from 'node:child_process';
import { mkdtempSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';

const tarball = process.argv[2];
if (!tarball) {
  console.error('Usage: node scripts/release/smoke-test-package.mjs <package.tgz>');
  process.exit(1);
}

const project = mkdtempSync(join(tmpdir(), 'docscan-smoke-'));
try {
  writeFileSync(join(project, 'package.json'), JSON.stringify({ name: 'smoke', private: true, type: 'module' }));
  // npm is npm.cmd on Windows, which Node only spawns through a shell: pass one quoted command line.
  const install = spawnSync(`npm install --no-audit --no-fund "${resolve(tarball)}"`, {
    cwd: project,
    stdio: 'inherit',
    shell: true,
  });
  if (install.status !== 0) throw new Error('npm install of the tarball failed');

  writeFileSync(
    join(project, 'smoke.mjs'),
    `import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { createDocScanner, AspectRatio } from 'docscan';

const { version } = JSON.parse(readFileSync('node_modules/docscan/package.json', 'utf8'));
const scanner = await createDocScanner();
assert.equal(scanner.version, version, 'wasm version matches package.json');

// Light page on a dark background.
const width = 320, height = 240;
const data = new Uint8ClampedArray(width * height * 4);
for (let y = 0; y < height; y++)
  for (let x = 0; x < width; x++) {
    const v = x >= 60 && x < 260 && y >= 40 && y < 200 ? 235 : 50;
    data.set([v, v, v, 255], (y * width + x) * 4);
  }
const { detection, image } = scanner.scan({ width, height, data }, { enhance: 'bw' });
assert.equal(detection.found, true, 'page found');
assert.ok(Math.abs(image.width - 200) <= 4 && Math.abs(image.height - 160) <= 4, 'cropped size');
assert.ok(AspectRatio.A4 > 1.4);
console.log('smoke test passed: docscan ' + version);
`,
  );
  const run = spawnSync(process.execPath, ['smoke.mjs'], { cwd: project, stdio: 'inherit' });
  if (run.status !== 0) throw new Error('smoke test failed');
} finally {
  rmSync(project, { recursive: true, force: true });
}
