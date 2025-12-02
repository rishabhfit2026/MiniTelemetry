#pragma once
#include <cstdint>
#include <nlohmann/json.hpp> // Include the library

// This struct represents one piece of data from a sensor.
struct SensorData {
    int id;             // Which sensor is this? (0, 1, 2...)
    double value;       // The actual reading (e.g., 25.5 degrees)
    long timestamp;     // Time in milliseconds

    // This magic macro teaches the library how to read/write this struct to JSON
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(SensorData, id, value, timestamp)
};