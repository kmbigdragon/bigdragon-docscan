#pragma once

#include "docscan/types.hpp"

namespace docscan
{

struct DetectOptions
{
    /// Detection runs on a copy whose longest side is at most this many pixels.
    /// Smaller is faster, larger gives more precise corners.
    int workingSize = 640;
    /// Minimum document area relative to the image area, in [0, 1). Pages in SmartDoc 2015 cover
    /// only 10-16 % of the frame; 0.05 was chosen on its dev split (see docs/EVALUATION.md).
    double minAreaRatio = 0.05;
};

struct DetectResult
{
    bool found = false;
    /// Corners in input-image pixels. When nothing is found this is the full image frame, so a
    /// UI can still show handles for manual adjustment.
    Quad corners{};
    /// Heuristic score in [0, 1]: edge support, rectangularity and size of the outline.
    double confidence = 0.0;
};

/// Finds the most likely document or card outline in a photo.
DetectResult detectDocument(const cv::Mat& image, const DetectOptions& options = {});

} // namespace docscan
