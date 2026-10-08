#include "synthetic.hpp"

#include <docscan/detect.hpp>
#include <docscan/geometry.hpp>

#include <gtest/gtest.h>

#include <opencv2/imgproc.hpp>

namespace docscan
{
namespace
{

constexpr float kTolerance = 6.f; // pixels, on 1280x960 inputs

void expectCornersNear(const Quad& actual, const Quad& expected, float tolerance = kTolerance)
{
    for (size_t i = 0; i < 4; ++i)
    {
        EXPECT_NEAR(actual[i].x, expected[i].x, tolerance) << "corner " << i;
        EXPECT_NEAR(actual[i].y, expected[i].y, tolerance) << "corner " << i;
    }
}

// Dark card with rounded corners on a light table, rotated by `angle` degrees.
// `truth` receives the virtual sharp corners of the card.
cv::Mat renderRoundedCard(cv::Size size, const cv::Rect& card, int radius, double angle, Quad& truth)
{
    cv::Mat flat(size, CV_8UC3, cv::Scalar(225, 225, 220));
    const cv::Scalar color(120, 60, 30);
    cv::rectangle(flat, cv::Rect(card.x + radius, card.y, card.width - 2 * radius, card.height), color,
                  cv::FILLED);
    cv::rectangle(flat, cv::Rect(card.x, card.y + radius, card.width, card.height - 2 * radius), color,
                  cv::FILLED);
    const int right = card.x + card.width - 1 - radius;
    const int bottom = card.y + card.height - 1 - radius;
    for (const cv::Point& center : {cv::Point(card.x + radius, card.y + radius), cv::Point(right, card.y + radius),
                                    cv::Point(right, bottom), cv::Point(card.x + radius, bottom)})
    {
        cv::circle(flat, center, radius, color, cv::FILLED, cv::LINE_AA);
    }

    const cv::Mat rotation =
        cv::getRotationMatrix2D(cv::Point2f(size.width / 2.f, size.height / 2.f), angle, 1.0);
    cv::Mat image;
    cv::warpAffine(flat, image, rotation, size, cv::INTER_LINEAR, cv::BORDER_REPLICATE);

    const float x0 = static_cast<float>(card.x);
    const float y0 = static_cast<float>(card.y);
    const float x1 = static_cast<float>(card.x + card.width - 1);
    const float y1 = static_cast<float>(card.y + card.height - 1);
    std::vector<cv::Point2f> corners = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
    cv::transform(corners, corners, rotation);
    truth = orderCorners(corners);
    return image;
}

TEST(Detect, FindsDocumentInPerspective)
{
    const Quad truth = {cv::Point2f(180, 120), cv::Point2f(1020, 160), cv::Point2f(1100, 840),
                        cv::Point2f(120, 780)};
    const cv::Mat image = test::renderDocument({1280, 960}, truth);

    const DetectResult result = detectDocument(image);

    ASSERT_TRUE(result.found);
    EXPECT_GT(result.confidence, 0.7);
    expectCornersNear(result.corners, truth);
}

TEST(Detect, FindsRotatedCardWithRoundedCorners)
{
    Quad truth;
    const cv::Mat image = renderRoundedCard({1280, 960}, cv::Rect(340, 280, 600, 378), 30, 12.0, truth);

    const DetectResult result = detectDocument(image);

    ASSERT_TRUE(result.found);
    expectCornersNear(result.corners, truth);
}

TEST(Detect, AcceptsGrayAndBgraInput)
{
    const Quad truth = {cv::Point2f(200, 150), cv::Point2f(1000, 150), cv::Point2f(1000, 800),
                        cv::Point2f(200, 800)};
    const cv::Mat bgr = test::renderDocument({1280, 960}, truth);
    cv::Mat gray;
    cv::Mat bgra;
    cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);
    cv::cvtColor(bgr, bgra, cv::COLOR_BGR2BGRA);

    for (const cv::Mat& image : {gray, bgra})
    {
        const DetectResult result = detectDocument(image);
        ASSERT_TRUE(result.found) << "channels: " << image.channels();
        expectCornersNear(result.corners, truth);
    }
}

TEST(Detect, ReturnsFullFrameWhenNothingIsFound)
{
    const cv::Mat image(480, 640, CV_8UC3, cv::Scalar(128, 128, 128));

    const DetectResult result = detectDocument(image);

    EXPECT_FALSE(result.found);
    EXPECT_EQ(result.confidence, 0.0);
    EXPECT_EQ(result.corners, fullFrameQuad(image.size()));
}

TEST(Detect, RejectsInvalidInput)
{
    EXPECT_THROW(detectDocument(cv::Mat()), Error);
    EXPECT_THROW(detectDocument(cv::Mat(100, 100, CV_32FC1, cv::Scalar(0))), Error);

    DetectOptions options;
    options.workingSize = 10;
    EXPECT_THROW(detectDocument(cv::Mat(100, 100, CV_8UC1, cv::Scalar(0)), options), Error);
}

} // namespace
} // namespace docscan
