#pragma once
#include <cstdint>
#include <nlohmann/json.hpp>

// This struct represents one piece of data from a sensor.
struct SensorData {
    int id;                 // Which sensor is this? (0, 1, 2...)
    double value;           // The actual reading (e.g., 25.5 degrees)
    long timestamp;         // Time in milliseconds
};

// Manual JSON conversion functions
inline void to_json(nlohmann::json& j, const SensorData& data) {
    j = nlohmann::json{
        {"id", data.id},
        {"value", data.value},
        {"timestamp", data.timestamp}
    };
}

inline void from_json(const nlohmann::json& j, SensorData& data) {
    j.at("id").get_to(data.id);
    j.at("value").get_to(data.value);
    j.at("timestamp").get_to(data.timestamp);
}