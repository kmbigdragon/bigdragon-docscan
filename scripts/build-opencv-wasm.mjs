#!/usr/bin/env node
// Builds a minimal static OpenCV (core + imgproc) with Emscripten. Run once before `npm run build`.
//
//   node scripts/build-opencv-wasm.mjs [--version 4.13.0] [--no-simd] [--clean]
//
// Result: third_party/opencv/wasm (the CMake package used by the `wasm` preset).
// The exception/SIMD flags below must match the ones in CMakeLists.txt.

import { existsSync, rmSync } from 'node:fs';
import { join } from 'node:path';
import { parseArgs } from 'node:util';
import { findEmscriptenToolchain, repoRoot, run } from './lib/toolchain.mjs';

const { values: options } = parseArgs({
  options: {
    version: { type: 'string', default: process.env.OPENCV_VERSION ?? '4.13.0' },
    'no-simd': { type: 'boolean', default: false },
    clean: { type: 'boolean', default: false },
  },
});

const simd = !options['no-simd'];
const base = join(repoRoot, 'third_party', 'opencv');
const sourceDir = join(base, `src-${options.version}`);
const buildDir = join(base, `build-wasm-${options.version}`);
const installDir = join(base, 'wasm');
const toolchain = findEmscriptenToolchain();

if (options.clean) {
  rmSync(buildDir, { recursive: true, force: true });
  rmSync(installDir, { recursive: true, force: true });
}

if (!existsSync(sourceDir)) {
  run('git', ['clone', '--depth', '1', '--branch', options.version, 'https://github.com/opencv/opencv.git', sourceDir]);
}

const flags = ['-fwasm-exceptions', ...(simd ? ['-msimd128'] : [])].join(' ');

run('cmake', [
  '-S', sourceDir,
  '-B', buildDir,
  '-G', 'Ninja',
  `-DCMAKE_TOOLCHAIN_FILE=${toolchain}`,
  '-DCMAKE_BUILD_TYPE=Release',
  `-DCMAKE_INSTALL_PREFIX=${installDir}`,
  `-DCMAKE_C_FLAGS=${flags}`,
  `-DCMAKE_CXX_FLAGS=${flags}`,
  // Only the modules docscan uses, as static libraries.
  '-DBUILD_LIST=core,imgproc',
  '-DBUILD_SHARED_LIBS=OFF',
  '-DBUILD_ZLIB=ON',
  '-DENABLE_PIC=OFF',
  `-DCV_ENABLE_INTRINSICS=${simd ? 'ON' : 'OFF'}`,
  '-DCPU_BASELINE=',
  '-DCPU_DISPATCH=',
  '-DCV_TRACE=OFF',
  // No threads, accelerators or media/GUI backends inside WebAssembly.
  '-DWITH_PTHREADS_PF=OFF',
  '-DWITH_IPP=OFF',
  '-DWITH_ITT=OFF',
  '-DWITH_OPENCL=OFF',
  '-DWITH_TBB=OFF',
  '-DWITH_OPENMP=OFF',
  '-DWITH_EIGEN=OFF',
  '-DWITH_LAPACK=OFF',
  '-DWITH_PROTOBUF=OFF',
  '-DWITH_ADE=OFF',
  '-DWITH_QUIRC=OFF',
  '-DWITH_FFMPEG=OFF',
  '-DWITH_GSTREAMER=OFF',
  '-DWITH_GTK=OFF',
  '-DWITH_WIN32UI=OFF',
  '-DWITH_JPEG=OFF',
  '-DWITH_PNG=OFF',
  '-DWITH_TIFF=OFF',
  '-DWITH_WEBP=OFF',
  '-DWITH_OPENJPEG=OFF',
  '-DWITH_JASPER=OFF',
  '-DWITH_OPENEXR=OFF',
  '-DWITH_AVIF=OFF',
  '-DBUILD_TESTS=OFF',
  '-DBUILD_PERF_TESTS=OFF',
  '-DBUILD_EXAMPLES=OFF',
  '-DBUILD_DOCS=OFF',
  '-DBUILD_JAVA=OFF',
  '-DBUILD_opencv_apps=OFF',
  '-DOPENCV_GENERATE_PKGCONFIG=OFF',
]);
run('cmake', ['--build', buildDir, '--parallel']);
run('cmake', ['--install', buildDir]);

console.log(`\nOpenCV ${options.version} (WebAssembly${simd ? ', SIMD' : ''}) installed to ${installDir}`);
