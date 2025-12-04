enable_testing()

find_package(GTest REQUIRED)
find_package(nlohmann_json REQUIRED)

# Test: Serialization
add_executable(test_serialization test_serialization.cpp)
target_include_directories(test_serialization PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
)
target_link_libraries(test_serialization
    PRIVATE
        GTest::GTest
        GTest::Main
        nlohmann_json::nlohmann_json
)
add_test(NAME SerializationTests COMMAND test_serialization)

# Test: Queue (your existing test_queue.cpp)
add_executable(test_queue test_queue.cpp)
target_link_libraries(test_queue
    PRIVATE
        telemetry_core
        GTest::GTest
        GTest::Main
)
add_test(NAME QueueTests COMMAND test_queue)

# Test: End-to-End
add_executable(test_end_to_end test_end_to_end.cpp)
target_link_libraries(test_end_to_end
    PRIVATE
        telemetry_core
        GTest::GTest
        GTest::Main
)
add_test(NAME EndToEndTests COMMAND test_end_to_end)