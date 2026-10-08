/** RGBA pixels, 4 bytes per pixel, row by row: the layout of the browser's `ImageData`. */
export interface ImageLike {
  readonly width: number;
  readonly height: number;
  readonly data: Uint8Array | Uint8ClampedArray;
}

/** Image produced by the library. In a browser: `new ImageData(image.data, image.width, image.height)`. */
export interface RgbaImage {
  width: number;
  height: number;
  data: Uint8ClampedArray;
}

export interface Point {
  x: number;
  y: number;
}

/** Corners in image pixels, ordered top-left, top-right, bottom-right, bottom-left. */
export type Quad = [Point, Point, Point, Point];

export interface DetectOptions {
  /** Longest side (px) of the downscaled copy used for detection. Default 640. */
  workingSize?: number;
  /** Minimum document area relative to the image, in [0, 1). Default 0.1. */
  minAreaRatio?: number;
}

export interface DetectResult {
  found: boolean;
  /** Heuristic score in [0, 1]. */
  confidence: number;
  /** Detected corners, or the full image frame when `found` is false. */
  corners: Quad;
}

export interface WarpOptions {
  /** Long side / short side of the real document, see `AspectRatio`. Default: estimated from the corners. */
  aspectRatio?: number;
  /** Upper bound for the longest output side (px). Default: no limit. */
  maxOutputSize?: number;
}

/** Scan filters: unchanged, grayscale, black & white (text), "magic color" (white paper, vivid ink). */
export type EnhanceMode = 'none' | 'gray' | 'bw' | 'magic';

export interface ScanOptions {
  detect?: DetectOptions;
  warp?: WarpOptions;
  /** Default 'none'. */
  enhance?: EnhanceMode;
}

export interface ScanResult {
  detection: DetectResult;
  image: RgbaImage;
}

export interface CreateOptions {
  /** Where to fetch `docscan.wasm` from (CDN, bundler asset URL...). */
  locateFile?: (path: string, scriptDirectory: string) => string;
  /** The wasm bytes, if you already have them (skips the fetch). */
  wasmBinary?: ArrayBuffer | Uint8Array;
}
