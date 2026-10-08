#!/usr/bin/env node
// Release guard: the pushed tag must be exactly "v" + package.json version.
//   node scripts/release/check-tag.mjs v0.2.0

import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { repoRoot } from '../lib/toolchain.mjs';

const tag = process.argv[2] ?? '';
const { version } = JSON.parse(readFileSync(join(repoRoot, 'package.json'), 'utf8'));
if (tag !== `v${version}`) {
  console.error(`Tag "${tag}" does not match package.json version "${version}" (expected "v${version}").`);
  console.error('Bump the version with `npm version <new-version>`, which also creates the matching tag.');
  process.exit(1);
}
console.log(`Tag ${tag} matches package.json.`);
