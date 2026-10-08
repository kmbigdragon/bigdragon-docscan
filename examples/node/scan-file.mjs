// Scans an image file with Node.js - the same flow a web backend would use for an upload.
//
//   npm install sharp            (image decoding/encoding; not a dependency of docscan itself)
//   node examples/node/scan-file.mjs <input.jpg|png> [output.jpg] [none|gray|bw|magic]

import { createDocScanner } from '../../dist/index.js';

const [input, output = 'scanned.jpg', enhance = 'magic'] = process.argv.slice(2);
if (!input) {
  console.error('Usage: node examples/node/scan-file.mjs <input> [output.jpg] [none|gray|bw|magic]');
  process.exit(1);
}

const sharp = await import('sharp').then(
  (module) => module.default,
  () => {
    console.error('This example needs sharp: npm install sharp');
    process.exit(1);
  },
);

// Decode to raw RGBA, honouring the EXIF orientation of phone photos.
const { data, info } = await sharp(input).rotate().ensureAlpha().raw().toBuffer({ resolveWithObject: true });

const scanner = await createDocScanner();
const started = performance.now();
const { detection, image } = scanner.scan(
  { width: info.width, height: info.height, data },
  { enhance, warp: { maxOutputSize: 3000 } },
);
const elapsed = performance.now() - started;

await sharp(image.data, { raw: { width: image.width, height: image.height, channels: 4 } })
  .jpeg({ quality: 90 })
  .toFile(output);

console.log(`docscan ${scanner.version}: ${info.width}x${info.height} -> ${image.width}x${image.height} in ${elapsed.toFixed(0)} ms`);
console.log(`found: ${detection.found} (confidence ${detection.confidence.toFixed(2)}), saved ${output}`);
