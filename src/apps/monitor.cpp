#include <iostream>
#include <atomic>
#include <csignal>
#include <chrono>
#include <thread>
#include <map>
#include <iomanip>
#include <mutex>
#include <cstring>

#include <dds/dds.h>
#include "telemetry.h"
#include <nlohmann/json.hpp>

#include "../core/telemetry_types.h"

std::atomic<bool> g_running{true};

// Per-sensor tracking
struct SensorState {
    uint64_t expected_seq = 0;
    uint64_t message_count = 0;
    uint64_t dropped_count = 0;
    
    double current_value = 0.0;
    double min_value = 1e9;
    double max_value = -1e9;
    double sum_value = 0.0;
    
    uint64_t last_timestamp = 0;
    bool initialized = false;
};

std::map<int, SensorState> g_sensors;
std::mutex g_sensor_mutex;

// Dashboard refresh settings
constexpr int DASHBOARD_REFRESH_MS = 2000;  // 2 seconds
uint64_t g_last_dashboard_ms = 0;

void signal_handler(int signal) {
    std::cout << "\n[Monitor] Caught signal " << signal << ", shutting down...\n";
    g_running = false;
}

uint64_t get_current_time_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

void print_dashboard() {
    uint64_t now_ms = get_current_time_ms();
    
    // Throttle dashboard updates
    if (g_last_dashboard_ms > 0 && (now_ms - g_last_dashboard_ms) < DASHBOARD_REFRESH_MS) {
        return;  // Too soon, skip this update
    }
    
    g_last_dashboard_ms = now_ms;
    
    std::lock_guard<std::mutex> lock(g_sensor_mutex);
    
    // Check if we have any data
    bool has_data = false;
    for (const auto& pair : g_sensors) {
        if (pair.second.message_count > 0) {
            has_data = true;
            break;
        }
    }
    
    if (!has_data) {
        return;  // Don't print empty dashboard
    }
    
    std::cout << "\n=== TELEMETRY DASHBOARD ===\n";
    std::cout << "==============================================================================\n";
    
    for (const auto& pair : g_sensors) {
        int id = pair.first;
        const SensorState& state = pair.second;
        
        if (state.message_count == 0) {
            continue;  // Skip sensors with no data
        }
        
        double avg = state.sum_value / state.message_count;
        
        std::cout << "[Sensor " << id << "] "
                  << "Value: " << std::fixed << std::setprecision(2) << state.current_value
                  << " | Min: " << state.min_value
                  << " | Max: " << state.max_value
                  << " | Avg: " << avg
                  << " | Count: " << state.message_count;
        
        if (state.dropped_count > 0) {
            std::cout << " | ⚠️  DROPPED: " << state.dropped_count;
        }
        
        std::cout << "\n";
    }
    
    std::cout << std::flush;
}

int main() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << "[Monitor] Starting...\n";

    // ========== DDS INITIALIZATION ==========
    dds_entity_t participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    if (participant < 0) {
        std::cerr << "[ERROR] Failed to create DDS participant\n";
        return 1;
    }
    std::cout << "[DDS] Participant created\n";

    dds_entity_t topic = dds_create_topic(
        participant,
        &Telemetry_JsonMessage_desc,
        "lab_telemetry",
        NULL,
        NULL
    );
    if (topic < 0) {
        std::cerr << "[ERROR] Failed to create DDS topic\n";
        dds_delete(participant);
        return 1;
    }
    std::cout << "[DDS] Topic created\n";

    // QoS for reliable delivery with larger history
    dds_qos_t *qos = dds_create_qos();
    dds_qset_reliability(qos, DDS_RELIABILITY_RELIABLE, DDS_SECS(10));
    dds_qset_history(qos, DDS_HISTORY_KEEP_LAST, 100);

    dds_entity_t reader = dds_create_reader(participant, topic, qos, NULL);
    dds_delete_qos(qos);

    if (reader < 0) {
        std::cerr << "[ERROR] Failed to create DDS reader\n";
        dds_delete(topic);
        dds_delete(participant);
        return 1;
    }
    
    std::cout << "[DDS] Subscribed to 'lab_telemetry'\n";
    std::cout << "[Monitor] Listening for messages (Ctrl+C to stop)...\n\n";

    // Initialize timer
    g_last_dashboard_ms = get_current_time_ms();

    // ========== MAIN LOOP ==========
    int consecutive_empty_reads = 0;
    
    // Allocate sample buffer ONCE outside loop
    Telemetry_JsonMessage msg;
    void* samples[1];
    samples[0] = &msg;
    dds_sample_info_t infos[1];
    
    while(g_running) {
        // Clear the message structure
        memset(&msg, 0, sizeof(msg));
        
        int ret = dds_take(reader, samples, infos, 1, 1);
        
        if (ret > 0 && infos[0].valid_data) {
            consecutive_empty_reads = 0;
            
            // Check if payload is valid
            if (msg.payload == NULL) {
                std::cerr << "[ERROR] Received NULL payload\n";
                continue;
            }
            
            try {
                // Safely parse JSON
                nlohmann::json j = nlohmann::json::parse(msg.payload);
                
                int sensor_id = j["id"];
                double value = j["value"];
                uint64_t timestamp = j["timestamp"];
                uint64_t sequence = j["sequence"];

                {
                    std::lock_guard<std::mutex> lock(g_sensor_mutex);
                    
                    SensorState& state = g_sensors[sensor_id];
                    
                    // First message from this sensor
                    if (!state.initialized) {
                        std::cout << "[Info] Sensor " << sensor_id 
                                  << " - First message received (seq: " << sequence << ")\n";
                        state.expected_seq = sequence;
                        state.initialized = true;
                    } else {
                        // Check for dropped messages
                        if (sequence != state.expected_seq) {
                            uint64_t dropped = sequence - state.expected_seq;
                            state.dropped_count += dropped;
                            
                            std::cout << "⚠️  [WARNING] Sensor " << sensor_id 
                                      << " - Dropped " << dropped << " message(s)!\n"
                                      << "    Expected sequence: " << state.expected_seq 
                                      << ", Got: " << sequence << "\n";
                        }
                    }
                    
                    // Update state
                    state.expected_seq = sequence + 1;
                    state.message_count++;
                    state.current_value = value;
                    state.last_timestamp = timestamp;
                    
                    // Update statistics
                    if (value < state.min_value) state.min_value = value;
                    if (value > state.max_value) state.max_value = value;
                    state.sum_value += value;
                }
                
            } catch (const std::exception& e) {
                std::cerr << "[ERROR] Failed to parse message: " << e.what() << "\n";
            }
            
            // Return the loan
            dds_return_loan(reader, samples, ret);
        } else {
            consecutive_empty_reads++;
        }
        
        // Print dashboard (throttled inside function)
        print_dashboard();
        
        // Adaptive sleep - sleep longer if no data
        int sleep_ms = (consecutive_empty_reads > 20) ? 100 : 10;
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    }

    // ========== CLEANUP ==========
    std::cout << "\n[Monitor] Cleaning up...\n";
    dds_delete(reader);
    dds_delete(topic);
    dds_delete(participant);

    // Final summary
    std::cout << "\n========== Final Summary ==========\n";
    {
        std::lock_guard<std::mutex> lock(g_sensor_mutex);
        for (const auto& pair : g_sensors) {
            int id = pair.first;
            const SensorState& state = pair.second;
            
            double avg = (state.message_count > 0) ? (state.sum_value / state.message_count) : 0.0;
            
            std::cout << "Sensor " << id << ":\n"
                      << "  Total messages: " << state.message_count << "\n"
                      << "  Dropped: " << state.dropped_count << "\n"
                      << "  Min: " << std::fixed << std::setprecision(2) << state.min_value << "\n"
                      << "  Max: " << state.max_value << "\n"
                      << "  Avg: " << avg << "\n";
        }
    }

    std::cout << "[Monitor] Exited cleanly.\n";
    return 0;
}