#pragma once

// Localization metrics used by docscan-bench (and covered by tests/cpp/test_metrics.cpp).

#include <docscan/types.hpp>

namespace docscan::eval
{

struct QuadMetrics
{
    /// Jaccard index (IoU) in the document's reference frame, as in ICDAR 2015 SmartDoc
    /// Challenge 1. 0 when the prediction cannot be compared (crosses the horizon, not convex).
    double jaccard = 0.0;
    /// Mean / worst corner distance in the reference frame, as a fraction of the reference
    /// diagonal (capped at 1).
    double meanCornerError = 1.0;
    double maxCornerError = 1.0;
    /// Mean corner distance in image pixels (no normalisation, for intuition only).
    double meanCornerErrorPixels = 0.0;
};

/// Compares a predicted quad with the ground truth the way the SmartDoc evaluation does: the
/// homography that maps the ground truth onto a `reference` rectangle (the real document size)
/// is applied to the prediction, and both are compared on the document plane. This makes the
/// score independent of the document's size and perspective in the photo.
/// An empty `reference` uses the ground-truth side lengths instead.
QuadMetrics compareQuads(const Quad& predicted, const Quad& truth, cv::Size2d reference = {});

} // namespace docscan::eval
