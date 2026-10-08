#include "image_utils.hpp"

#include "docscan/types.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <string>

namespace docscan::detail
{

void requireImage(const cv::Mat& image, const char* caller)
{
    if (image.empty())
    {
        throw Error(std::string(caller) + ": image is empty");
    }
    if (image.depth() != CV_8U)
    {
        throw Error(std::string(caller) + ": only 8-bit images are supported");
    }
    const int channels = image.channels();
    if (channels != 1 && channels != 3 && channels != 4)
    {
        throw Error(std::string(caller) + ": expected 1, 3 or 4 channels, got " +
                    std::to_string(channels));
    }
}

cv::Mat toGray(const cv::Mat& image)
{
    cv::Mat gray;
    switch (image.channels())
    {
    case 1:
        gray = image.clone();
        break;
    case 3:
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
        break;
    case 4:
        cv::cvtColor(image, gray, cv::COLOR_BGRA2GRAY);
        break;
    default:
        throw Error("toGray: unsupported channel count");
    }
    return gray;
}

cv::Mat toBgr(const cv::Mat& image)
{
    cv::Mat bgr;
    switch (image.channels())
    {
    case 1:
        cv::cvtColor(image, bgr, cv::COLOR_GRAY2BGR);
        break;
    case 3:
        bgr = image.clone();
        break;
    case 4:
        cv::cvtColor(image, bgr, cv::COLOR_BGRA2BGR);
        break;
    default:
        throw Error("toBgr: unsupported channel count");
    }
    return bgr;
}

double downscaleFactor(cv::Size size, int maxSide)
{
    const int longest = std::max(size.width, size.height);
    return longest > maxSide ? static_cast<double>(maxSide) / longest : 1.0;
}

int oddAtLeast(int value, int minimum)
{
    const int result = std::max(value, minimum);
    return result % 2 == 0 ? result + 1 : result;
}

} // namespace docscan::detail
