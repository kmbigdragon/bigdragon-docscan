# docscan

[![CI](https://github.com/kmbigdragon/bigdragon-docscan/actions/workflows/ci.yml/badge.svg)](https://github.com/kmbigdragon/bigdragon-docscan/actions/workflows/ci.yml)

Thư viện quét tài liệu/thẻ kiểu CamScanner: **tự phát hiện viền → cắt & nắn phối cảnh → bộ lọc scan**.
Lõi viết bằng C++17 + OpenCV, biên dịch sang **WebAssembly** để dùng được ở cả **trình duyệt** và **Node.js**
qua một API JavaScript/TypeScript duy nhất.

```js
import { createDocScanner, AspectRatio } from 'docscan';

const scanner = await createDocScanner();
const { found, corners } = scanner.detect(imageData);            // 4 góc: TL, TR, BR, BL
const page = scanner.warp(imageData, corners, { aspectRatio: AspectRatio.ID_CARD });
const scan = scanner.enhance(page, 'magic');                     // 'none' | 'gray' | 'bw' | 'magic'
// hoặc gộp cả 3 bước: scanner.scan(imageData, { enhance: 'bw' })
```

## Cài đặt

Mỗi bản phát hành trên [GitHub Releases](https://github.com/kmbigdragon/bigdragon-docscan/releases) có sẵn tarball npm:

```bash
npm install https://github.com/kmbigdragon/bigdragon-docscan/releases/download/v0.1.0/docscan-0.1.0.tgz
```

Quy trình CI/CD và cách phát hành phiên bản mới: [docs/RELEASING.md](docs/RELEASING.md).

## Kiến trúc

```
 C++ core (core/)           ← toàn bộ thuật toán, chỉ phụ thuộc OpenCV core + imgproc
   │
   ├── bindings/wasm/       ← Embind: chuyển ImageData ⇄ cv::Mat, lỗi C++ → JS Error
   │      └─> dist/wasm/docscan.mjs + docscan.wasm   (Emscripten)
   │             └─> js/index.ts → dist/index.js     (API TypeScript công khai)
   │
   ├── apps/cli/            ← docscan-cli: chạy pipeline trên file ảnh (native, để tinh chỉnh thuật toán)
   └── tests/cpp/           ← unit test GoogleTest (native, ảnh tổng hợp có ground truth)
```

| Thư mục | Nội dung |
| --- | --- |
| `core/include/docscan/` | API C++ công khai: `detect.hpp`, `warp.hpp`, `enhance.hpp`, `scan.hpp`, `geometry.hpp` |
| `core/src/` | Cài đặt thuật toán (`detect.cpp` là phần quan trọng nhất) |
| `bindings/wasm/` | Lớp Embind và cờ link Emscripten |
| `js/` | Wrapper TypeScript (`index.ts`, `types.ts`) + khai báo kiểu cho module wasm |
| `apps/cli/` | Công cụ dòng lệnh native |
| `tests/cpp/`, `tests/js/` | Test C++ (GoogleTest) và test API JS (`node:test`) |
| `examples/web/`, `examples/node/` | Demo trình duyệt (kéo góc để chỉnh) và script Node (dùng `sharp`) |
| `scripts/` | Build OpenCV cho wasm, build module wasm, server tĩnh cho demo |

Quy ước: lõi C++ nhận ảnh 8-bit 1/3/4 kênh theo thứ tự **BGR(A)** của OpenCV; phía JS luôn là **RGBA**
(giống `ImageData`). Binding lo việc chuyển đổi.

## Yêu cầu

- CMake ≥ 3.24, Ninja
- Node.js ≥ 22 (để build/test; thư viện đã build chạy được trên Node ≥ 18 và trình duyệt hiện đại)
- [Emscripten](https://emscripten.org/docs/getting_started/downloads.html) (`emcc` trên PATH hoặc biến `EMSDK`)
- Cho bản native: trình biên dịch C++17 + OpenCV 4.x. Trên Windows dùng MSYS2 UCRT64:
  `pacman -S mingw-w64-ucrt-x86_64-{gcc,opencv}`. Trên Linux: `apt install libopencv-dev`.

## Bắt đầu

```bash
npm install
npm run opencv:wasm     # MỘT LẦN: build OpenCV (core + imgproc) cho wasm → third_party/opencv/wasm
npm run build           # build dist/wasm/* rồi compile TypeScript → dist/
npm test                # test API JS trên Node
npm run serve           # mở http://localhost:8080/examples/web/
```

`npm run opencv:wasm -- --version 4.14.0` để đổi phiên bản OpenCV, `--no-simd` để bỏ SIMD
(khi đó phải configure với `-DDOCSCAN_WASM_SIMD=OFF`), `--clean` để build lại từ đầu.

### Phát triển thuật toán (native)

Vòng lặp nhanh nhất để tinh chỉnh thuật toán: sửa `core/src/*.cpp` → build native → chạy test/CLI.

```bash
cmake --preset native            # Debug; dùng native-release để đo tốc độ
cmake --build --preset native
ctest --preset native

./build/native/apps/cli/docscan-cli photo.jpg out.png --enhance magic --debug outline.jpg
./build/native/apps/cli/docscan-cli card.jpg card.png --aspect card --max-size 1600
```

`--debug` lưu ảnh gốc có vẽ khung phát hiện (xanh = tìm thấy, đỏ = không tìm thấy).

### Dùng trong Node.js

Wasm không giải mã JPEG/PNG (để giữ module nhỏ), nên ở Node hãy giải mã bằng `sharp`
(xem `examples/node/scan-file.mjs`):

```js
const { data, info } = await sharp(input).rotate().ensureAlpha().raw().toBuffer({ resolveWithObject: true });
const { image } = scanner.scan({ width: info.width, height: info.height, data }, { enhance: 'magic' });
await sharp(image.data, { raw: { width: image.width, height: image.height, channels: 4 } }).jpeg().toFile(out);
```

### Dùng trong trình duyệt

Lấy `ImageData` từ canvas, truyền thẳng vào các hàm; kết quả hiển thị bằng
`ctx.putImageData(new ImageData(out.data, out.width, out.height), 0, 0)`.
Nếu bundler không tự copy file wasm, dùng `createDocScanner({ locateFile: () => urlCuaDocscanWasm })`
(file được export tại `docscan/docscan.wasm`).

### Dùng với React (Vite)

Đã kiểm chứng với Vite 8 + React 19 (dev, build, Web Worker); Vite tự đóng gói `docscan.wasm` thành asset.
Cảnh báo `Module "node:module" has been externalized` khi build là vô hại (nhánh đó chỉ chạy trên Node).

```bash
npm install ../docscan          # khi phát triển: link tới thư mục (cần `npm run build` trong docscan trước)
# hoặc: npm pack trong docscan, rồi npm install ./docscan-0.1.0.tgz
```

Khi link bằng thư mục, Vite dev server chặn file ngoài dự án (lỗi 403 khi tải `docscan.wasm`):

```ts
// vite.config.ts
import { defineConfig, searchForWorkspaceRoot } from 'vite';
export default defineConfig({
  server: { fs: { allow: [searchForWorkspaceRoot(process.cwd()), '../docscan'] } },
});
```

Tải module một lần cho cả app (StrictMode chạy effect hai lần):

```tsx
import { useEffect, useState } from 'react';
import { createDocScanner, type DocScanner } from 'docscan';

let scannerPromise: Promise<DocScanner> | undefined;
const loadDocScanner = () => (scannerPromise ??= createDocScanner());

export function useDocScanner() {
  const [scanner, setScanner] = useState<DocScanner | null>(null);
  useEffect(() => {
    let active = true;
    loadDocScanner().then((s) => active && setScanner(s));
    return () => { active = false; };
  }, []);
  return scanner;
}

// Trong component: ImageData lấy từ canvas
const { detection, image } = scanner.scan(imageData, { enhance: 'magic' });
ctx.putImageData(new ImageData(image.data, image.width, image.height), 0, 0);
```

Ảnh camera lớn (12MP) mất cỡ 1 giây, nên chạy trong Web Worker để không đơ giao diện:

```ts
// docscan.worker.ts
import { createDocScanner, type ScanOptions } from 'docscan';
const scannerPromise = createDocScanner();
self.onmessage = async (e: MessageEvent<{ id: number; photo: ImageData; options: ScanOptions }>) => {
  const { id, photo, options } = e.data;
  try {
    const result = (await scannerPromise).scan(photo, options);
    self.postMessage({ id, result }, { transfer: [result.image.data.buffer] });
  } catch (error) {
    self.postMessage({ id, error: (error as Error).message });
  }
};

// phía component
const worker = new Worker(new URL('./docscan.worker.ts', import.meta.url), { type: 'module' });
```

Next.js (chưa kiểm chứng): chỉ gọi docscan ở client (`'use client'`, tải trong `useEffect`), không gọi khi SSR.

## Thuật toán hiện tại (baseline)

1. **Detect** (`core/src/detect.cpp`): thu nhỏ về 640px → xám → *closing* để xoá chữ trong trang →
   3 bản đồ nhị phân (Canny mạnh, Canny yếu, Otsu) → với các contour lớn nhất: convex hull →
   xấp xỉ 4 đỉnh → **tinh chỉnh bằng fit đường thẳng từng cạnh** (khôi phục góc nhọn của thẻ bo góc) →
   chấm điểm theo *độ phủ cạnh*, *độ vuông góc*, *diện tích* → chọn tứ giác tốt nhất.
2. **Warp** (`warp.cpp`): kích thước ra theo cạnh dài nhất (hoặc theo `aspectRatio`), thu nhỏ trước bằng
   `INTER_AREA` khi giảm mạnh để tránh răng cưa, rồi `warpPerspective`.
3. **Enhance** (`enhance.cpp`): ước lượng nền giấy (dilate + median trên ảnh nhỏ) rồi chia nền để xoá
   bóng đổ; `bw` = adaptive threshold, `magic` = làm phẳng từng kênh màu + tăng tương phản quanh màu trắng.

## Đánh giá định lượng

Chất lượng phát hiện được đo trên bộ chuẩn ICDAR 2015 SmartDoc (24 889 frame) theo đúng giao thức
Jaccard index của cuộc thi, có khoảng tin cậy bootstrap và so sánh ghép cặp giữa các phiên bản.
Hiện tại: **mean JI 0.869** (nền 1–3 ≈ 0.97). Cách chạy, chỉ số, kết quả và nhật ký thí nghiệm:
[docs/EVALUATION.md](docs/EVALUATION.md).

```bash
npm run dataset:smartdoc15
build/native-release/apps/bench/docscan-bench data/smartdoc15-ch1/manifest.csv bench-results/run.csv
npm run eval:report -- bench-results/run.csv --baseline bench-results/baseline.csv
```

## Hướng phát triển (roadmap)

- [ ] Tinh chỉnh góc ở độ phân giải gốc (hiện góc tìm ở ảnh 640px rồi phóng lên).
- [ ] Ước lượng tỉ lệ thật từ phối cảnh (Zhang & He 2007) thay vì dùng cạnh dài nhất.
- [ ] Phát hiện bằng đường Hough khi một góc bị che hoặc nằm ngoài khung hình.
- [ ] Bộ phát hiện học máy (segmentation ONNX qua `cv::dnn`) cho nền phức tạp.
- [ ] Tự xoay theo hướng chữ (0/90/180/270), phát hiện nhiều tài liệu trong một ảnh.
- [ ] Web Worker wrapper; tuỳ chọn bản wasm đa luồng (pthreads, cần header COOP/COEP).
- [ ] Phát hiện liên tục trên luồng camera (giữ ổn định góc giữa các khung hình).
- [ ] Nếu server cần throughput cao: thêm `bindings/node` (N-API) dùng chung `core/`.
- [ ] CI (GitHub Actions) build native + wasm, publish npm.

## Ghi chú kỹ thuật

- **Exceptions**: cả OpenCV lẫn docscan đều biên dịch với `-fwasm-exceptions`; nếu đổi cờ này phải
  build lại OpenCV (`npm run opencv:wasm -- --clean`).
- **Bộ nhớ**: mỗi lời gọi copy ảnh vào heap wasm một lần và copy kết quả ra một lần; JS không phải tự
  giải phóng gì. Heap tự tăng (`ALLOW_MEMORY_GROWTH`), tối đa 2 GB.
- **Phiên bản**: lấy từ `package.json` (CMake đọc file này), hiển thị qua `scanner.version` / `docscan::version()`.
- **Tên gói npm**: `docscan` có thể đã bị dùng; nên đổi sang dạng scoped (`@ten-ban/docscan`) trước khi publish.
