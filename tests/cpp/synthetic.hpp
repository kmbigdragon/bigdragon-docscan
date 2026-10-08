#pragma once

// Synthetic test images with known ground truth, so tests need no fixture files.

#include <docscan/types.hpp>

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <vector>

namespace docscan::test
{

/// Light "page" with dark text lines at `corners`, on a darker, slightly noisy background.
inline cv::Mat renderDocument(cv::Size size, const Quad& corners,
                              const cv::Scalar& paper = cv::Scalar(235, 235, 235),
                              const cv::Scalar& background = cv::Scalar(70, 80, 90))
{
    cv::Mat image(size, CV_8UC3, background);
    cv::Mat noise(size, CV_8UC3);
    cv::randu(noise, cv::Scalar::all(0), cv::Scalar::all(12));
    image += noise;

    std::vector<cv::Point> outline;
    for (const cv::Point2f& corner : corners)
    {
        outline.emplace_back(cvRound(corner.x), cvRound(corner.y));
    }
    cv::fillConvexPoly(image, outline, paper, cv::LINE_AA);

    // Text lines are drawn in page coordinates [0, 1]^2 and projected with the page homography.
    const std::vector<cv::Point2f> unit = {{0.f, 0.f}, {1.f, 0.f}, {1.f, 1.f}, {0.f, 1.f}};
    const cv::Mat homography =
        cv::getPerspectiveTransform(unit, std::vector<cv::Point2f>(corners.begin(), corners.end()));
    const int thickness = std::max(1, size.width / 250);
    int line = 0;
    for (float y = 0.12f; y < 0.9f; y += 0.06f, ++line)
    {
        const float end = line % 4 == 3 ? 0.55f : 0.9f;
        const std::vector<cv::Point2f> segment = {{0.1f, y}, {end, y}};
        std::vector<cv::Point2f> projected;
        cv::perspectiveTransform(segment, projected, homography);
        cv::line(image, projected[0], projected[1], cv::Scalar(40, 40, 40), thickness, cv::LINE_AA);
    }
    return image;
}

} // namespace docscan::test
