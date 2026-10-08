// JavaScript bindings (Embind) for the docscan core.
//
// Images cross the boundary as plain objects shaped like the browser's ImageData:
//     { width: number, height: number, data: Uint8Array | Uint8ClampedArray }   // RGBA
// Every result is a new JS object that owns its pixels, so JavaScript never frees WebAssembly
// memory by hand. Keep js/wasm/docscan.d.mts in sync with this file.

#include <docscan/docscan.hpp>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

namespace
{

using emscripten::val;

// ---- JS -> C++ --------------------------------------------------------------------------------

bool isMissing(const val& value)
{
    return value.isUndefined() || value.isNull();
}

// Copies a JS RGBA image into WebAssembly memory as a CV_8UC4 Mat (one bulk copy).
cv::Mat rgbaFromJs(const val& image)
{
    if (isMissing(image))
    {
        throw docscan::Error("image is required: { width, height, data }");
    }
    const val width = image["width"];
    const val height = image["height"];
    const val data = image["data"];
    if (!width.isNumber() || !height.isNumber())
    {
        throw docscan::Error("image.width and image.height must be numbers");
    }
    const double cols = width.as<double>();
    const double rows = height.as<double>();
    if (!(cols >= 1 && rows >= 1 && cols <= 65535 && rows <= 65535) || std::floor(cols) != cols ||
        std::floor(rows) != rows)
    {
        throw docscan::Error("image.width and image.height must be integers in [1, 65535]");
    }
    if (isMissing(data) ||
        !(data.instanceof(val::global("Uint8Array")) || data.instanceof(val::global("Uint8ClampedArray"))))
    {
        throw docscan::Error("image.data must be a Uint8Array or Uint8ClampedArray of RGBA pixels");
    }
    const size_t bytes = static_cast<size_t>(cols) * static_cast<size_t>(rows) * 4;
    if (data["length"].as<double>() != static_cast<double>(bytes))
    {
        throw docscan::Error("image.data must hold width * height * 4 bytes (RGBA)");
    }

    cv::Mat rgba(static_cast<int>(rows), static_cast<int>(cols), CV_8UC4);
    // TypedArray.prototype.set on a view of the Mat's memory: a single memcpy.
    val(emscripten::typed_memory_view(bytes, rgba.data)).call<void>("set", data);
    return rgba;
}

cv::Mat bgrFromJs(const val& image)
{
    cv::Mat bgr;
    cv::cvtColor(rgbaFromJs(image), bgr, cv::COLOR_RGBA2BGR);
    return bgr;
}

docscan::Quad quadFromJs(const val& corners)
{
    const char* const message = "corners must be an array of 4 points { x, y }";
    if (isMissing(corners) || !corners.isArray() || corners["length"].as<int>() != 4)
    {
        throw docscan::Error(message);
    }
    docscan::Quad quad;
    for (int i = 0; i < 4; ++i)
    {
        const val point = corners[i];
        if (isMissing(point) || !point["x"].isNumber() || !point["y"].isNumber())
        {
            throw docscan::Error(message);
        }
        quad[i] = cv::Point2f(point["x"].as<float>(), point["y"].as<float>());
    }
    return quad;
}

// Reads an optional numeric field; leaves `target` untouched when it is absent.
template <typename T>
void readNumber(const val& object, const char* key, T& target)
{
    if (isMissing(object))
    {
        return;
    }
    const val value = object[key];
    if (isMissing(value))
    {
        return;
    }
    const double number = value.isNumber() ? value.as<double>() : std::nan("");
    if (!std::isfinite(number))
    {
        throw docscan::Error(std::string("option '") + key + "' must be a finite number");
    }
    target = static_cast<T>(std::clamp(number, static_cast<double>(std::numeric_limits<T>::lowest()),
                                       static_cast<double>(std::numeric_limits<T>::max())));
}

docscan::DetectOptions detectOptionsFromJs(const val& options)
{
    docscan::DetectOptions result;
    readNumber(options, "workingSize", result.workingSize);
    readNumber(options, "minAreaRatio", result.minAreaRatio);
    return result;
}

docscan::WarpOptions warpOptionsFromJs(const val& options)
{
    docscan::WarpOptions result;
    readNumber(options, "aspectRatio", result.aspectRatio);
    readNumber(options, "maxOutputSize", result.maxOutputSize);
    return result;
}

docscan::EnhanceMode enhanceModeFromJs(const val& mode)
{
    if (isMissing(mode))
    {
        return docscan::EnhanceMode::None;
    }
    if (!mode.isString())
    {
        throw docscan::Error("enhance mode must be one of 'none', 'gray', 'bw', 'magic'");
    }
    return docscan::parseEnhanceMode(mode.as<std::string>());
}

// ---- C++ -> JS --------------------------------------------------------------------------------

val quadToJs(const docscan::Quad& quad)
{
    val corners = val::array();
    for (const cv::Point2f& corner : quad)
    {
        val point = val::object();
        point.set("x", corner.x);
        point.set("y", corner.y);
        corners.call<void>("push", point);
    }
    return corners;
}

val detectionToJs(const docscan::DetectResult& detection)
{
    val result = val::object();
    result.set("found", detection.found);
    result.set("confidence", detection.confidence);
    result.set("corners", quadToJs(detection.corners));
    return result;
}

// `image` is GRAY or BGR (core output) or RGBA (warp output, which keeps the JS layout).
val imageToJs(const cv::Mat& image)
{
    cv::Mat rgba;
    switch (image.channels())
    {
    case 1:
        cv::cvtColor(image, rgba, cv::COLOR_GRAY2RGBA);
        break;
    case 3:
        cv::cvtColor(image, rgba, cv::COLOR_BGR2RGBA);
        break;
    default:
        rgba = image.isContinuous() ? image : image.clone();
        break;
    }

    const size_t bytes = rgba.total() * rgba.elemSize();
    val data = val::global("Uint8ClampedArray").new_(bytes);
    data.call<void>("set", val(emscripten::typed_memory_view(bytes, rgba.data)));

    val result = val::object();
    result.set("width", rgba.cols);
    result.set("height", rgba.rows);
    result.set("data", data);
    return result;
}

// Runs `body`, turning C++ exceptions (docscan::Error, cv::Exception, std::bad_alloc...) into
// JavaScript Errors carrying the same message.
template <typename Body>
val guarded(Body&& body)
{
    std::string message;
    try
    {
        return body();
    }
    catch (const std::exception& e)
    {
        message = e.what();
    }
    val::global("Error").new_(message).throw_();
    __builtin_unreachable();
}

// ---- Exported functions -----------------------------------------------------------------------

std::string version()
{
    return docscan::version();
}

val detect(const val& image, const val& options)
{
    return guarded([&] {
        // Color matters: detection also looks for hue edges, not only luminance edges.
        return detectionToJs(docscan::detectDocument(bgrFromJs(image), detectOptionsFromJs(options)));
    });
}

val warp(const val& image, const val& corners, const val& options)
{
    return guarded([&] {
        // Warping does not care about channel order: stay in RGBA end to end.
        const cv::Mat page = docscan::warpDocument(rgbaFromJs(image), quadFromJs(corners),
                                                   warpOptionsFromJs(options));
        return imageToJs(page);
    });
}

val enhance(const val& image, const val& mode)
{
    return guarded([&] {
        return imageToJs(docscan::enhanceDocument(bgrFromJs(image), enhanceModeFromJs(mode)));
    });
}

val scan(const val& image, const val& options)
{
    return guarded([&] {
        docscan::ScanOptions scanOptions;
        if (!isMissing(options))
        {
            scanOptions.detect = detectOptionsFromJs(options["detect"]);
            scanOptions.warp = warpOptionsFromJs(options["warp"]);
            scanOptions.enhance = enhanceModeFromJs(options["enhance"]);
        }
        const docscan::ScanResult result = docscan::scanDocument(bgrFromJs(image), scanOptions);

        val output = val::object();
        output.set("detection", detectionToJs(result.detection));
        output.set("image", imageToJs(result.image));
        return output;
    });
}

} // namespace

EMSCRIPTEN_BINDINGS(docscan)
{
    emscripten::function("version", &version);
    emscripten::function("detect", &detect);
    emscripten::function("warp", &warp);
    emscripten::function("enhance", &enhance);
    emscripten::function("scan", &scan);
}
