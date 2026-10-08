// docscan-cli: runs the scan pipeline on image files. Native builds only; handy for tuning the
// algorithms on real photos without going through JavaScript.

#include <docscan/docscan.hpp>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{

constexpr const char* kUsage = R"(Usage: docscan-cli <input> <output> [options]

Options:
  --enhance <mode>   none | gray | bw | magic          (default: magic)
  --aspect <ratio>   a4 | letter | card | <number>      (default: from the corners)
  --max-size <px>    limit the longest output side      (default: no limit)
  --min-area <ratio> smallest document area / image area (default: library default)
  --debug <file>     also save the input with the detected outline drawn on it
  -h, --help         show this help
)";

struct CliOptions
{
    std::string input;
    std::string output;
    std::string debug;
    docscan::ScanOptions scan;
};

double parseAspect(const std::string& value)
{
    if (value == "a4")
    {
        return docscan::kAspectA4;
    }
    if (value == "letter")
    {
        return docscan::kAspectLetter;
    }
    if (value == "card")
    {
        return docscan::kAspectIdCard;
    }
    return std::stod(value);
}

CliOptions parseArgs(const std::vector<std::string>& args)
{
    CliOptions options;
    options.scan.enhance = docscan::EnhanceMode::MagicColor;

    std::vector<std::string> positional;
    for (size_t i = 0; i < args.size(); ++i)
    {
        const std::string& arg = args[i];
        const auto value = [&]() -> const std::string& {
            if (i + 1 >= args.size())
            {
                throw docscan::Error("missing value for " + arg);
            }
            return args[++i];
        };

        if (arg == "--enhance")
        {
            options.scan.enhance = docscan::parseEnhanceMode(value());
        }
        else if (arg == "--aspect")
        {
            options.scan.warp.aspectRatio = parseAspect(value());
        }
        else if (arg == "--max-size")
        {
            options.scan.warp.maxOutputSize = std::stoi(value());
        }
        else if (arg == "--min-area")
        {
            options.scan.detect.minAreaRatio = std::stod(value());
        }
        else if (arg == "--debug")
        {
            options.debug = value();
        }
        else if (arg.rfind("--", 0) == 0)
        {
            throw docscan::Error("unknown option " + arg);
        }
        else
        {
            positional.push_back(arg);
        }
    }

    if (positional.size() != 2)
    {
        throw docscan::Error("expected <input> and <output>");
    }
    options.input = positional[0];
    options.output = positional[1];
    return options;
}

void saveDebugImage(const std::string& path, const cv::Mat& image, const docscan::DetectResult& detection)
{
    cv::Mat canvas = image.clone();
    std::vector<cv::Point> outline;
    for (const cv::Point2f& corner : detection.corners)
    {
        outline.emplace_back(cvRound(corner.x), cvRound(corner.y));
    }
    const cv::Scalar color = detection.found ? cv::Scalar(0, 200, 0) : cv::Scalar(0, 0, 255);
    const int thickness = std::max(2, std::max(image.cols, image.rows) / 300);
    cv::polylines(canvas, outline, true, color, thickness, cv::LINE_AA);
    for (const cv::Point& corner : outline)
    {
        cv::circle(canvas, corner, thickness * 3, color, cv::FILLED, cv::LINE_AA);
    }
    if (!cv::imwrite(path, canvas))
    {
        throw docscan::Error("cannot write " + path);
    }
}

} // namespace

int main(int argc, char** argv)
{
    const std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty() || std::find(args.begin(), args.end(), "-h") != args.end() ||
        std::find(args.begin(), args.end(), "--help") != args.end())
    {
        std::cout << kUsage;
        return args.empty() ? 1 : 0;
    }

    try
    {
        const CliOptions options = parseArgs(args);
        const cv::Mat image = cv::imread(options.input, cv::IMREAD_COLOR);
        if (image.empty())
        {
            throw docscan::Error("cannot read " + options.input);
        }

        const auto start = std::chrono::steady_clock::now();
        const docscan::ScanResult result = docscan::scanDocument(image, options.scan);
        const std::chrono::duration<double, std::milli> elapsed = std::chrono::steady_clock::now() - start;

        std::cout << "docscan " << docscan::version() << "\n"
                  << "input:      " << image.cols << "x" << image.rows << "\n"
                  << "found:      " << (result.detection.found ? "yes" : "no") << "\n"
                  << "confidence: " << result.detection.confidence << "\n"
                  << "corners:   ";
        for (const cv::Point2f& corner : result.detection.corners)
        {
            std::cout << " (" << corner.x << ", " << corner.y << ")";
        }
        std::cout << "\n"
                  << "output:     " << result.image.cols << "x" << result.image.rows << "\n"
                  << "time:       " << elapsed.count() << " ms\n";

        if (!cv::imwrite(options.output, result.image))
        {
            throw docscan::Error("cannot write " + options.output);
        }
        if (!options.debug.empty())
        {
            saveDebugImage(options.debug, image, result.detection);
        }
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "error: " << e.what() << "\n\n" << kUsage;
        return 1;
    }
}
