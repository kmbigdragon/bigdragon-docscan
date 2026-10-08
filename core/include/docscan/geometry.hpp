#pragma once

#include "docscan/types.hpp"

#include <vector>

namespace docscan
{

/// Orders 4 points as top-left, top-right, bottom-right, bottom-left.
/// Throws Error unless exactly 4 points are given.
Quad orderCorners(const std::vector<cv::Point2f>& points);

/// The whole image as a quad (centres of the 4 corner pixels).
Quad fullFrameQuad(cv::Size size);

/// Area in square pixels (corners must be in order).
double quadArea(const Quad& quad);

/// Largest |cos| of the 4 interior angles: 0 for a rectangle, close to 1 for a very skewed quad.
double maxCornerCosine(const Quad& quad);

} // namespace docscan
