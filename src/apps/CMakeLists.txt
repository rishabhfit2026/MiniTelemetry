cmake_minimum_required(VERSION 3.15)

find_package(CycloneDDS REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Threads REQUIRED)

# ========== SENSOR HUB PROCESS ==========
add_executable(sensor_hub_process
    sensor_hub.cpp
)

target_include_directories(sensor_hub_process PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_BINARY_DIR}/src/core  # For generated IDL headers
)

target_link_libraries(sensor_hub_process PRIVATE
    telemetry_core
    CycloneDDS::ddsc
    nlohmann_json::nlohmann_json
    Threads::Threads
)

# ========== MONITOR PROCESS ==========
add_executable(monitor_process
    monitor.cpp
)

target_include_directories(monitor_process PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_BINARY_DIR}/src/core  # For generated IDL headers
)

target_link_libraries(monitor_process PRIVATE
    telemetry_core
    CycloneDDS::ddsc
    nlohmann_json::nlohmann_json
    Threads::Threads
)

# Install targets (optional)
install(TARGETS sensor_hub_process monitor_process
    RUNTIME DESTINATION bin
)