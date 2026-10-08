import createDocScanModule, { type DocScanModule } from './wasm/docscan.mjs';
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
} from './types.js';

export type * from './types.js';

/** Real-world proportions (long side / short side) for `WarpOptions.aspectRatio`. */
export const AspectRatio = Object.freeze({
  /** ISO 216 A-series paper (A3, A4, A5...). */
  A4: 297 / 210,
  /** US Letter. */
  LETTER: 11 / 8.5,
  /** ISO/IEC 7810 ID-1: ID cards, bank cards, driving licences. */
  ID_CARD: 85.6 / 53.98,
});

/**
 * A loaded scanner. All methods are synchronous and CPU-bound: in a browser, call them from a
 * Web Worker to keep the page responsive. Invalid input throws an `Error`.
 */
export class DocScanner {
  readonly #module: DocScanModule;

  private constructor(module: DocScanModule) {
    this.#module = module;
  }

  /** Downloads and instantiates the WebAssembly module. */
  static async create(options: CreateOptions = {}): Promise<DocScanner> {
    return new DocScanner(await createDocScanModule(options));
  }

  /** Version of the native core. */
  get version(): string {
    return this.#module.version();
  }

  /** Finds the document/card outline. Show `corners` to the user so they can adjust them. */
  detect(image: ImageLike, options: DetectOptions = {}): DetectResult {
    return this.#module.detect(image, options);
  }

  /** Cuts out the quad and removes the perspective. */
  warp(image: ImageLike, corners: Quad, options: WarpOptions = {}): RgbaImage {
    return this.#module.warp(image, corners, options);
  }

  /** Applies a scan filter, typically to the output of `warp`. */
  enhance(image: ImageLike, mode: EnhanceMode = 'magic'): RgbaImage {
    return this.#module.enhance(image, mode);
  }

  /** detect → warp → enhance in one call. Keeps the whole frame when nothing is detected. */
  scan(image: ImageLike, options: ScanOptions = {}): ScanResult {
    return this.#module.scan(image, options);
  }
}

/** Shorthand for `DocScanner.create()`. */
export function createDocScanner(options?: CreateOptions): Promise<DocScanner> {
  return DocScanner.create(options);
}
