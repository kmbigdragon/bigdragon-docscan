#pragma once

#include <opencv2/core.hpp>

#include <array>
#include <stdexcept>

namespace docscan
{

/// Four document corners in image pixels, always ordered top-left, top-right, bottom-right,
/// bottom-left (clockwise on screen).
using Quad = std::array<cv::Point2f, 4>;

/// Thrown for invalid input: empty image, unsupported pixel format, degenerate quad, bad option...
class Error : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

} // namespace docscan
