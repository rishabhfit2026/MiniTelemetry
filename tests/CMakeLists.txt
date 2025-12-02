cmake_minimum_required(VERSION 3.15)

enable_testing()

find_package(GTest REQUIRED)
find_package(nlohmann_json REQUIRED)

add_executable(test_main test_main.cpp)

target_link_libraries(test_main PRIVATE
    telemetry_core
    GTest::gtest
    GTest::gtest_main
    nlohmann_json::nlohmann_json
)

add_test(NAME run_tests COMMAND test_main)
