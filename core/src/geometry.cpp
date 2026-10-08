#include "docscan/geometry.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace docscan
{

Quad orderCorners(const std::vector<cv::Point2f>& points)
{
    if (points.size() != 4)
    {
        throw Error("orderCorners: expected 4 points, got " + std::to_string(points.size()));
    }

    cv::Point2f center(0.f, 0.f);
    for (const cv::Point2f& point : points)
    {
        center += point;
    }
    center *= 0.25f;

    Quad quad;
    std::copy(points.begin(), points.end(), quad.begin());

    // The y axis points down, so a growing atan2 angle means clockwise on screen.
    std::sort(quad.begin(), quad.end(), [&center](const cv::Point2f& a, const cv::Point2f& b) {
        return std::atan2(a.y - center.y, a.x - center.x) < std::atan2(b.y - center.y, b.x - center.x);
    });

    // Start from the top-left corner: the one with the smallest x + y.
    const auto topLeft = std::min_element(quad.begin(), quad.end(),
                                          [](const cv::Point2f& a, const cv::Point2f& b) {
                                              return a.x + a.y < b.x + b.y;
                                          });
    std::rotate(quad.begin(), topLeft, quad.end());
    return quad;
}

Quad fullFrameQuad(cv::Size size)
{
    const float right = static_cast<float>(std::max(size.width - 1, 0));
    const float bottom = static_cast<float>(std::max(size.height - 1, 0));
    return {cv::Point2f(0.f, 0.f), cv::Point2f(right, 0.f), cv::Point2f(right, bottom),
            cv::Point2f(0.f, bottom)};
}

double quadArea(const Quad& quad)
{
    // Shoelace formula.
    double twiceArea = 0.0;
    for (size_t i = 0; i < quad.size(); ++i)
    {
        const cv::Point2f& a = quad[i];
        const cv::Point2f& b = quad[(i + 1) % quad.size()];
        twiceArea += static_cast<double>(a.x) * b.y - static_cast<double>(b.x) * a.y;
    }
    return std::abs(twiceArea) * 0.5;
}

double maxCornerCosine(const Quad& quad)
{
    double result = 0.0;
    for (size_t i = 0; i < quad.size(); ++i)
    {
        const cv::Point2d corner = quad[i];
        const cv::Point2d toPrevious = cv::Point2d(quad[(i + 3) % 4]) - corner;
        const cv::Point2d toNext = cv::Point2d(quad[(i + 1) % 4]) - corner;
        const double norms = std::sqrt(toPrevious.dot(toPrevious) * toNext.dot(toNext));
        if (norms < 1e-12)
        {
            return 1.0; // two corners coincide
        }
        result = std::max(result, std::abs(toPrevious.dot(toNext)) / norms);
    }
    return result;
}

} // namespace docscan
