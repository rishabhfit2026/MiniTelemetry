cmake_minimum_required(VERSION 3.15)

project(MiniTelemetry VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Dependencies (GTest + json)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
)
FetchContent_MakeAvailable(json)

# 🔥 ADD THESE TWO LINES
find_package(CycloneDDS REQUIRED)
find_package(CycloneDDS-CXX REQUIRED)

# Project folders
add_subdirectory(src/core)
add_subdirectory(src/apps)
add_subdirectory(tests)
