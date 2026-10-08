configure_file(include/docscan/version.hpp.in
    "${CMAKE_CURRENT_BINARY_DIR}/include/docscan/version.hpp" @ONLY)

add_library(docscan_core STATIC
    src/detect.cpp
    src/enhance.cpp
    src/geometry.cpp
    src/image_utils.cpp
    src/scan.cpp
    src/warp.cpp)
add_library(docscan::core ALIAS docscan_core)

target_include_directories(docscan_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_BINARY_DIR}/include"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")

# Public headers use cv::Mat; imgproc is an implementation detail.
target_link_libraries(docscan_core
    PUBLIC opencv_core
    PRIVATE opencv_imgproc)
target_compile_features(docscan_core PUBLIC cxx_std_17)
docscan_enable_warnings(docscan_core)
