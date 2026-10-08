#!/usr/bin/env node
// Runs before `npm pack` / `npm publish` (the `prepack` script): refuses to package a missing or
// stale build. Prints nothing on success so `npm pack --silent` still prints only the file name.

import { existsSync, readFileSync } from 'node:fs';
import { join } from 'node:path';
import { pathToFileURL } from 'node:url';
import { repoRoot } from '../lib/toolchain.mjs';

const fail = (message) => {
  console.error(`prepack: ${message}`);
  process.exit(1);
};

for (const file of ['dist/index.js', 'dist/index.d.ts', 'dist/wasm/docscan.mjs', 'dist/wasm/docscan.wasm']) {
  if (!existsSync(join(repoRoot, file))) fail(`${file} is missing - run \`npm run build\` first.`);
}

// The version is compiled into the wasm module from package.json: a mismatch means the build is stale.
const { version } = JSON.parse(readFileSync(join(repoRoot, 'package.json'), 'utf8'));
const { createDocScanner } = await import(pathToFileURL(join(repoRoot, 'dist', 'index.js')).href);
const built = (await createDocScanner()).version;
if (built !== version) fail(`dist/ was built for ${built} but package.json says ${version} - run \`npm run build\`.`);
