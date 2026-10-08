#include "docscan/enhance.hpp"

#include "image_utils.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace docscan
{
namespace
{

// Estimates the local paper brightness: dilation removes dark strokes (text), the median blur
// smooths what is left. Lighting varies slowly, so a small copy is enough.
cv::Mat estimateBackground(const cv::Mat& channel)
{
    const double scale = detail::downscaleFactor(channel.size(), 512);
    cv::Mat small;
    cv::resize(channel, small, cv::Size(), scale, scale, cv::INTER_AREA);
    cv::dilate(small, small, cv::getStructuringElement(cv::MORPH_ELLIPSE, {7, 7}));
    cv::medianBlur(small, small, 21);
    cv::Mat background;
    cv::resize(small, background, channel.size(), 0, 0, cv::INTER_LINEAR);
    return background;
}

// Divides out the background: the paper becomes uniformly white (no shadows, no uneven lighting)
// while the ink keeps its contrast.
cv::Mat flattenIllumination(const cv::Mat& channel)
{
    cv::Mat flat;
    cv::divide(channel, estimateBackground(channel), flat, 255.0);
    return flat;
}

cv::Mat blackWhite(const cv::Mat& image)
{
    const cv::Mat flat = flattenIllumination(detail::toGray(image));
    const int blockSize = detail::oddAtLeast(std::max(flat.cols, flat.rows) / 50, 15);
    cv::Mat binary;
    cv::adaptiveThreshold(flat, binary, 255, cv::ADAPTIVE_THRESH_MEAN_C, cv::THRESH_BINARY, blockSize, 15);
    return binary;
}

cv::Mat magicColor(const cv::Mat& image)
{
    std::vector<cv::Mat> channels;
    cv::split(detail::toBgr(image), channels);
    for (cv::Mat& channel : channels)
    {
        channel = flattenIllumination(channel);
    }
    cv::Mat result;
    cv::merge(channels, result);

    // Stretch the contrast around white: paper stays at 255, ink gets darker.
    constexpr double kGain = 1.2;
    result.convertTo(result, -1, kGain, 255.0 * (1.0 - kGain));
    return result;
}

} // namespace

cv::Mat enhanceDocument(const cv::Mat& image, EnhanceMode mode)
{
    detail::requireImage(image, "enhanceDocument");
    switch (mode)
    {
    case EnhanceMode::None:
        return image.clone();
    case EnhanceMode::Gray:
        return detail::toGray(image);
    case EnhanceMode::BlackWhite:
        return blackWhite(image);
    case EnhanceMode::MagicColor:
        return magicColor(image);
    }
    throw Error("enhanceDocument: unknown mode");
}

EnhanceMode parseEnhanceMode(std::string_view name)
{
    if (name == "none")
    {
        return EnhanceMode::None;
    }
    if (name == "gray")
    {
        return EnhanceMode::Gray;
    }
    if (name == "bw")
    {
        return EnhanceMode::BlackWhite;
    }
    if (name == "magic")
    {
        return EnhanceMode::MagicColor;
    }
    throw Error("unknown enhance mode '" + std::string(name) + "' (expected none, gray, bw or magic)");
}

} // namespace docscan
