cmake_minimum_required(VERSION 3.15)

find_package(CycloneDDS REQUIRED)
find_package(Threads REQUIRED)

# Generate IDL first
idlc_generate(
    TARGET telemetry_idl_lib
    FILES telemetry.idl
)

# Check the type of library CycloneDDS created
get_target_property(_idl_type telemetry_idl_lib TYPE)

if(_idl_type STREQUAL "INTERFACE_LIBRARY")
    message(STATUS "CycloneDDS created INTERFACE library, wrapping it...")
    
    # Get the generated source file location
    get_target_property(_idl_sources telemetry_idl_lib INTERFACE_SOURCES)
    
    # Create a STATIC library that wraps the generated code
    add_library(telemetry_idl_wrapper STATIC 
        ${_idl_sources}
    )
    
    # Get include directories from the INTERFACE library
    get_target_property(_idl_includes telemetry_idl_lib INTERFACE_INCLUDE_DIRECTORIES)
    
    # Apply include directories
    target_include_directories(telemetry_idl_wrapper PUBLIC ${_idl_includes})
    
    # Link the INTERFACE library and DDS
    target_link_libraries(telemetry_idl_wrapper 
        PUBLIC 
            CycloneDDS::ddsc
            telemetry_idl_lib
    )
    
    # Use the wrapper as our IDL library
    set(IDL_LIBRARY telemetry_idl_wrapper)
else()
    # If it's already a proper library, just link DDS
    target_link_libraries(telemetry_idl_lib PUBLIC CycloneDDS::ddsc)
    set(IDL_LIBRARY telemetry_idl_lib)
endif()

# Core library with actual source files
add_library(telemetry_core STATIC
    telemetry_core.cpp
    thread_safe_queue.cpp
)

target_include_directories(telemetry_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}  # For generated IDL headers
)

target_link_libraries(telemetry_core PUBLIC
    ${IDL_LIBRARY}
    Threads::Threads
)