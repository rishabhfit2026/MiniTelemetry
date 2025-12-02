cmake_minimum_required(VERSION 3.15)

find_package(CycloneDDS REQUIRED)
find_package(Threads REQUIRED)

# Generate IDL (CycloneDDS makes an INTERFACE library by default)
idlc_generate(
    TARGET telemetry_idl_lib
    FILES telemetry.idl
)

# --- FIX: CycloneDDS created telemetry_idl_lib as INTERFACE.
# --- Delete it and recreate a new STATIC library with same name.
get_target_property(_idl_type telemetry_idl_lib TYPE)
if(_idl_type STREQUAL "INTERFACE_LIBRARY")
    message(STATUS "Fixing CycloneDDS IDL target: converting INTERFACE -> STATIC")

    # Save include dirs
    get_target_property(_idl_includes telemetry_idl_lib INTERFACE_INCLUDE_DIRECTORIES)

    # Remove the bad INTERFACE target
    unset(telemetry_idl_lib CACHE)
    remove_library(telemetry_idl_lib) # ignore warning if any

    # Create a REAL STATIC library
    add_library(telemetry_idl_lib STATIC ${CMAKE_CURRENT_BINARY_DIR}/telemetry.c)

    # Restore include dirs
    target_include_directories(telemetry_idl_lib PUBLIC ${_idl_includes})
endif()

# Link DDS
target_link_libraries(telemetry_idl_lib PUBLIC CycloneDDS::ddsc)

# Core library
add_library(telemetry_core STATIC)

target_include_directories(telemetry_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(telemetry_core PUBLIC
    telemetry_idl_lib
    Threads::Threads
)
