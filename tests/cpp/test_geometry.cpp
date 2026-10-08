#include <docscan/geometry.hpp>

#include <gtest/gtest.h>

#include <algorithm>

namespace docscan
{
namespace
{

TEST(OrderCorners, SortsShuffledRectangle)
{
    const Quad expected = {cv::Point2f(10, 20), cv::Point2f(110, 20), cv::Point2f(110, 80),
                           cv::Point2f(10, 80)};
    std::vector<cv::Point2f> shuffled = {expected[2], expected[0], expected[3], expected[1]};

    EXPECT_EQ(orderCorners(shuffled), expected);

    std::reverse(shuffled.begin(), shuffled.end());
    EXPECT_EQ(orderCorners(shuffled), expected);
}

TEST(OrderCorners, HandlesRotatedQuad)
{
    // Rectangle rotated by ~30 degrees: the sum/difference trick fails here, the angle sort works.
    const Quad expected = {cv::Point2f(50, 0), cv::Point2f(137, 50), cv::Point2f(87, 137),
                           cv::Point2f(0, 87)};

    EXPECT_EQ(orderCorners({expected[1], expected[3], expected[0], expected[2]}), expected);
}

TEST(OrderCorners, RequiresFourPoints)
{
    EXPECT_THROW(orderCorners({{0, 0}, {1, 0}, {1, 1}}), Error);
}

TEST(Geometry, QuadAreaAndFullFrame)
{
    const Quad frame = fullFrameQuad({101, 51});

    EXPECT_EQ(frame[0], cv::Point2f(0, 0));
    EXPECT_EQ(frame[2], cv::Point2f(100, 50));
    EXPECT_DOUBLE_EQ(quadArea(frame), 100.0 * 50.0);
}

TEST(Geometry, MaxCornerCosine)
{
    EXPECT_NEAR(maxCornerCosine(fullFrameQuad({100, 100})), 0.0, 1e-9);

    const Quad skewed = {cv::Point2f(0, 0), cv::Point2f(100, 0), cv::Point2f(150, 100), cv::Point2f(50, 100)};
    EXPECT_NEAR(maxCornerCosine(skewed), std::cos(CV_PI / 2 - std::atan2(50.0, 100.0)), 1e-6);
}

} // namespace
} // namespace docscan
