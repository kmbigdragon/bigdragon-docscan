#include <metrics.hpp>

#include <gtest/gtest.h>

#include <cmath>

namespace docscan::eval
{
namespace
{

const Quad kSquare = {cv::Point2f(0, 0), cv::Point2f(100, 0), cv::Point2f(100, 100), cv::Point2f(0, 100)};

Quad shifted(const Quad& quad, cv::Point2f offset)
{
    Quad result = quad;
    for (cv::Point2f& corner : result)
    {
        corner += offset;
    }
    return result;
}

TEST(Metrics, PerfectPredictionScoresOne)
{
    const Quad perspective = {cv::Point2f(180, 120), cv::Point2f(1020, 160), cv::Point2f(1100, 840),
                              cv::Point2f(120, 780)};

    const QuadMetrics metrics = compareQuads(perspective, perspective, {2100, 2970});

    EXPECT_NEAR(metrics.jaccard, 1.0, 1e-6);
    EXPECT_NEAR(metrics.meanCornerError, 0.0, 1e-6);
    EXPECT_NEAR(metrics.maxCornerError, 0.0, 1e-6);
}

TEST(Metrics, ShiftedPredictionMatchesHandComputedValues)
{
    // Overlap 90 x 100, union 110 x 100.
    const QuadMetrics metrics = compareQuads(shifted(kSquare, {10, 0}), kSquare, {100, 100});

    EXPECT_NEAR(metrics.jaccard, 9000.0 / 11000.0, 1e-5);
    EXPECT_NEAR(metrics.meanCornerError, 10.0 / std::hypot(100.0, 100.0), 1e-6);
    EXPECT_NEAR(metrics.meanCornerErrorPixels, 10.0, 1e-6);
}

TEST(Metrics, IndependentOfImageScale)
{
    // Same relative error on a document twice as large in the photo: same score.
    Quad large = kSquare;
    for (cv::Point2f& corner : large)
    {
        corner *= 2.f;
    }

    const QuadMetrics small = compareQuads(shifted(kSquare, {10, 0}), kSquare, {210, 297});
    const QuadMetrics big = compareQuads(shifted(large, {20, 0}), large, {210, 297});

    EXPECT_NEAR(small.jaccard, big.jaccard, 1e-6);
    EXPECT_NEAR(small.meanCornerError, big.meanCornerError, 1e-6);
}

TEST(Metrics, InvalidShapesScoreZero)
{
    // Corners 1 and 2 swapped: a self-intersecting "bow tie".
    const Quad bowTie = {kSquare[0], kSquare[2], kSquare[1], kSquare[3]};

    EXPECT_EQ(compareQuads(bowTie, kSquare, {100, 100}).jaccard, 0.0);
}

TEST(Metrics, DisjointPredictionScoresZero)
{
    EXPECT_NEAR(compareQuads(shifted(kSquare, {500, 0}), kSquare, {100, 100}).jaccard, 0.0, 1e-9);
}

} // namespace
} // namespace docscan::eval
