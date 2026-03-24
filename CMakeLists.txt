cmake_minimum_required(VERSION 3.16)
project(berezta VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# macOS: ensure C++ standard library headers are found.
if(APPLE)
    execute_process(COMMAND xcrun --show-sdk-path OUTPUT_VARIABLE MACOS_SDK_PATH OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(EXISTS "${MACOS_SDK_PATH}/usr/include/c++/v1")
        include_directories(SYSTEM "${MACOS_SDK_PATH}/usr/include/c++/v1")
    endif()
endif()

# --- Third-party dependencies ---
include(FetchContent)

# utfcpp: header-only UTF-8 iteration and validation.
FetchContent_Declare(
    utfcpp
    GIT_REPOSITORY https://github.com/nemtrif/utfcpp.git
    GIT_TAG v4.0.6
    GIT_SHALLOW TRUE
)
set(UTF8_TESTS OFF CACHE BOOL "" FORCE)
set(UTF8_INSTALL OFF CACHE BOOL "" FORCE)
set(UTF8_SAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(utfcpp)

# wcwidth: terminal display width for Unicode characters (vendored).
add_library(wcwidth STATIC src/vendor/wcwidth.c)
target_include_directories(wcwidth PUBLIC ${CMAKE_SOURCE_DIR}/src/vendor)

# --- Core library (no platform dependencies) ---
add_library(berezta_core
    src/core/buffer.cpp
    src/core/codec.cpp
    src/core/selection.cpp
    src/core/document.cpp
    src/core/history.cpp
    src/core/search.cpp
    src/core/json.cpp
    src/core/utf8_util.cpp
)
target_include_directories(berezta_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(berezta_core PUBLIC utf8cpp wcwidth)

# --- TUI library (terminal I/O) ---
add_library(berezta_tui
    src/tui/terminal.cpp
    src/tui/renderer.cpp
    src/tui/input.cpp
    src/tui/status_bar.cpp
    src/tui/prompt.cpp
    src/tui/help.cpp
    src/tui/app.cpp
)
target_include_directories(berezta_tui PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(berezta_tui PUBLIC berezta_core)

# --- Main executable ---
add_executable(berezta src/main.cpp)
target_link_libraries(berezta PRIVATE berezta_tui)

# --- Tests ---
option(BEREZTA_BUILD_TESTS "Build tests" ON)
if(BEREZTA_BUILD_TESTS)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()

    add_executable(berezta_tests
        tests/buffer_test.cpp
        tests/codec_test.cpp
        tests/selection_test.cpp
        tests/document_test.cpp
        tests/history_test.cpp
        tests/search_test.cpp
        tests/json_test.cpp
        tests/utf8_util_test.cpp
    )
    target_link_libraries(berezta_tests PRIVATE berezta_core GTest::gtest_main)
    target_include_directories(berezta_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

    include(GoogleTest)
    gtest_discover_tests(berezta_tests)
endif()
