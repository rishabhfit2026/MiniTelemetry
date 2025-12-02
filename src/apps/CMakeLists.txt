cmake_minimum_required(VERSION 3.15)

add_executable(sensor_hub_process sensor_hub.cpp)

find_package(nlohmann_json REQUIRED)

target_link_libraries(sensor_hub_process
    PRIVATE
        telemetry_core
        nlohmann_json::nlohmann_json
)
