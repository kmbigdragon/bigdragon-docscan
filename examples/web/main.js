// Browser demo: pick a photo -> automatic detection -> drag the corners -> filtered result.
// Runs on the main thread for simplicity; a real app should call docscan from a Web Worker.

import { createDocScanner } from '../../dist/index.js';

const $ = (id) => document.getElementById(id);
const fileInput = $('file');
const modeSelect = $('mode');
const downloadButton = $('download');
const status = $('status');
const source = $('source');
const result = $('result');
const sourceContext = source.getContext('2d', { willReadFrequently: true });
const resultContext = result.getContext('2d');

const MAX_PHOTO_SIDE = 2000; // keep the demo snappy on large camera photos

const scanner = await createDocScanner();
status.textContent = `docscan ${scanner.version} sẵn sàng: chọn một ảnh chụp tài liệu hoặc thẻ.`;

let photo = null; // ImageData of the loaded photo
let corners = null; // current quad, edited by dragging
let dragged = -1; // index of the corner being dragged

fileInput.addEventListener('change', async () => {
  const file = fileInput.files?.[0];
  if (!file) return;

  const bitmap = await createImageBitmap(file, { imageOrientation: 'from-image' });
  const scale = Math.min(1, MAX_PHOTO_SIDE / Math.max(bitmap.width, bitmap.height));
  source.width = Math.round(bitmap.width * scale);
  source.height = Math.round(bitmap.height * scale);
  sourceContext.drawImage(bitmap, 0, 0, source.width, source.height);
  photo = sourceContext.getImageData(0, 0, source.width, source.height);

  const started = performance.now();
  const detection = scanner.detect(photo);
  const elapsed = performance.now() - started;
  corners = detection.corners;
  status.textContent =
    `${detection.found ? 'Đã tìm thấy' : 'Không tìm thấy'} tài liệu ` +
    `(độ tin cậy ${detection.confidence.toFixed(2)}, ${elapsed.toFixed(0)} ms). Kéo các góc để chỉnh.`;
  drawOverlay();
  render();
});

modeSelect.addEventListener('change', render);

downloadButton.addEventListener('click', () => {
  result.toBlob(
    (blob) => {
      const link = document.createElement('a');
      link.href = URL.createObjectURL(blob);
      link.download = 'scan.jpg';
      link.click();
      URL.revokeObjectURL(link.href);
    },
    'image/jpeg',
    0.92,
  );
});

function drawOverlay() {
  sourceContext.putImageData(photo, 0, 0);
  const radius = Math.max(6, Math.max(source.width, source.height) / 90);
  sourceContext.lineWidth = radius / 2;
  sourceContext.strokeStyle = sourceContext.fillStyle = '#22c55e';
  sourceContext.beginPath();
  corners.forEach(({ x, y }, i) => (i === 0 ? sourceContext.moveTo(x, y) : sourceContext.lineTo(x, y)));
  sourceContext.closePath();
  sourceContext.stroke();
  for (const { x, y } of corners) {
    sourceContext.beginPath();
    sourceContext.arc(x, y, radius, 0, 2 * Math.PI);
    sourceContext.fill();
  }
}

function render() {
  if (!photo) return;
  try {
    const started = performance.now();
    const page = scanner.warp(photo, corners, { maxOutputSize: MAX_PHOTO_SIDE });
    const output = scanner.enhance(page, modeSelect.value);
    result.width = output.width;
    result.height = output.height;
    resultContext.putImageData(new ImageData(output.data, output.width, output.height), 0, 0);
    downloadButton.disabled = false;
    console.debug(`warp + enhance: ${(performance.now() - started).toFixed(0)} ms`);
  } catch (error) {
    status.textContent = `Lỗi: ${error.message}`;
  }
}

// ---- Corner dragging ---------------------------------------------------------------------------

function toPhotoCoordinates(event) {
  const box = source.getBoundingClientRect();
  return {
    x: ((event.clientX - box.left) * source.width) / box.width,
    y: ((event.clientY - box.top) * source.height) / box.height,
  };
}

source.addEventListener('pointerdown', (event) => {
  if (!corners) return;
  const pointer = toPhotoCoordinates(event);
  const reach = Math.max(source.width, source.height) / 20;
  dragged = corners.findIndex(({ x, y }) => Math.hypot(x - pointer.x, y - pointer.y) < reach);
  if (dragged >= 0) source.setPointerCapture(event.pointerId);
});

source.addEventListener('pointermove', (event) => {
  if (dragged < 0) return;
  const { x, y } = toPhotoCoordinates(event);
  corners[dragged] = {
    x: Math.min(Math.max(x, 0), source.width - 1),
    y: Math.min(Math.max(y, 0), source.height - 1),
  };
  drawOverlay();
});

source.addEventListener('pointerup', () => {
  if (dragged < 0) return;
  dragged = -1;
  render();
});
