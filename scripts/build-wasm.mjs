#!/usr/bin/env node
// Configures and builds the WebAssembly module: dist/wasm/docscan.mjs + dist/wasm/docscan.wasm.
//   node scripts/build-wasm.mjs

import { existsSync } from 'node:fs';
import { join } from 'node:path';
import { findEmscriptenToolchain, repoRoot, run } from './lib/toolchain.mjs';

if (!existsSync(join(repoRoot, 'third_party', 'opencv', 'wasm', 'lib', 'cmake', 'opencv4'))) {
  console.error('OpenCV for WebAssembly is missing: run `npm run opencv:wasm` first (one-time, a few minutes).');
  process.exit(1);
}

run('cmake', ['--preset', 'wasm', `-DCMAKE_TOOLCHAIN_FILE=${findEmscriptenToolchain()}`]);
run('cmake', ['--build', '--preset', 'wasm']);
