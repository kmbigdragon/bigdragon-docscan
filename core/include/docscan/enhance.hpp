#pragma once

#include "docscan/types.hpp"

#include <string_view>

namespace docscan
{

/// Scan filters, as found in CamScanner-like apps.
enum class EnhanceMode
{
    None,       ///< unchanged copy
    Gray,       ///< grayscale (1 channel)
    BlackWhite, ///< shadow-free binarized page, best for text (1 channel)
    MagicColor, ///< white paper, stronger ink, colors kept (3 channels, BGR)
};

/// Applies a scan filter to an already cropped page.
cv::Mat enhanceDocument(const cv::Mat& image, EnhanceMode mode);

/// Parses "none" | "gray" | "bw" | "magic". Throws Error for anything else.
EnhanceMode parseEnhanceMode(std::string_view name);

} // namespace docscan
