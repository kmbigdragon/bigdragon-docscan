include(FetchContent)
FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    FIND_PACKAGE_ARGS NAMES GTest)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE) # MSVC: same runtime (/MD) as the other targets
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(docscan_tests
    test_detect.cpp
    test_enhance.cpp
    test_geometry.cpp
    test_warp.cpp)
target_link_libraries(docscan_tests PRIVATE docscan::core opencv_imgproc GTest::gtest_main)
docscan_enable_warnings(docscan_tests)

include(GoogleTest)
gtest_discover_tests(docscan_tests DISCOVERY_MODE PRE_TEST)
