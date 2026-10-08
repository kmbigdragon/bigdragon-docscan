// Types of the Emscripten-generated module (dist/wasm/docscan.mjs).
// Keep in sync with bindings/wasm/docscan_wasm.cpp.

import type {
  CreateOptions,
  DetectOptions,
  DetectResult,
  EnhanceMode,
  ImageLike,
  Quad,
  RgbaImage,
  ScanOptions,
  ScanResult,
  WarpOptions,
} from '../types.js';

export interface DocScanModule {
  version(): string;
  detect(image: ImageLike, options: DetectOptions): DetectResult;
  warp(image: ImageLike, corners: Quad, options: WarpOptions): RgbaImage;
  enhance(image: ImageLike, mode: EnhanceMode): RgbaImage;
  scan(image: ImageLike, options: ScanOptions): ScanResult;
}

declare function createDocScanModule(options?: CreateOptions): Promise<DocScanModule>;
export default createDocScanModule;
