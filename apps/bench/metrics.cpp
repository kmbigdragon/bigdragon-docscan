#include "metrics.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <vector>

namespace docscan::eval
{
namespace
{

// Shoelace area of a simple polygon.
double polygonArea(const std::vector<cv::Point2d>& polygon)
{
    double twiceArea = 0.0;
    for (size_t i = 0; i < polygon.size(); ++i)
    {
        const cv::Point2d& a = polygon[i];
        const cv::Point2d& b = polygon[(i + 1) % polygon.size()];
        twiceArea += a.x * b.y - b.x * a.y;
    }
    return std::abs(twiceArea) * 0.5;
}

// A quadrilateral is convex and simple iff the 4 turns between consecutive edges have the same
// strict sign (a self-intersecting "bow tie" alternates).
bool isConvexQuad(const std::vector<cv::Point2d>& quad)
{
    int positive = 0;
    int negative = 0;
    for (size_t i = 0; i < 4; ++i)
    {
        const cv::Point2d edge = quad[(i + 1) % 4] - quad[i];
        const cv::Point2d nextEdge = quad[(i + 2) % 4] - quad[(i + 1) % 4];
        const double turn = edge.cross(nextEdge);
        positive += turn > 0.0 ? 1 : 0;
        negative += turn < 0.0 ? 1 : 0;
    }
    return positive == 4 || negative == 4;
}

// Sutherland-Hodgman clipping against the rectangle [0, width] x [0, height]. Exact in double
// precision, including the common case of edges lying on the rectangle (where
// cv::intersectConvexConvex is numerically unstable).
std::vector<cv::Point2d> clipToRectangle(std::vector<cv::Point2d> polygon, double width, double height)
{
    // Each half-plane: signed distance of a point to the boundary, >= 0 means inside.
    const std::array<std::function<double(const cv::Point2d&)>, 4> inside = {
        [](const cv::Point2d& p) { return p.x; },
        [width](const cv::Point2d& p) { return width - p.x; },
        [](const cv::Point2d& p) { return p.y; },
        [height](const cv::Point2d& p) { return height - p.y; },
    };
    for (const auto& distance : inside)
    {
        std::vector<cv::Point2d> clipped;
        for (size_t i = 0; i < polygon.size(); ++i)
        {
            const cv::Point2d& current = polygon[i];
            const cv::Point2d& next = polygon[(i + 1) % polygon.size()];
            const double dCurrent = distance(current);
            const double dNext = distance(next);
            if (dCurrent >= 0.0)
            {
                clipped.push_back(current);
            }
            if ((dCurrent >= 0.0) != (dNext >= 0.0))
            {
                clipped.push_back(current + (next - current) * (dCurrent / (dCurrent - dNext)));
            }
        }
        polygon = std::move(clipped);
        if (polygon.empty())
        {
            break;
        }
    }
    return polygon;
}

} // namespace

QuadMetrics compareQuads(const Quad& predicted, const Quad& truth, cv::Size2d reference)
{
    if (reference.width <= 0 || reference.height <= 0)
    {
        reference.width = std::max(cv::norm(truth[1] - truth[0]), cv::norm(truth[2] - truth[3]));
        reference.height = std::max(cv::norm(truth[3] - truth[0]), cv::norm(truth[2] - truth[1]));
    }
    const float width = static_cast<float>(reference.width);
    const float height = static_cast<float>(reference.height);
    const std::vector<cv::Point2f> target = {{0.f, 0.f}, {width, 0.f}, {width, height}, {0.f, height}};

    QuadMetrics metrics;
    for (size_t i = 0; i < 4; ++i)
    {
        metrics.meanCornerErrorPixels += cv::norm(predicted[i] - truth[i]) / 4.0;
    }

    // Project the prediction onto the document plane. A corner with w <= 0 lies behind the
    // camera's horizon: the shape is meaningless there and scores 0.
    const cv::Matx33d homography(
        cv::getPerspectiveTransform(std::vector<cv::Point2f>(truth.begin(), truth.end()), target));
    std::vector<cv::Point2d> projected;
    for (const cv::Point2f& corner : predicted)
    {
        const cv::Vec3d p = homography * cv::Vec3d(corner.x, corner.y, 1.0);
        if (p[2] <= 1e-12)
        {
            return metrics;
        }
        projected.emplace_back(p[0] / p[2], p[1] / p[2]);
    }

    const double diagonal = std::hypot(reference.width, reference.height);
    double errorSum = 0.0;
    double errorMax = 0.0;
    for (size_t i = 0; i < 4; ++i)
    {
        const double error = std::min(1.0, cv::norm(projected[i] - cv::Point2d(target[i])) / diagonal);
        errorSum += error;
        errorMax = std::max(errorMax, error);
    }
    metrics.meanCornerError = errorSum / 4.0;
    metrics.maxCornerError = errorMax;

    // Self-intersecting / concave predictions are not valid page outlines (the official
    // evaluation rejects self-intersecting shapes too).
    if (!isConvexQuad(projected))
    {
        return metrics;
    }
    const double intersectionArea = polygonArea(clipToRectangle(projected, reference.width, reference.height));
    const double predictedArea = polygonArea(projected);
    const double referenceArea = reference.width * reference.height;
    const double unionArea = predictedArea + referenceArea - intersectionArea;
    metrics.jaccard = unionArea > 0.0 ? std::clamp(intersectionArea / unionArea, 0.0, 1.0) : 0.0;
    return metrics;
}

} // namespace docscan::eval
