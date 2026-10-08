#!/usr/bin/env node
// ICDAR 2015 SmartDoc Challenge 1 (document page localization in smartphone video frames).
// Downloads the frames (~1 GB, CC BY 4.0), verifies the checksum, extracts them and writes
// data/smartdoc15-ch1/manifest.csv for docscan-bench.
//
//   node scripts/datasets/smartdoc15.mjs
//
// Dataset: https://github.com/jchazalon/smartdoc15-ch1-dataset (Chazalon, Burie et al.)
// Split: the 30 videos of model *001 (one per document type and background, 20 %) are `dev` -
// tune parameters there only; the other 120 videos are `test`, for reporting.

import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { createReadStream, createWriteStream, existsSync, mkdirSync, readdirSync, readFileSync, writeFileSync } from 'node:fs';
import { join, relative } from 'node:path';
import { Readable } from 'node:stream';
import { pipeline } from 'node:stream/promises';
import { gunzipSync } from 'node:zlib';
import { repoRoot } from '../lib/toolchain.mjs';

const release = 'https://github.com/jchazalon/smartdoc15-ch1-dataset/releases/download/v2.0.0';
const dir = join(repoRoot, 'data', 'smartdoc15-ch1');
const archive = join(dir, 'frames.tar.gz');
const framesDir = join(dir, 'frames');

async function download(url, file) {
  console.log(`downloading ${url}`);
  const response = await fetch(url);
  if (!response.ok) throw new Error(`HTTP ${response.status} for ${url}`);
  await pipeline(Readable.fromWeb(response.body), createWriteStream(file));
}

async function sha256(file) {
  const hash = createHash('sha256');
  await pipeline(createReadStream(file), hash);
  return hash.digest('hex');
}

function findFile(root, name) {
  for (const entry of readdirSync(root, { withFileTypes: true })) {
    const path = join(root, entry.name);
    if (entry.isFile() && entry.name === name) return path;
    if (entry.isDirectory()) {
      const found = findFile(path, name);
      if (found) return found;
    }
  }
  return null;
}

mkdirSync(dir, { recursive: true });

// 1. Download + verify.
const checksums = await (await fetch(`${release}/sha256.chksum`)).text();
const expected = checksums.match(/^([0-9a-f]{64})\s+frames\.tar\.gz$/m)?.[1];
if (!expected) throw new Error('frames.tar.gz checksum not found in sha256.chksum');
if (!existsSync(archive) || (await sha256(archive)) !== expected) {
  await download(`${release}/frames.tar.gz`, archive);
  if ((await sha256(archive)) !== expected) throw new Error('checksum mismatch for frames.tar.gz');
}

// 2. Extract (tar ships with Windows 10+, macOS and Linux).
const metadataName = 'metadata.csv.gz';
let metadataPath = existsSync(framesDir) ? findFile(framesDir, metadataName) : null;
if (!metadataPath) {
  mkdirSync(framesDir, { recursive: true });
  console.log('extracting frames.tar.gz ...');
  const tar = spawnSync('tar', ['-xzf', archive, '-C', framesDir], { stdio: 'inherit' });
  if (tar.status !== 0) throw new Error('tar failed');
  metadataPath = findFile(framesDir, metadataName);
  if (!metadataPath) throw new Error(`${metadataName} not found in the archive`);
}

// 3. Manifest.
const [header, ...lines] = gunzipSync(readFileSync(metadataPath)).toString('utf8').trim().split(/\r?\n/);
const columns = header.split(',');
const col = (name) => {
  const index = columns.indexOf(name);
  if (index < 0) throw new Error(`metadata column '${name}' missing (have: ${columns.join(', ')})`);
  return index;
};
const imageColumn = columns.indexOf('image_path');
const metadataDir = join(metadataPath, '..');

const rows = ['image,group,category,split,ref_w,ref_h,tl_x,tl_y,tr_x,tr_y,br_x,br_y,bl_x,bl_y'];
for (const line of lines) {
  const f = line.split(',');
  const background = f[col('bg_name')];
  const model = f[col('model_name')];
  const frame = String(f[col('frame_index')]).padStart(4, '0');
  const image = imageColumn >= 0 ? f[imageColumn] : `${background}/${model}/frame_${frame}.jpeg`;
  rows.push(
    [
      relative(dir, join(metadataDir, image)).replaceAll('\\', '/'),
      `${background}/${model}`,
      background,
      model.endsWith('001') ? 'dev' : 'test',
      f[col('model_width')],
      f[col('model_height')],
      ...['tl', 'tr', 'br', 'bl'].flatMap((c) => [f[col(`${c}_x`)], f[col(`${c}_y`)]]),
    ].join(','),
  );
}
writeFileSync(join(dir, 'manifest.csv'), rows.join('\n') + '\n');
console.log(`wrote ${join(dir, 'manifest.csv')} (${rows.length - 1} frames)`);
