#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include "../src/core/telemetry_types.h"

TEST(SerializationTest, SensorDataRoundTrip) {
    // Create original data
    SensorData original;
    original.id = 42;
    original.value = 25.5;
    original.timestamp = 1234567890;

    // Serialize to JSON
    nlohmann::json j;
    j["id"] = original.id;
    j["value"] = original.value;
    j["timestamp"] = original.timestamp;

    // Deserialize back
    SensorData parsed;
    parsed.id = j["id"];
    parsed.value = j["value"];
    parsed.timestamp = j["timestamp"];

    // Verify
    EXPECT_EQ(original.id, parsed.id);
    EXPECT_DOUBLE_EQ(original.value, parsed.value);
    EXPECT_EQ(original.timestamp, parsed.timestamp);
}

TEST(SerializationTest, JsonStringRoundTrip) {
    SensorData original{99, 30.5, 9876543210};
    
    nlohmann::json j;
    j["id"] = original.id;
    j["value"] = original.value;
    j["timestamp"] = original.timestamp;
    j["sequence"] = 5;
    
    std::string json_str = j.dump();
    
    nlohmann::json parsed = nlohmann::json::parse(json_str);
    
    EXPECT_EQ(original.id, parsed["id"].get<int>());
    EXPECT_DOUBLE_EQ(original.value, parsed["value"].get<double>());
    EXPECT_EQ(original.timestamp, parsed["timestamp"].get<uint64_t>());
    EXPECT_EQ(5, parsed["sequence"].get<uint64_t>());
}