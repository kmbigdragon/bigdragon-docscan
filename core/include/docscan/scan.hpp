#pragma once

#include "docscan/detect.hpp"
#include "docscan/enhance.hpp"
#include "docscan/warp.hpp"

namespace docscan
{

struct ScanOptions
{
    DetectOptions detect;
    WarpOptions warp;
    EnhanceMode enhance = EnhanceMode::None;
};

struct ScanResult
{
    DetectResult detection;
    cv::Mat image;
};

/// detect -> warp -> enhance in one call. When no document is found the whole frame is kept
/// (only maxOutputSize applies).
ScanResult scanDocument(const cv::Mat& image, const ScanOptions& options = {});

} // namespace docscan
