#!/usr/bin/env node
// Minimal static server for the examples (ES modules and .wasm cannot be loaded from file://).
//   node scripts/serve.mjs      ->  http://localhost:8080/examples/web/
// Environment: PORT (default 8080), HOST (default 127.0.0.1).

import { createReadStream, statSync } from 'node:fs';
import { createServer } from 'node:http';
import { extname, join, normalize, sep } from 'node:path';
import { repoRoot } from './lib/toolchain.mjs';

const port = Number(process.env.PORT ?? 8080);
const host = process.env.HOST ?? '127.0.0.1';
const contentTypes = {
  '.html': 'text/html; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.mjs': 'text/javascript; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.map': 'application/json; charset=utf-8',
  '.wasm': 'application/wasm',
  '.png': 'image/png',
  '.jpg': 'image/jpeg',
  '.svg': 'image/svg+xml',
};

createServer((request, response) => {
  const { pathname } = new URL(request.url ?? '/', 'http://localhost');
  let file = normalize(join(repoRoot, decodeURIComponent(pathname)));
  if (file !== repoRoot && !file.startsWith(repoRoot + sep)) {
    response.writeHead(403).end();
    return;
  }
  try {
    if (statSync(file).isDirectory()) file = join(file, 'index.html');
    statSync(file);
  } catch {
    response.writeHead(404, { 'Content-Type': 'text/plain' }).end(`Not found: ${pathname}`);
    return;
  }
  response.writeHead(200, {
    'Content-Type': contentTypes[extname(file)] ?? 'application/octet-stream',
    'Cache-Control': 'no-store',
  });
  createReadStream(file).pipe(response);
}).listen(port, host, () => {
  console.log(`Serving ${repoRoot}\n  http://localhost:${port}/examples/web/`);
});
