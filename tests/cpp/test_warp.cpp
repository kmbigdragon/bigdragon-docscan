#include "synthetic.hpp"

#include <docscan/geometry.hpp>
#include <docscan/scan.hpp>
#include <docscan/warp.hpp>

#include <gtest/gtest.h>

namespace docscan
{
namespace
{

TEST(Warp, FullFrameIsIdentity)
{
    cv::Mat image(60, 80, CV_8UC3);
    cv::randu(image, cv::Scalar::all(0), cv::Scalar::all(255));

    const cv::Mat output = warpDocument(image, fullFrameQuad(image.size()));

    ASSERT_EQ(output.size(), image.size());
    EXPECT_EQ(cv::norm(output, image, cv::NORM_INF), 0.0);
}

TEST(Warp, KeepsChannelLayout)
{
    const cv::Mat rgba(40, 50, CV_8UC4, cv::Scalar(1, 2, 3, 4));

    const cv::Mat output = warpDocument(rgba, fullFrameQuad(rgba.size()));

    EXPECT_EQ(output.type(), CV_8UC4);
    EXPECT_EQ(output.at<cv::Vec4b>(10, 10), cv::Vec4b(1, 2, 3, 4));
}

TEST(Warp, OutputSizeFollowsCornersAndOptions)
{
    const Quad landscape = {cv::Point2f(0, 0), cv::Point2f(399, 0), cv::Point2f(399, 199), cv::Point2f(0, 199)};
    EXPECT_EQ(computeOutputSize(landscape), cv::Size(400, 200));

    WarpOptions a4;
    a4.aspectRatio = kAspectA4;
    EXPECT_EQ(computeOutputSize(landscape, a4), cv::Size(400, 283)); // orientation kept

    WarpOptions limited;
    limited.maxOutputSize = 100;
    EXPECT_EQ(computeOutputSize(landscape, limited), cv::Size(100, 50));
}

TEST(Warp, RectifiesSyntheticDocument)
{
    const Quad page = {cv::Point2f(180, 120), cv::Point2f(1020, 160), cv::Point2f(1100, 840),
                       cv::Point2f(120, 780)};
    const cv::Mat image = test::renderDocument({1280, 960}, page);

    const cv::Mat output = warpDocument(image, page);

    // Inside a few pixels from the border there must be only paper and text, no background.
    const cv::Mat inner = output(cv::Rect(4, 4, output.cols - 8, output.rows - 8));
    cv::Mat gray;
    cv::cvtColor(inner, gray, cv::COLOR_BGR2GRAY);
    const cv::Rect margin(0, 0, gray.cols, gray.rows / 12); // top band has no text lines
    double minValue = 0;
    cv::minMaxLoc(gray(margin), &minValue);
    EXPECT_GT(minValue, 200.0);
}

TEST(Warp, RejectsInvalidCorners)
{
    const cv::Mat image(100, 100, CV_8UC3, cv::Scalar::all(0));
    const Quad degenerate = {cv::Point2f(10, 10), cv::Point2f(10, 10), cv::Point2f(10, 10), cv::Point2f(10, 10)};
    const Quad outside = {cv::Point2f(500, 500), cv::Point2f(600, 500), cv::Point2f(600, 600),
                          cv::Point2f(500, 600)};

    EXPECT_THROW(warpDocument(image, degenerate), Error);
    EXPECT_THROW(warpDocument(image, outside), Error);
}

TEST(Scan, CropsDetectedDocument)
{
    const Quad page = {cv::Point2f(200, 150), cv::Point2f(1000, 150), cv::Point2f(1000, 750),
                       cv::Point2f(200, 750)};
    const cv::Mat image = test::renderDocument({1280, 960}, page);
    ScanOptions options;
    options.enhance = EnhanceMode::BlackWhite;

    const ScanResult result = scanDocument(image, options);

    ASSERT_TRUE(result.detection.found);
    EXPECT_NEAR(result.image.cols, 801, 8);
    EXPECT_NEAR(result.image.rows, 601, 8);
    EXPECT_EQ(result.image.channels(), 1);
}

} // namespace
} // namespace docscan
