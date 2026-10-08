#include <docscan/enhance.hpp>

#include <gtest/gtest.h>

#include <opencv2/imgproc.hpp>

namespace docscan
{
namespace
{

// Paper lit from the left (bright -> dim, like a shadow) with dark text lines.
cv::Mat shadowedPage()
{
    cv::Mat page(400, 300, CV_8UC3);
    for (int x = 0; x < page.cols; ++x)
    {
        const uchar value = cv::saturate_cast<uchar>(250 - 110 * x / page.cols);
        page.col(x).setTo(cv::Scalar::all(value));
    }
    for (int y = 40; y < page.rows - 40; y += 30)
    {
        cv::line(page, {30, y}, {270, y}, cv::Scalar::all(30), 3);
    }
    return page;
}

TEST(Enhance, OutputFormats)
{
    const cv::Mat page = shadowedPage();

    const cv::Mat none = enhanceDocument(page, EnhanceMode::None);
    const cv::Mat gray = enhanceDocument(page, EnhanceMode::Gray);
    const cv::Mat bw = enhanceDocument(page, EnhanceMode::BlackWhite);
    const cv::Mat magic = enhanceDocument(page, EnhanceMode::MagicColor);

    for (const cv::Mat& output : {none, gray, bw, magic})
    {
        EXPECT_EQ(output.size(), page.size());
        EXPECT_EQ(output.depth(), CV_8U);
    }
    EXPECT_EQ(none.channels(), 3);
    EXPECT_EQ(gray.channels(), 1);
    EXPECT_EQ(bw.channels(), 1);
    EXPECT_EQ(magic.channels(), 3);

    // Black & white output only contains 0 and 255.
    EXPECT_EQ(cv::countNonZero(bw == 0) + cv::countNonZero(bw == 255), static_cast<int>(bw.total()));
}

TEST(Enhance, MagicColorRemovesShadow)
{
    const cv::Mat magic = enhanceDocument(shadowedPage(), EnhanceMode::MagicColor);
    cv::Mat gray;
    cv::cvtColor(magic, gray, cv::COLOR_BGR2GRAY);

    // A text-free band: the dim right side must become as white as the bright left side.
    const cv::Mat band = gray(cv::Rect(0, 0, gray.cols, 25));
    double minValue = 0;
    cv::minMaxLoc(band, &minValue);
    EXPECT_GT(minValue, 235.0);

    // Text stays dark.
    EXPECT_LT(gray.at<uchar>(40, 150), 80);
}

TEST(Enhance, ParseMode)
{
    EXPECT_EQ(parseEnhanceMode("none"), EnhanceMode::None);
    EXPECT_EQ(parseEnhanceMode("gray"), EnhanceMode::Gray);
    EXPECT_EQ(parseEnhanceMode("bw"), EnhanceMode::BlackWhite);
    EXPECT_EQ(parseEnhanceMode("magic"), EnhanceMode::MagicColor);
    EXPECT_THROW(parseEnhanceMode("sepia"), Error);
}

} // namespace
} // namespace docscan
