#include "docscan/detect.hpp"

#include "docscan/geometry.hpp"
#include "image_utils.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

// Baseline pipeline (classic CV, no learning):
//   1. downscale, grayscale, morphological closing to erase text inside the page
//   2. build several binary maps (strong / weak Canny edges, Otsu mask)
//   3. for the largest contours of each map: convex hull -> 4-vertex polygon -> line-fit refinement
//   4. keep quads that are big, roughly rectangular and lie on real edges; return the best one
//
// Extension points (see README "Roadmap"): sub-pixel refinement at full resolution, Hough-line
// based detection for partly hidden corners, a learned detector (e.g. ONNX via cv::dnn).

namespace docscan
{
namespace
{

constexpr size_t kMaxContoursPerMap = 5; // only the largest contours can be the document
constexpr double kMaxCornerCosine = 0.6; // reject quads with angles outside ~53..127 degrees
constexpr double kMinEdgeSupport = 0.5;  // share of the outline that must lie on edge pixels

using Contour = std::vector<cv::Point>;

struct EdgeMaps
{
    std::vector<cv::Mat> sources; // binary maps whose contours are document candidates
    cv::Mat evidence;             // edge pixels used to verify the sides of a candidate
};

struct Candidate
{
    Quad quad;
    double score = 0.0;
};

EdgeMaps buildEdgeMaps(const cv::Mat& gray)
{
    // Closing (dilate, then erode) wipes out dark text and thin texture, so the paper becomes a
    // flat region and mostly its outline produces edges.
    const int kernelSize = detail::oddAtLeast(std::max(gray.cols, gray.rows) / 70, 3);
    cv::Mat smooth;
    cv::morphologyEx(gray, smooth, cv::MORPH_CLOSE,
                     cv::getStructuringElement(cv::MORPH_RECT, {kernelSize, kernelSize}));
    cv::GaussianBlur(smooth, smooth, {5, 5}, 0);

    // Otsu separates paper from background directly and gives Canny a contrast-adaptive base.
    cv::Mat mask;
    const double otsu = cv::threshold(smooth, mask, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    cv::Mat strong;
    cv::Mat weak;
    cv::Canny(smooth, strong, std::max(20.0, 0.5 * otsu), std::max(40.0, otsu));
    cv::Canny(smooth, weak, std::max(10.0, 0.25 * otsu), std::max(20.0, 0.5 * otsu));

    // Thicken the edges so small gaps do not split the outline into several contours.
    const cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, {3, 3});
    cv::dilate(strong, strong, kernel);
    cv::dilate(weak, weak, kernel);

    EdgeMaps maps;
    cv::dilate(weak, maps.evidence, kernel);
    maps.sources = {strong, weak, mask};
    return maps;
}

// Approximates a convex hull by 4 vertices, loosening the tolerance until it fits.
std::optional<Quad> approximateQuad(const Contour& hull)
{
    const double perimeter = cv::arcLength(hull, true);
    Contour polygon;
    for (const double epsilon : {0.02, 0.03, 0.05, 0.08})
    {
        cv::approxPolyDP(hull, polygon, epsilon * perimeter, true);
        if (polygon.size() <= 4)
        {
            break;
        }
    }
    if (polygon.size() != 4 || !cv::isContourConvex(polygon))
    {
        return std::nullopt;
    }
    return orderCorners(std::vector<cv::Point2f>(polygon.begin(), polygon.end()));
}

// Intersection of two lines in cv::fitLine format (vx, vy, x0, y0).
std::optional<cv::Point2f> intersectLines(const cv::Vec4f& a, const cv::Vec4f& b)
{
    const cv::Point2f directionA(a[0], a[1]);
    const cv::Point2f directionB(b[0], b[1]);
    const float cross = directionA.cross(directionB);
    if (std::abs(cross) < 1e-3f)
    {
        return std::nullopt; // (nearly) parallel
    }
    const cv::Point2f originA(a[2], a[3]);
    const cv::Point2f originB(b[2], b[3]);
    const float t = (originB - originA).cross(directionB) / cross;
    return originA + directionA * t;
}

// Re-estimates the corners by fitting a line to the contour points along each side and
// intersecting neighbouring sides. This recovers the sharp corners of cards with rounded corners
// and averages out pixel noise. Returns `quad` unchanged when the fit is not reliable.
Quad refineByLineFit(const Quad& quad, const Contour& contour)
{
    double perimeter = 0.0;
    for (size_t i = 0; i < 4; ++i)
    {
        perimeter += cv::norm(quad[(i + 1) % 4] - quad[i]);
    }
    const double maxDistance = std::max(2.0, 0.01 * perimeter);
    constexpr double kCornerMargin = 0.1; // skip the (possibly rounded) ends of each side

    std::array<std::vector<cv::Point2f>, 4> sidePoints;
    for (const cv::Point& contourPoint : contour)
    {
        const cv::Point2f point(contourPoint);
        int bestSide = -1;
        double bestDistance = maxDistance;
        for (int side = 0; side < 4; ++side)
        {
            const cv::Point2f start = quad[side];
            const cv::Point2f along = quad[(side + 1) % 4] - start;
            const double lengthSquared = along.dot(along);
            if (lengthSquared < 1.0)
            {
                continue;
            }
            const double t = (point - start).dot(along) / lengthSquared;
            if (t < kCornerMargin || t > 1.0 - kCornerMargin)
            {
                continue;
            }
            const double distance = std::abs(along.cross(point - start)) / std::sqrt(lengthSquared);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestSide = side;
            }
        }
        if (bestSide >= 0)
        {
            sidePoints[bestSide].push_back(point);
        }
    }

    std::array<cv::Vec4f, 4> lines;
    for (int side = 0; side < 4; ++side)
    {
        if (sidePoints[side].size() < 8)
        {
            return quad;
        }
        cv::fitLine(sidePoints[side], lines[side], cv::DIST_HUBER, 0, 0.01, 0.01);
    }

    Quad refined;
    for (int corner = 0; corner < 4; ++corner)
    {
        // Corner i joins side i-1 (coming from corner i-1) and side i (going to corner i+1).
        const std::optional<cv::Point2f> point = intersectLines(lines[(corner + 3) % 4], lines[corner]);
        if (!point || cv::norm(*point - quad[corner]) > 0.05 * perimeter)
        {
            return quad;
        }
        refined[corner] = *point;
    }
    return refined;
}

// Fraction of points sampled along the outline that fall on edge pixels.
double edgeSupport(const Quad& quad, const cv::Mat& evidence)
{
    int hits = 0;
    int samples = 0;
    for (size_t side = 0; side < 4; ++side)
    {
        const cv::Point2f start = quad[side];
        const cv::Point2f along = quad[(side + 1) % 4] - start;
        const int steps = std::max(1, cvRound(cv::norm(along) / 2.0));
        for (int step = 0; step < steps; ++step)
        {
            const cv::Point2f point = start + along * (static_cast<float>(step) / steps);
            const int x = cvRound(point.x);
            const int y = cvRound(point.y);
            ++samples;
            if (x >= 0 && y >= 0 && x < evidence.cols && y < evidence.rows &&
                evidence.at<uchar>(y, x) != 0)
            {
                ++hits;
            }
        }
    }
    return samples > 0 ? static_cast<double>(hits) / samples : 0.0;
}

void collectCandidates(const cv::Mat& source, const cv::Mat& evidence, double minArea,
                       std::vector<Candidate>& candidates)
{
    std::vector<Contour> contours;
    cv::findContours(source, contours, cv::RETR_LIST, cv::CHAIN_APPROX_NONE);

    // Rank by hull area: an outline with small gaps still has the hull of the whole document.
    struct Ranked
    {
        double area;
        Contour hull;
        const Contour* contour;
    };
    std::vector<Ranked> ranked;
    for (const Contour& contour : contours)
    {
        Contour hull;
        cv::convexHull(contour, hull);
        const double area = cv::contourArea(hull);
        if (area >= minArea)
        {
            ranked.push_back({area, std::move(hull), &contour});
        }
    }
    std::sort(ranked.begin(), ranked.end(),
              [](const Ranked& a, const Ranked& b) { return a.area > b.area; });
    if (ranked.size() > kMaxContoursPerMap)
    {
        ranked.erase(ranked.begin() + kMaxContoursPerMap, ranked.end());
    }

    const double imageArea = static_cast<double>(source.total());
    for (const Ranked& item : ranked)
    {
        const std::optional<Quad> rough = approximateQuad(item.hull);
        if (!rough)
        {
            continue;
        }
        const Quad quad = refineByLineFit(*rough, *item.contour);
        const double area = quadArea(quad);
        const double cosine = maxCornerCosine(quad);
        if (area < minArea || cosine > kMaxCornerCosine ||
            !cv::isContourConvex(std::vector<cv::Point2f>(quad.begin(), quad.end())))
        {
            continue;
        }
        const double support = edgeSupport(quad, evidence);
        if (support < kMinEdgeSupport)
        {
            continue;
        }

        // Prefer outlines that lie on real edges, are close to rectangular and are large.
        const double sizeScore = std::min(1.0, area / (0.5 * imageArea));
        const double score = 0.5 * support + 0.25 * (1.0 - cosine) + 0.25 * sizeScore;
        candidates.push_back({quad, score});
    }
}

} // namespace

DetectResult detectDocument(const cv::Mat& image, const DetectOptions& options)
{
    detail::requireImage(image, "detectDocument");
    if (options.workingSize < 64)
    {
        throw Error("detectDocument: workingSize must be at least 64");
    }
    if (!(options.minAreaRatio >= 0.0 && options.minAreaRatio < 1.0))
    {
        throw Error("detectDocument: minAreaRatio must be in [0, 1)");
    }

    const double scale = detail::downscaleFactor(image.size(), options.workingSize);
    cv::Mat small = image;
    if (scale < 1.0)
    {
        cv::resize(image, small, cv::Size(), scale, scale, cv::INTER_AREA);
    }
    const cv::Mat gray = detail::toGray(small);

    const EdgeMaps maps = buildEdgeMaps(gray);
    const double minArea = options.minAreaRatio * static_cast<double>(gray.total());
    std::vector<Candidate> candidates;
    for (const cv::Mat& source : maps.sources)
    {
        collectCandidates(source, maps.evidence, minArea, candidates);
    }

    DetectResult result;
    result.corners = fullFrameQuad(image.size());
    const auto best = std::max_element(candidates.begin(), candidates.end(),
                                       [](const Candidate& a, const Candidate& b) {
                                           return a.score < b.score;
                                       });
    if (best == candidates.end())
    {
        return result;
    }

    result.found = true;
    result.confidence = std::clamp(best->score, 0.0, 1.0);
    const float maxX = static_cast<float>(image.cols - 1);
    const float maxY = static_cast<float>(image.rows - 1);
    const float inverseScale = static_cast<float>(1.0 / scale);
    for (size_t i = 0; i < 4; ++i)
    {
        // Undo the resize (pixel centres: x_small = (x + 0.5) * scale - 0.5).
        const cv::Point2f point = (best->quad[i] + cv::Point2f(0.5f, 0.5f)) * inverseScale -
                                  cv::Point2f(0.5f, 0.5f);
        result.corners[i] = {std::clamp(point.x, 0.f, maxX), std::clamp(point.y, 0.f, maxY)};
    }
    return result;
}

} // namespace docscan
