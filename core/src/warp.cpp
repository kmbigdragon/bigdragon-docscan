#include "docscan/warp.hpp"

#include "docscan/geometry.hpp"
#include "image_utils.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

// TODO: estimate the real aspect ratio from the perspective (Zhang & He, "Whiteboard scanning and
// image enhancement", 2007) instead of using the longest side lengths.

namespace docscan
{
namespace
{

void requireValidOptions(const WarpOptions& options)
{
    if (!(options.aspectRatio >= 0.0) || !std::isfinite(options.aspectRatio))
    {
        throw Error("warpDocument: aspectRatio must be a finite number >= 0");
    }
    if (options.maxOutputSize < 0)
    {
        throw Error("warpDocument: maxOutputSize must be >= 0");
    }
}

void requireValidQuad(const Quad& quad)
{
    for (const cv::Point2f& point : quad)
    {
        if (!std::isfinite(point.x) || !std::isfinite(point.y))
        {
            throw Error("warpDocument: corners must be finite numbers");
        }
    }
    if (quadArea(quad) < 4.0)
    {
        throw Error("warpDocument: the corners do not enclose an area");
    }
}

} // namespace

cv::Size computeOutputSize(const Quad& corners, const WarpOptions& options)
{
    requireValidOptions(options);

    // Corners are pixel centres: a side of length L spans L + 1 pixels.
    const double top = cv::norm(corners[1] - corners[0]);
    const double bottom = cv::norm(corners[2] - corners[3]);
    const double left = cv::norm(corners[3] - corners[0]);
    const double right = cv::norm(corners[2] - corners[1]);
    double width = std::max(top, bottom) + 1.0;
    double height = std::max(left, right) + 1.0;

    if (options.aspectRatio > 0.0)
    {
        // Keep the longer measured side, derive the other one: orientation is preserved.
        const double ratio = options.aspectRatio >= 1.0 ? options.aspectRatio : 1.0 / options.aspectRatio;
        if (width >= height)
        {
            height = width / ratio;
        }
        else
        {
            width = height / ratio;
        }
    }

    const double longest = std::max(width, height);
    if (options.maxOutputSize > 0 && longest > options.maxOutputSize)
    {
        const double shrink = options.maxOutputSize / longest;
        width *= shrink;
        height *= shrink;
    }
    return {std::max(2, static_cast<int>(std::lround(width))),
            std::max(2, static_cast<int>(std::lround(height)))};
}

cv::Mat warpDocument(const cv::Mat& image, const Quad& corners, const WarpOptions& options)
{
    detail::requireImage(image, "warpDocument");
    requireValidQuad(corners);
    const cv::Size outputSize = computeOutputSize(corners, options);

    // Only the bounding box of the quad is needed.
    std::vector<cv::Point2f> source(corners.begin(), corners.end());
    const cv::Rect box = cv::boundingRect(source) & cv::Rect(0, 0, image.cols, image.rows);
    if (box.empty())
    {
        throw Error("warpDocument: the corners lie outside the image");
    }
    cv::Mat region = image(box);
    for (cv::Point2f& point : source)
    {
        point -= cv::Point2f(box.tl());
    }

    // warpPerspective samples without averaging, so a large reduction would alias: shrink the
    // region with area interpolation first.
    WarpOptions unclamped = options;
    unclamped.maxOutputSize = 0;
    const double shrink = static_cast<double>(outputSize.width) / computeOutputSize(corners, unclamped).width;
    if (shrink < 0.75)
    {
        cv::Mat reduced;
        cv::resize(region, reduced, cv::Size(), shrink, shrink, cv::INTER_AREA);
        const cv::Point2f half(0.5f, 0.5f);
        for (cv::Point2f& point : source)
        {
            point = (point + half) * static_cast<float>(shrink) - half;
        }
        region = reduced;
    }

    const float right = static_cast<float>(outputSize.width - 1);
    const float bottom = static_cast<float>(outputSize.height - 1);
    const std::vector<cv::Point2f> target = {{0.f, 0.f}, {right, 0.f}, {right, bottom}, {0.f, bottom}};
    const cv::Mat transform = cv::getPerspectiveTransform(source, target);

    cv::Mat output;
    cv::warpPerspective(region, output, transform, outputSize, cv::INTER_LINEAR, cv::BORDER_REPLICATE);
    return output;
}

} // namespace docscan
