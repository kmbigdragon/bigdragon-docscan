#pragma once

// Internal helpers shared by the core implementation (not part of the public API).

#include <opencv2/core.hpp>

namespace docscan::detail
{

/// Throws docscan::Error unless `image` is a non-empty 8-bit image with 1, 3 or 4 channels.
void requireImage(const cv::Mat& image, const char* caller);

/// New 1-channel copy of a GRAY / BGR / BGRA image.
cv::Mat toGray(const cv::Mat& image);

/// New 3-channel BGR copy of a GRAY / BGR / BGRA image.
cv::Mat toBgr(const cv::Mat& image);

/// Factor (<= 1) that brings the longest side of `size` down to `maxSide`.
double downscaleFactor(cv::Size size, int maxSide);

/// Smallest odd number >= max(value, minimum).
int oddAtLeast(int value, int minimum);

} // namespace docscan::detail
