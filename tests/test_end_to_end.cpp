#include <gtest/gtest.h>
#include <map>
#include "../src/core/telemetry_types.h"
#include "../src/core/thread_safe_queue.h"

// Simple sensor state tracker (mimics monitor logic)
struct SimpleSensorTracker {
    uint64_t expected_seq = 0;
    uint64_t message_count = 0;
    uint64_t dropped_count = 0;
    double sum_value = 0.0;
    
    void process_message(const SensorData& data, uint64_t sequence) {
        if (message_count > 0 && sequence != expected_seq) {
            dropped_count += (sequence - expected_seq);
        }
        
        expected_seq = sequence + 1;
        message_count++;
        sum_value += data.value;
    }
    
    double get_average() const {
        return message_count > 0 ? sum_value / message_count : 0.0;
    }
};

TEST(EndToEndTest, SensorDataPipeline) {
    ThreadSafeQueue<SensorData> queue;
    std::map<int, SimpleSensorTracker> trackers;
    
    // Simulate 3 sensors sending 10 messages each
    for (int sensor_id = 0; sensor_id < 3; ++sensor_id) {
        for (uint64_t seq = 0; seq < 10; ++seq) {
            SensorData data;
            data.id = sensor_id;
            data.value = 20.0 + seq;
            data.timestamp = seq * 1000;
            
            queue.push(data);
        }
    }
    
    // Process all messages (mimics monitor)
    uint64_t sequences[3] = {0, 0, 0};
    SensorData data;
    
    int processed = 0;
    while (queue.pop(data) && processed < 30) {
        int sensor_id = data.id;
        trackers[sensor_id].process_message(data, sequences[sensor_id]++);
        processed++;
    }
    
    // Verify results
    for (int sensor_id = 0; sensor_id < 3; ++sensor_id) {
        EXPECT_EQ(10, trackers[sensor_id].message_count);
        EXPECT_EQ(0, trackers[sensor_id].dropped_count);
        EXPECT_DOUBLE_EQ(24.5, trackers[sensor_id].get_average());
    }
}

TEST(EndToEndTest, DroppedMessageDetection) {
    SimpleSensorTracker tracker;
    
    SensorData data{0, 25.0, 1000};
    
    tracker.process_message(data, 0);  // seq 0
    tracker.process_message(data, 1);  // seq 1
    tracker.process_message(data, 5);  // seq 5 (dropped 2,3,4)
    
    EXPECT_EQ(3, tracker.message_count);
    EXPECT_EQ(3, tracker.dropped_count);  // Missing sequences 2,3,4
}