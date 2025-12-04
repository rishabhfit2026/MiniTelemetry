#include <iostream>
#include <fstream>
#include <atomic>
#include <csignal>
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>

#include <dds/dds.h>
#include "telemetry.h"
#include <nlohmann/json.hpp>

#include "../core/telemetry_types.h"

std::atomic<bool> g_running{true};
std::atomic<uint64_t> g_total_logged{0};

void signal_handler(int signal) {
    std::cout << "\n[Logger] Caught signal " << signal << ", shutting down...\n";
    g_running = false;
}

std::string get_timestamp_string() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

int main(int argc, char** argv) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Parse command line arguments
    std::string output_file = "telemetry_log.csv";
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--output" && i + 1 < argc) {
            output_file = argv[++i];
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [OPTIONS]\n";
            std::cout << "Options:\n";
            std::cout << "  --output <file>  Output CSV file (default: telemetry_log.csv)\n";
            std::cout << "  --help           Show this help message\n";
            return 0;
        }
    }

    std::cout << "[Logger] Starting...\n";
    std::cout << "[Logger] Output file: " << output_file << "\n";

    // Open CSV file for writing
    std::ofstream csv_file(output_file);
    if (!csv_file.is_open()) {
        std::cerr << "[ERROR] Failed to open output file: " << output_file << "\n";
        return 1;
    }

    // Write CSV header
    csv_file << "timestamp,sensor_id,value,sequence,received_at\n";
    csv_file.flush();
    std::cout << "[Logger] CSV header written\n";

    // ========== DDS INITIALIZATION ==========
    dds_entity_t participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    if (participant < 0) {
        std::cerr << "[ERROR] Failed to create DDS participant\n";
        csv_file.close();
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
        csv_file.close();
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
        csv_file.close();
        return 1;
    }
    
    std::cout << "[DDS] Subscribed to 'lab_telemetry'\n";
    std::cout << "[Logger] Listening for messages (Ctrl+C to stop)...\n\n";

    // ========== MAIN LOOP ==========
    Telemetry_JsonMessage msg;
    void* samples[1];
    samples[0] = &msg;
    dds_sample_info_t infos[1];
    
    auto last_flush = std::chrono::steady_clock::now();
    constexpr int FLUSH_INTERVAL_MS = 1000; // Flush every second
    
    while(g_running) {
        memset(&msg, 0, sizeof(msg));
        
        int ret = dds_take(reader, samples, infos, 1, 1);
        
        if (ret > 0 && infos[0].valid_data) {
            if (msg.payload == NULL) {
                std::cerr << "[ERROR] Received NULL payload\n";
                continue;
            }
            
            try {
                nlohmann::json j = nlohmann::json::parse(msg.payload);
                
                int sensor_id = j["id"];
                double value = j["value"];
                uint64_t timestamp = j["timestamp"];
                uint64_t sequence = j["sequence"];
                
                std::string received_at = get_timestamp_string();
                
                // Write to CSV: timestamp,sensor_id,value,sequence,received_at
                csv_file << timestamp << ","
                         << sensor_id << ","
                         << std::fixed << std::setprecision(2) << value << ","
                         << sequence << ","
                         << received_at << "\n";
                
                g_total_logged++;
                
                // Print progress every 25 messages
                if (g_total_logged % 25 == 0) {
                    std::cout << "[Logger] Logged " << g_total_logged 
                              << " messages (Sensor " << sensor_id 
                              << ", seq: " << sequence << ")\n";
                }
                
            } catch (const std::exception& e) {
                std::cerr << "[ERROR] Failed to parse message: " << e.what() << "\n";
            }
            
            dds_return_loan(reader, samples, ret);
        }
        
        // Periodic flush to ensure data is written to disk
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_flush
        ).count();
        
        if (elapsed > FLUSH_INTERVAL_MS) {
            csv_file.flush();
            last_flush = now;
        }
        
        // Small sleep to avoid busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // ========== CLEANUP ==========
    std::cout << "\n[Logger] Cleaning up...\n";
    
    // Final flush
    csv_file.flush();
    csv_file.close();
    
    dds_delete(reader);
    dds_delete(topic);
    dds_delete(participant);

    std::cout << "\n========== Summary ==========\n";
    std::cout << "Total messages logged: " << g_total_logged.load() << "\n";
    std::cout << "Output file: " << output_file << "\n";
    std::cout << "[Logger] Exited cleanly.\n";
    
    return 0;
}