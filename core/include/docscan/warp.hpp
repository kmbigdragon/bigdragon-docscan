#pragma once

#include "docscan/types.hpp"

namespace docscan
{

/// Real-world proportions (long side / short side) for WarpOptions::aspectRatio.
inline constexpr double kAspectA4 = 297.0 / 210.0;       ///< ISO 216 A-series paper
inline constexpr double kAspectLetter = 11.0 / 8.5;      ///< US Letter
inline constexpr double kAspectIdCard = 85.60 / 53.98;   ///< ISO/IEC 7810 ID-1 (ID, bank cards)

struct WarpOptions
{
    /// Long side / short side of the real document (e.g. kAspectIdCard). 0 = derive from the
    /// corner distances. The output keeps the orientation (portrait/landscape) of the quad.
    double aspectRatio = 0.0;
    /// Upper bound for the longest output side in pixels. 0 = no limit.
    int maxOutputSize = 0;
};

/// Output size warpDocument() will produce for these corners.
cv::Size computeOutputSize(const Quad& corners, const WarpOptions& options = {});

/// Cuts the quad out of the image and removes the perspective. Works on any channel layout
/// (the output has the same type as the input). Corners may lie partly outside the image.
cv::Mat warpDocument(const cv::Mat& image, const Quad& corners, const WarpOptions& options = {});

} // namespace docscan
