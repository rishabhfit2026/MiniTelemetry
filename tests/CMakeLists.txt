cmake_minimum_required(VERSION 3.15)

# Test executable
add_executable(test_main
    test_main.cpp
    test_queue.cpp
)

target_link_libraries(test_main
    PRIVATE
        telemetry_core
        GTest::gtest
        GTest::gtest_main
        nlohmann_json::nlohmann_json
)

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(test_main)