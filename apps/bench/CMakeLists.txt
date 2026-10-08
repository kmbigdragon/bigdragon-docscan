find_package(Threads REQUIRED)

# Metrics live in a small library so the unit tests can cover them.
add_library(docscan_eval STATIC metrics.cpp)
target_include_directories(docscan_eval PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(docscan_eval PUBLIC docscan::core PRIVATE opencv_imgproc)
docscan_enable_warnings(docscan_eval)

add_executable(docscan_bench main.cpp)
target_link_libraries(docscan_bench PRIVATE docscan_eval opencv_imgcodecs Threads::Threads)
set_target_properties(docscan_bench PROPERTIES OUTPUT_NAME docscan-bench)
docscan_enable_warnings(docscan_bench)
