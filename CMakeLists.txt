cmake_minimum_required(VERSION 3.15)

# Set policy for FetchContent timestamp extraction
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

# Add C language support (required for CycloneDDS generated code)
project(MiniTelemetry VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies (GTest + json)
include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(json)

# Find required packages
find_package(CycloneDDS REQUIRED)
find_package(Threads REQUIRED)

# Enable testing
enable_testing()

# Project folders
add_subdirectory(src/core)
add_subdirectory(src/apps)
add_subdirectory(tests)