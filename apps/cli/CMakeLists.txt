add_executable(docscan_cli main.cpp)
target_link_libraries(docscan_cli PRIVATE docscan::core opencv_imgcodecs opencv_imgproc)
set_target_properties(docscan_cli PROPERTIES OUTPUT_NAME docscan-cli)
docscan_enable_warnings(docscan_cli)
