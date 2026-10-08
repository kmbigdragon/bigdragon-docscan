#!/usr/bin/env node
// Git pre-commit hook (run by husky, see .husky/pre-commit): quick checks picked from the staged
// files, so a commit only waits for what it can break. The full build/test matrix runs in CI.
// Skip in an emergency with `git commit --no-verify`.

import { execFileSync, spawnSync } from 'node:child_process';
import { existsSync, readFileSync } from 'node:fs';
import { join } from 'node:path';
import { repoRoot } from '../lib/toolchain.mjs';

const staged = execFileSync('git', ['diff', '--cached', '--name-only', '--diff-filter=ACMR'], {
  cwd: repoRoot,
  encoding: 'utf8',
})
  .split('\n')
  .filter(Boolean);
const touches = (pattern) => staged.some((file) => pattern.test(file));
const exists = (path) => existsSync(join(repoRoot, path));

function fail(message) {
  console.error(`\npre-commit: ${message}\n(fix it, or skip the checks once with \`git commit --no-verify\`)`);
  process.exit(1);
}

// Fixed command lines only (no user input): a shell is needed for npm/npx (.cmd) on Windows.
function run(name, commandLine) {
  console.log(`pre-commit: ${name}`);
  const result = spawnSync(commandLine, { cwd: repoRoot, stdio: 'inherit', shell: true });
  if (result.status !== 0) fail(`"${name}" failed`);
}

function skip(name, reason) {
  console.log(`pre-commit: skip ${name} (${reason})`);
}

// 1. Syntax of staged JSON and JavaScript files.
for (const file of staged.filter((f) => f.endsWith('.json'))) {
  try {
    JSON.parse(readFileSync(join(repoRoot, file), 'utf8'));
  } catch (error) {
    fail(`${file} is not valid JSON: ${error.message}`);
  }
}
for (const file of staged.filter((f) => /\.(mjs|js)$/.test(f) && !f.startsWith('dist/'))) {
  const result = spawnSync(process.execPath, ['--check', file], { cwd: repoRoot, stdio: 'inherit' });
  if (result.status !== 0) fail(`${file} has a syntax error`);
}

// 2. TypeScript wrapper.
const jsChanged = touches(/^(js\/|tsconfig\.json$|package\.json$)/);
if (jsChanged) run('typecheck (tsc --noEmit)', 'npx tsc -p tsconfig.json --noEmit');

// 3. C++: incremental native build + GoogleTest, when a native build tree exists.
const cppChanged = touches(/(\.(cpp|hpp|h|cmake)|CMakeLists\.txt|CMakePresets\.json|version\.hpp\.in)$/);
if (cppChanged) {
  if (exists('build/native/CMakeCache.txt')) {
    run('C++ build', 'cmake --build --preset native');
    run('C++ tests', 'ctest --preset native');
  } else {
    skip('C++ build and tests', 'run `cmake --preset native` once to enable them');
  }
}

// 4. JavaScript API tests against the wasm build (rebuilt first if the bindings changed).
if (touches(/^(js\/|bindings\/|tests\/js\/)/)) {
  if (exists('dist/wasm/docscan.wasm')) {
    if (touches(/^bindings\//)) run('wasm build', 'npm run build:wasm');
    if (jsChanged) run('TypeScript build', 'npm run build:ts');
    run('JS tests', 'npm test');
  } else {
    skip('JS tests', 'run `npm run build` once to enable them');
  }
}

// 5. GitHub Actions workflows, if actionlint is installed.
if (touches(/^\.github\/workflows\//)) {
  const probe = spawnSync('actionlint -version', { shell: true, stdio: 'ignore' });
  if (probe.status === 0) run('actionlint', 'actionlint');
  else skip('actionlint', 'not installed: https://github.com/rhysd/actionlint');
}
