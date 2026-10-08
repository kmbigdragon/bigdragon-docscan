#!/usr/bin/env node
// Packs the npm tarball without the dev-only `prepare` script (husky): npm >= 11 would warn
// everyone installing the tarball about an unapproved install script. package.json is restored
// afterwards, even on failure. Prints only the tarball file name (for CI).
//   node scripts/release/pack.mjs

import { spawnSync } from 'node:child_process';
import { readFileSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { repoRoot } from '../lib/toolchain.mjs';

const manifestPath = join(repoRoot, 'package.json');
const original = readFileSync(manifestPath, 'utf8');
const manifest = JSON.parse(original);
delete manifest.scripts.prepare;

writeFileSync(manifestPath, JSON.stringify(manifest, null, 2) + '\n');
let result;
try {
  result = spawnSync('npm pack --silent', { cwd: repoRoot, shell: true, encoding: 'utf8', stdio: ['ignore', 'pipe', 'inherit'] });
} finally {
  writeFileSync(manifestPath, original);
}
if (result.status !== 0) process.exit(result.status ?? 1);
console.log(result.stdout.trim().split(/\r?\n/).at(-1));
