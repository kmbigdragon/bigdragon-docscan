// Helpers shared by the build scripts: locating Emscripten and running commands.

import { spawnSync } from 'node:child_process';
import { existsSync } from 'node:fs';
import { delimiter, dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

export const repoRoot = resolve(dirname(fileURLToPath(import.meta.url)), '..', '..');

/**
 * Absolute path of Emscripten's CMake toolchain file, found through $EMSDK, $EMSCRIPTEN
 * or an `emcc` on PATH.
 */
export function findEmscriptenToolchain() {
  const roots = [];
  if (process.env.EMSDK) roots.push(join(process.env.EMSDK, 'upstream', 'emscripten'));
  if (process.env.EMSCRIPTEN) roots.push(process.env.EMSCRIPTEN);
  for (const dir of (process.env.PATH ?? '').split(delimiter)) {
    if (dir && ['emcc', 'emcc.exe', 'emcc.bat', 'emcc.py'].some((name) => existsSync(join(dir, name)))) {
      roots.push(dir);
    }
  }

  for (const root of roots) {
    const toolchain = join(root, 'cmake', 'Modules', 'Platform', 'Emscripten.cmake');
    if (existsSync(toolchain)) return toolchain;
  }
  throw new Error(
    'Emscripten not found. Install emsdk (https://emscripten.org/docs/getting_started/downloads.html), ' +
      'then run `emsdk activate latest` and emsdk_env, or put emcc on PATH.',
  );
}

/** Runs a command from the repository root, streaming its output; exits if it fails. */
export function run(command, args, options = {}) {
  console.log(`\n> ${command} ${args.join(' ')}`);
  const result = spawnSync(command, args, { stdio: 'inherit', cwd: repoRoot, ...options });
  if (result.error) throw result.error;
  if (result.status !== 0) process.exit(result.status ?? 1);
}
