#pragma once

/// docscan - detect, crop and enhance documents and cards in photos.
///
/// Pixel format: 8-bit images with 1 (gray), 3 (BGR) or 4 (BGRA) channels, i.e. OpenCV's default
/// channel order. Every function throws docscan::Error on invalid input.
///
/// Typical flow of a CamScanner-like UI:
///   1. detectDocument()  -> corners (show them, let the user drag them)
///   2. warpDocument()    -> cropped, perspective-corrected page
///   3. enhanceDocument() -> scan filter (gray, black & white, magic color)
/// or scanDocument() for all three in one call.

#include "docscan/detect.hpp"
#include "docscan/enhance.hpp"
#include "docscan/geometry.hpp"
#include "docscan/scan.hpp"
#include "docscan/types.hpp"
#include "docscan/version.hpp"
#include "docscan/warp.hpp"
