add_executable(docscan_wasm docscan_wasm.cpp)
target_link_libraries(docscan_wasm PRIVATE docscan::core opencv_imgproc)
docscan_enable_warnings(docscan_wasm)

# Produces docscan.mjs (ES module factory) + docscan.wasm, consumed by js/index.ts.
set_target_properties(docscan_wasm PROPERTIES
    OUTPUT_NAME docscan
    SUFFIX ".mjs"
    RUNTIME_OUTPUT_DIRECTORY "${DOCSCAN_WASM_OUTPUT_DIR}")

target_link_options(docscan_wasm PRIVATE
    -lembind
    -sMODULARIZE=1
    -sEXPORT_ES6=1
    -sEXPORT_NAME=createDocScanModule
    -sENVIRONMENT=web,worker,node
    -sALLOW_MEMORY_GROWTH=1
    -sSTACK_SIZE=1048576
    -sFILESYSTEM=0
    $<$<CONFIG:Debug>:-sASSERTIONS=2>)
