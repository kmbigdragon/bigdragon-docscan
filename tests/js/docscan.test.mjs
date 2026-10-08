// Tests of the published JavaScript API (dist/), run with `npm test` after `npm run build`.

import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { before, test } from 'node:test';
import { AspectRatio, createDocScanner } from '../../dist/index.js';

/** Light convex quad (the "page") on a dark background, as RGBA pixels. */
function renderDocument(width, height, corners) {
  const data = new Uint8ClampedArray(width * height * 4);
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      const value = isInside(corners, x, y) ? 235 : 60;
      data.set([value, value, value, 255], (y * width + x) * 4);
    }
  }
  return { width, height, data };
}

/** Point-in-convex-polygon test for clockwise (on screen) corners. */
function isInside(corners, x, y) {
  return corners.every((a, i) => {
    const b = corners[(i + 1) % corners.length];
    return (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x) >= 0;
  });
}

function assertCornersNear(actual, expected, tolerance) {
  actual.forEach((corner, i) => {
    assert.ok(Math.abs(corner.x - expected[i].x) <= tolerance, `corner ${i}: x=${corner.x}, expected ${expected[i].x}`);
    assert.ok(Math.abs(corner.y - expected[i].y) <= tolerance, `corner ${i}: y=${corner.y}, expected ${expected[i].y}`);
  });
}

const page = [
  { x: 120, y: 90 },
  { x: 680, y: 120 },
  { x: 720, y: 560 },
  { x: 80, y: 520 },
];
const photo = renderDocument(800, 600, page);
let scanner;

before(async () => {
  scanner = await createDocScanner();
});

test('version matches package.json', () => {
  const { version } = JSON.parse(readFileSync(new URL('../../package.json', import.meta.url), 'utf8'));
  assert.equal(scanner.version, version);
});

test('detect finds the page corners', () => {
  const result = scanner.detect(photo);
  assert.equal(result.found, true);
  assert.ok(result.confidence > 0.7, `confidence ${result.confidence}`);
  assertCornersNear(result.corners, page, 4);
});

test('detect returns the full frame when there is no document', () => {
  const blank = { width: 64, height: 48, data: new Uint8Array(64 * 48 * 4).fill(128) };
  const result = scanner.detect(blank);
  assert.equal(result.found, false);
  assert.deepEqual(result.corners, [
    { x: 0, y: 0 },
    { x: 63, y: 0 },
    { x: 63, y: 47 },
    { x: 0, y: 47 },
  ]);
});

test('warp of the full frame returns the same pixels', () => {
  const data = new Uint8ClampedArray(40 * 30 * 4).map((_, i) => (i * 7) % 256);
  const image = { width: 40, height: 30, data };
  const corners = [
    { x: 0, y: 0 },
    { x: 39, y: 0 },
    { x: 39, y: 29 },
    { x: 0, y: 29 },
  ];
  const output = scanner.warp(image, corners);
  assert.equal(output.width, 40);
  assert.equal(output.height, 30);
  assert.ok(output.data instanceof Uint8ClampedArray);
  assert.deepEqual(output.data, data);
});

test('warp applies aspect ratio and size limit', () => {
  const output = scanner.warp(photo, page, { aspectRatio: AspectRatio.A4, maxOutputSize: 500 });
  assert.equal(output.width, 500); // landscape quad stays landscape
  assert.equal(output.height, Math.round(500 / AspectRatio.A4));
});

test('enhance keeps the size and returns RGBA', () => {
  for (const mode of ['none', 'gray', 'bw', 'magic']) {
    const output = scanner.enhance(photo, mode);
    assert.equal(output.width, photo.width, mode);
    assert.equal(output.height, photo.height, mode);
    assert.equal(output.data.length, photo.width * photo.height * 4, mode);
  }
});

test('scan runs detect, warp and enhance', () => {
  const { detection, image } = scanner.scan(photo, { enhance: 'bw', warp: { maxOutputSize: 400 } });
  assert.equal(detection.found, true);
  assert.equal(Math.max(image.width, image.height), 400);
  const values = new Set(image.data.filter((_, i) => i % 4 === 0));
  assert.ok([...values].every((v) => v === 0 || v === 255), 'black & white output');
});

test('invalid input throws a JS Error with a clear message', () => {
  assert.throws(() => scanner.detect({ width: 10, height: 10, data: new Uint8Array(3) }), {
    name: 'Error',
    message: /width \* height \* 4/,
  });
  assert.throws(() => scanner.warp(photo, [{ x: 0, y: 0 }]), /4 points/);
  assert.throws(() => scanner.enhance(photo, 'sepia'), /unknown enhance mode/);
  assert.throws(() => scanner.detect(photo, { workingSize: 'big' }), /workingSize/);
});
