#include <iostream>
#include <atomic>
#include <csignal>
#include <chrono>
#include <thread>
#include <map>
#include <iomanip>
#include <mutex>
#include <cstring>
#include <set>

#include <dds/dds.h>
#include "telemetry.h"
#include <nlohmann/json.hpp>

#include "../core/telemetry_types.h"

std::atomic<bool> g_running{true};

// Sensor metadata
struct SensorMetadata {
    std::string name;
    std::string unit;
};

const std::map<int, SensorMetadata> SENSOR_CONFIG = {
    {0, {"Temperature", "°C"}},
    {1, {"Pressure", "hPa"}},
    {2, {"Humidity", "%"}}
};

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
    uint64_t last_received_ms = 0;
    bool initialized = false;
    
    std::set<uint64_t> seen_sequences;
};

std::map<int, SensorState> g_sensors;
std::mutex g_sensor_mutex;

// Rate limiting
uint64_t g_last_print_ms = 0;
const uint64_t REFRESH_INTERVAL_MS = 200; // Update every 200ms
bool g_first_print = true;

void signal_handler(int signal) {
    std::cout << "\n[Monitor] Caught signal " << signal << ", shutting down...\n";
    g_running = false;
}

uint64_t get_current_time_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

std::string get_sensor_name(int id) {
    auto it = SENSOR_CONFIG.find(id);
    if (it != SENSOR_CONFIG.end()) {
        return it->second.name;
    }
    return "Unknown";
}

std::string get_sensor_unit(int id) {
    auto it = SENSOR_CONFIG.find(id);
    if (it != SENSOR_CONFIG.end()) {
        return it->second.unit;
    }
    return "";
}

void move_cursor_home() {
    // Move cursor to home position without clearing
    std::cout << "\033[H";
}

void clear_screen_once() {
    // Clear screen only on first print
    std::cout << "\033[2J\033[H";
}

void hide_cursor() {
    std::cout << "\033[?25l";
}

void show_cursor() {
    std::cout << "\033[?25h";
}

void print_dashboard() {
    std::lock_guard<std::mutex> lock(g_sensor_mutex);
    
    bool has_data = false;
    for (const auto& pair : g_sensors) {
        if (pair.second.message_count > 0) {
            has_data = true;
            break;
        }
    }
    
    if (!has_data) {
        return;
    }
    
    // Only clear screen on first print, then just move cursor home
    if (g_first_print) {
        clear_screen_once();
        g_first_print = false;
    } else {
        move_cursor_home();
    }

    std::cout << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                      LIVE TELEMETRY DASHBOARD                                ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════════════════╝\n\n";
    
    for (const auto& pair : g_sensors) {
        int id = pair.first;
        const SensorState& state = pair.second;
        
        if (state.message_count == 0) continue;
        
        double avg = state.sum_value / state.message_count;
        std::string name = get_sensor_name(id);
        std::string unit = get_sensor_unit(id);
        
        std::cout << "┌─ Sensor " << id << ": " << std::setw(11) << std::left << name << " ─────────────────────────────────────────────────────┐\n";
        std::cout << "│ Current: " << std::fixed << std::setprecision(2) 
                  << std::setw(8) << std::right << state.current_value << " " 
                  << std::setw(4) << std::left << unit;
        std::cout << " │ Min: " << std::setw(8) << state.min_value
                  << " │ Max: " << std::setw(8) << state.max_value
                  << " │ Avg: " << std::setw(8) << avg << " │\n";
        std::cout << "│ Messages: " << std::setw(5) << state.message_count;
        
        if (state.dropped_count > 0) {
            std::cout << " │ ⚠ DROPPED: " << std::setw(5) << state.dropped_count << " ";
        } else {
            std::cout << " │ ✓ No drops      ";
        }
        std::cout << "                                         │\n";
        std::cout << "└───────────────────────────────────────────────────────────────────────────┘\n";
    }
    
    std::cout << "\n";
    std::cout << "Last Update: " << std::fixed << std::setprecision(3) 
              << get_current_time_ms() / 1000.0 << "s | Press Ctrl+C to stop";
    
    // Add extra lines to clear any leftover content from previous prints
    std::cout << "\n\n\n\n";

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
    std::cout << "[Monitor] Waiting for data...\n\n";
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // Hide cursor for cleaner display
    hide_cursor();
    
    // ========== MAIN LOOP ==========
    Telemetry_JsonMessage msg;
    void* samples[1];
    samples[0] = &msg;
    dds_sample_info_t infos[1];
    
    bool data_updated = false;
    
    while(g_running) {
        memset(&msg, 0, sizeof(msg));
        
        int ret = dds_take(reader, samples, infos, 1, 1);
        
        if (ret > 0 && infos[0].valid_data) {
            if (msg.payload == NULL) {
                dds_return_loan(reader, samples, ret);
                continue;
            }
            
            try {
                nlohmann::json j = nlohmann::json::parse(msg.payload);
                int sensor_id = j["id"];
                double value = j["value"];
                uint64_t timestamp = j["timestamp"];
                uint64_t sequence = j["sequence"];
                
                uint64_t now_ms = get_current_time_ms();

                {
                    std::lock_guard<std::mutex> lock(g_sensor_mutex);
                    SensorState& state = g_sensors[sensor_id];
                    
                    if (state.seen_sequences.count(sequence) > 0) {
                        dds_return_loan(reader, samples, ret);
                        continue;
                    }
                    
                    state.seen_sequences.insert(sequence);
                    
                    if (!state.initialized) {
                        state.expected_seq = sequence;
                        state.initialized = true;
                    } else {
                        if (sequence != state.expected_seq) {
                            uint64_t dropped = sequence - state.expected_seq;
                            state.dropped_count += dropped;
                        }
                    }
                    
                    state.expected_seq = sequence + 1;
                    state.message_count++;
                    state.current_value = value;
                    state.last_timestamp = timestamp;
                    state.last_received_ms = now_ms;
                    
                    if (value < state.min_value) state.min_value = value;
                    if (value > state.max_value) state.max_value = value;
                    state.sum_value += value;
                    
                    data_updated = true;
                }
                
            } catch (const std::exception& e) {
                // Silently skip parse errors during live display
            }
            
            dds_return_loan(reader, samples, ret);
        }
        
        // Rate-limited printing
        uint64_t now_ms = get_current_time_ms();
        if (data_updated && (now_ms - g_last_print_ms) >= REFRESH_INTERVAL_MS) {
            print_dashboard();
            g_last_print_ms = now_ms;
            data_updated = false;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // ========== CLEANUP ==========
    show_cursor(); // Restore cursor
    clear_screen_once();
    
    std::cout << "[Monitor] Cleaning up...\n";
    dds_delete(reader);
    dds_delete(topic);
    dds_delete(participant);

    std::cout << "\n╔══════════════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                           FINAL SUMMARY                                      ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════════════════╝\n\n";
    
    {
        std::lock_guard<std::mutex> lock(g_sensor_mutex);
        for (const auto& pair : g_sensors) {
            int id = pair.first;
            const SensorState& state = pair.second;
            
            if (state.message_count == 0) continue;
            
            double avg = state.sum_value / state.message_count;
            
            std::cout << "Sensor " << id << " (" << get_sensor_name(id) << "):\n"
                      << "  Total messages: " << state.message_count << "\n"
                      << "  Dropped: " << state.dropped_count << "\n"
                      << "  Min: " << std::fixed << std::setprecision(2) 
                      << state.min_value << " " << get_sensor_unit(id) << "\n"
                      << "  Max: " << state.max_value << " " << get_sensor_unit(id) << "\n"
                      << "  Avg: " << avg << " " << get_sensor_unit(id) << "\n\n";
        }
    }

    std::cout << "[Monitor] Exited cleanly.\n";
    return 0;
}