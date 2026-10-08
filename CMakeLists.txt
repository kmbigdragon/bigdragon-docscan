cmake_minimum_required(VERSION 3.24)

# package.json is the single source of truth for the version.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/package.json" _docscan_package_json)
string(JSON _docscan_version GET "${_docscan_package_json}" version)
string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" _docscan_version "${_docscan_version}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS package.json)

project(docscan
    VERSION ${_docscan_version}
    DESCRIPTION "Document and card scanner (detect, crop, enhance) built on OpenCV"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(DocscanHelpers)

if(EMSCRIPTEN)
    set(_docscan_native OFF)
    set(_docscan_wasm ON)
else()
    set(_docscan_native ON)
    set(_docscan_wasm OFF)
endif()

option(DOCSCAN_BUILD_WASM "Build the WebAssembly module for JavaScript (needs Emscripten)" ${_docscan_wasm})
option(DOCSCAN_BUILD_CLI "Build the docscan-cli tool (native only)" ${_docscan_native})
option(DOCSCAN_BUILD_TESTS "Build the C++ unit tests (native only)" ${_docscan_native})
option(DOCSCAN_WASM_SIMD "Use WebAssembly SIMD128 (keep in sync with the OpenCV build)" ON)
set(DOCSCAN_WASM_OUTPUT_DIR "${PROJECT_SOURCE_DIR}/dist/wasm" CACHE PATH
    "Directory that receives docscan.mjs and docscan.wasm")

if(DOCSCAN_BUILD_WASM AND NOT EMSCRIPTEN)
    message(FATAL_ERROR "DOCSCAN_BUILD_WASM needs the Emscripten toolchain: run `npm run build:wasm`.")
endif()
if(EMSCRIPTEN AND (DOCSCAN_BUILD_CLI OR DOCSCAN_BUILD_TESTS))
    message(FATAL_ERROR "The CLI and the C++ tests are native-only: use the `native` preset for them.")
endif()

if(EMSCRIPTEN)
    # Must match how OpenCV was compiled (scripts/build-opencv-wasm.mjs).
    add_compile_options(-fwasm-exceptions)
    add_link_options(-fwasm-exceptions)
    if(DOCSCAN_WASM_SIMD)
        add_compile_options(-msimd128)
    endif()
endif()

set(_docscan_opencv_components core imgproc)
if(DOCSCAN_BUILD_CLI)
    list(APPEND _docscan_opencv_components imgcodecs)
endif()
find_package(OpenCV REQUIRED COMPONENTS ${_docscan_opencv_components})
message(STATUS "docscan ${PROJECT_VERSION}: OpenCV ${OpenCV_VERSION} from ${OpenCV_DIR}")

add_subdirectory(core)

if(DOCSCAN_BUILD_WASM)
    add_subdirectory(bindings/wasm)
endif()
if(DOCSCAN_BUILD_CLI)
    add_subdirectory(apps/cli)
endif()
if(DOCSCAN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/cpp)
endif()
