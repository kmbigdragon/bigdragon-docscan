// docscan-bench: runs detectDocument() over an annotated dataset and writes one CSV row per
// image, for scripts/eval/report.mjs to summarise. See docs/EVALUATION.md.
//
// Manifest (CSV with header, produced by scripts/datasets/*.mjs; paths relative to the manifest):
//   image,group,category,split,ref_w,ref_h,tl_x,tl_y,tr_x,tr_y,br_x,br_y,bl_x,bl_y

#include "metrics.hpp"

#include <docscan/docscan.hpp>

#include <opencv2/imgcodecs.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace
{

constexpr const char* kUsage = R"(Usage: docscan-bench <manifest.csv> <results.csv> [options]

Options:
  --split <name>        only rows of this split (e.g. dev, test)   (default: all)
  --stride <n>          every n-th row, for quick iterations        (default: 1)
  --threads <n>         worker threads; use 1 for timing figures    (default: all cores)
  --working-size <px>   DetectOptions::workingSize                  (default: library default)
  --min-area <ratio>    DetectOptions::minAreaRatio                 (default: library default)
)";

struct Sample
{
    std::string image;
    std::string group;
    std::string category;
    std::string split;
    cv::Size2d reference;
    docscan::Quad truth;
};

struct Outcome
{
    bool loaded = false;
    docscan::DetectResult detection;
    docscan::eval::QuadMetrics metrics;
    double milliseconds = 0.0;
};

struct Options
{
    std::filesystem::path manifest;
    std::filesystem::path results;
    std::string split;
    int stride = 1;
    unsigned threads = std::max(1u, std::thread::hardware_concurrency());
    docscan::DetectOptions detect;
};

std::vector<std::string> splitCsvLine(const std::string& line)
{
    std::vector<std::string> fields;
    std::stringstream stream(line);
    std::string field;
    while (std::getline(stream, field, ','))
    {
        fields.push_back(field);
    }
    return fields;
}

std::vector<Sample> loadManifest(const Options& options)
{
    std::ifstream file(options.manifest);
    if (!file)
    {
        throw docscan::Error("cannot open " + options.manifest.string());
    }
    std::string line;
    std::getline(file, line);
    std::map<std::string, size_t> column;
    const std::vector<std::string> header = splitCsvLine(line);
    for (size_t i = 0; i < header.size(); ++i)
    {
        column[header[i]] = i;
    }
    for (const char* name : {"image", "group", "category", "split", "ref_w", "ref_h", "tl_x", "tl_y", "tr_x",
                             "tr_y", "br_x", "br_y", "bl_x", "bl_y"})
    {
        if (column.count(name) == 0)
        {
            throw docscan::Error(std::string("manifest is missing column '") + name + "'");
        }
    }

    std::vector<Sample> samples;
    size_t row = 0;
    while (std::getline(file, line))
    {
        if (line.empty())
        {
            continue;
        }
        const std::vector<std::string> fields = splitCsvLine(line);
        const auto number = [&](const char* name) { return std::stod(fields.at(column[name])); };
        Sample sample;
        sample.split = fields.at(column["split"]);
        if ((!options.split.empty() && sample.split != options.split) || row++ % options.stride != 0)
        {
            continue;
        }
        sample.image = fields.at(column["image"]);
        sample.group = fields.at(column["group"]);
        sample.category = fields.at(column["category"]);
        sample.reference = {number("ref_w"), number("ref_h")};
        sample.truth = {cv::Point2f(static_cast<float>(number("tl_x")), static_cast<float>(number("tl_y"))),
                        cv::Point2f(static_cast<float>(number("tr_x")), static_cast<float>(number("tr_y"))),
                        cv::Point2f(static_cast<float>(number("br_x")), static_cast<float>(number("br_y"))),
                        cv::Point2f(static_cast<float>(number("bl_x")), static_cast<float>(number("bl_y")))};
        samples.push_back(sample);
    }
    return samples;
}

Outcome evaluate(const Sample& sample, const Options& options)
{
    Outcome outcome;
    const cv::Mat image = cv::imread((options.manifest.parent_path() / sample.image).string(), cv::IMREAD_COLOR);
    if (image.empty())
    {
        return outcome;
    }
    outcome.loaded = true;
    const auto start = std::chrono::steady_clock::now();
    outcome.detection = docscan::detectDocument(image, options.detect);
    outcome.milliseconds =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    outcome.metrics = docscan::eval::compareQuads(outcome.detection.corners, sample.truth, sample.reference);
    return outcome;
}

Options parseArgs(const std::vector<std::string>& args)
{
    Options options;
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
        if (arg == "--split")
        {
            options.split = value();
        }
        else if (arg == "--stride")
        {
            options.stride = std::max(1, std::stoi(value()));
        }
        else if (arg == "--threads")
        {
            options.threads = static_cast<unsigned>(std::max(1, std::stoi(value())));
        }
        else if (arg == "--working-size")
        {
            options.detect.workingSize = std::stoi(value());
        }
        else if (arg == "--min-area")
        {
            options.detect.minAreaRatio = std::stod(value());
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
        throw docscan::Error("expected <manifest.csv> and <results.csv>");
    }
    options.manifest = positional[0];
    options.results = positional[1];
    return options;
}

void writeResults(const Options& options, const std::vector<Sample>& samples, const std::vector<Outcome>& outcomes)
{
    std::ofstream out(options.results);
    if (!out)
    {
        throw docscan::Error("cannot write " + options.results.string());
    }
    out << "image,group,category,split,loaded,found,confidence,jaccard,corner_err_mean,corner_err_max,"
           "corner_err_px,detect_ms,tl_x,tl_y,tr_x,tr_y,br_x,br_y,bl_x,bl_y\n";
    out << std::setprecision(6);
    for (size_t i = 0; i < samples.size(); ++i)
    {
        const Sample& s = samples[i];
        const Outcome& o = outcomes[i];
        out << s.image << ',' << s.group << ',' << s.category << ',' << s.split << ',' << o.loaded << ','
            << o.detection.found << ',' << o.detection.confidence << ',' << o.metrics.jaccard << ','
            << o.metrics.meanCornerError << ',' << o.metrics.maxCornerError << ','
            << o.metrics.meanCornerErrorPixels << ',' << o.milliseconds;
        for (const cv::Point2f& corner : o.detection.corners)
        {
            out << ',' << corner.x << ',' << corner.y;
        }
        out << '\n';
    }

    // Everything needed to reproduce the run.
    std::ofstream meta(options.results.string() + ".meta.json");
    meta << "{\n"
         << "  \"docscan_version\": \"" << docscan::version() << "\",\n"
         << "  \"opencv_version\": \"" << CV_VERSION << "\",\n"
         << "  \"manifest\": \"" << options.manifest.generic_string() << "\",\n"
         << "  \"split\": \"" << options.split << "\",\n"
         << "  \"stride\": " << options.stride << ",\n"
         << "  \"threads\": " << options.threads << ",\n"
         << "  \"working_size\": " << options.detect.workingSize << ",\n"
         << "  \"min_area_ratio\": " << options.detect.minAreaRatio << ",\n"
         << "  \"samples\": " << samples.size() << "\n"
         << "}\n";
}

} // namespace

int main(int argc, char** argv)
{
    const std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty() || args[0] == "-h" || args[0] == "--help")
    {
        std::cout << kUsage;
        return args.empty() ? 1 : 0;
    }

    try
    {
        const Options options = parseArgs(args);
        const std::vector<Sample> samples = loadManifest(options);
        std::cout << "docscan " << docscan::version() << ": " << samples.size() << " images, " << options.threads
                  << " threads\n";

        std::vector<Outcome> outcomes(samples.size());
        std::atomic<size_t> next{0};
        std::atomic<size_t> done{0};
        std::mutex logMutex;
        const auto worker = [&] {
            for (size_t i = next++; i < samples.size(); i = next++)
            {
                outcomes[i] = evaluate(samples[i], options);
                if (++done % 1000 == 0)
                {
                    const std::lock_guard<std::mutex> lock(logMutex);
                    std::cout << "  " << done << " / " << samples.size() << "\n" << std::flush;
                }
            }
        };
        std::vector<std::thread> threads;
        for (unsigned t = 0; t < options.threads; ++t)
        {
            threads.emplace_back(worker);
        }
        for (std::thread& thread : threads)
        {
            thread.join();
        }

        writeResults(options, samples, outcomes);

        double jaccardSum = 0.0;
        size_t missing = 0;
        for (const Outcome& outcome : outcomes)
        {
            jaccardSum += outcome.metrics.jaccard;
            missing += outcome.loaded ? 0 : 1;
        }
        std::cout << "mean Jaccard index: " << (samples.empty() ? 0.0 : jaccardSum / samples.size()) << "\n";
        if (missing > 0)
        {
            std::cout << "warning: " << missing << " images could not be read (scored 0)\n";
        }
        std::cout << "wrote " << options.results.string() << " - summarise with scripts/eval/report.mjs\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "error: " << e.what() << "\n\n" << kUsage;
        return 1;
    }
}
