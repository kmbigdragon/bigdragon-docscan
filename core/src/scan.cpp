#include "docscan/scan.hpp"

namespace docscan
{

ScanResult scanDocument(const cv::Mat& image, const ScanOptions& options)
{
    ScanResult result;
    result.detection = detectDocument(image, options.detect);

    // Without a detection the corners are the full frame: keep the photo's own proportions.
    WarpOptions warpOptions = options.warp;
    if (!result.detection.found)
    {
        warpOptions.aspectRatio = 0.0;
    }
    const cv::Mat page = warpDocument(image, result.detection.corners, warpOptions);
    result.image = enhanceDocument(page, options.enhance);
    return result;
}

} // namespace docscan
