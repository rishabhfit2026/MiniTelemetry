#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>
#include <cstring>
#include <cstdlib>
#include <csignal>
#include <map>
#include <mutex>

// DDS headers
#include <dds/dds.h>
#include "telemetry.h"

// Include nlohmann json library
#include <nlohmann/json.hpp>

// Include our core files
#include "../core/telemetry_types.h"
#include "../core/thread_safe_queue.h"

// Global flag for threads to check
std::atomic<bool> g_running{true};
std::atomic<uint64_t> g_message_count{0};

// The shared queue (Thread-safe!)
ThreadSafeQueue<SensorData> g_data_queue;

// Global delay configuration
int g_artificial_delay_ms = 0;

// PER-SENSOR sequence tracking
std::map<int, uint64_t> g_sensor_sequences;
std::mutex g_sequence_mutex;

// Signal handler for graceful shutdown
void signal_handler(int signal) {
    std::cout << "\n[Sensor Hub] Caught signal " << signal << ", shutting down...\n";
    g_running = false;
    g_data_queue.stop();
}

// Sensor thread function - each sensor generates different data
void sensor_thread_func(int id) {
    //1.setup random number generations
    std::random_device rd;
    std::mt19937 gen(rd());
    
    // Different sensors simulate different physical quantities
    std::uniform_real_distribution<> dis;
    std::string sensor_type;
    
    switch(id) {
        case 0:  // Temperature sensor (°C)
            dis = std::uniform_real_distribution<>(20.0, 30.0);
            sensor_type = "Temperature";
            break;
        case 1:  // Pressure sensor (hPa) 
            dis = std::uniform_real_distribution<>(1000.0, 1020.0);
            sensor_type = "Pressure";
            break;
        case 2:  // Humidity sensor (%)
            dis = std::uniform_real_distribution<>(40.0, 60.0);
            sensor_type = "Humidity";
            break;
        default:
            dis = std::uniform_real_distribution<>(0.0, 100.0);
            sensor_type = "Generic";
    }

    std::cout << "[Thread] Sensor " << id << " (" << sensor_type << ") started\n";

    while(g_running) {
        SensorData data;
        data.id = id;
        //generate the fake values
        data.value = dis(gen);  // Random value within sensor's range
        //generate the timstamp
        data.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

        g_data_queue.push(data);

        // Sleep 500ms (2 Hz per sensor = 6 messages/sec total)
        int sleep_time = 500 + g_artificial_delay_ms;
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
    }
    
    std::cout << "[Thread] Sensor " << id << " (" << sensor_type << ") stopped\n";
}

void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [OPTIONS]\n";
    std::cout << "Options:\n";
    std::cout << "  --delay <ms>     Add artificial delay to sensor threads (for testing race conditions)\n";
    std::cout << "  --duration <sec> Run duration in seconds (default: infinite, use Ctrl+C to stop)\n";
    std::cout << "  --help           Show this help message\n";
    std::cout << "\nExample:\n";
    std::cout << "  " << prog_name << " --delay 50 --duration 60\n";
    std::cout << "  " << prog_name << "  # Runs indefinitely until Ctrl+C\n";
}

int main(int argc, char** argv) {
    int run_duration_sec = -1;  // -1 = infinite by default
    
    // Setup signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--delay" && i + 1 < argc) {
            g_artificial_delay_ms = std::atoi(argv[++i]);
            std::cout << "[Config] Artificial delay: " << g_artificial_delay_ms << "ms\n";
        } else if (arg == "--duration" && i + 1 < argc) {
            run_duration_sec = std::atoi(argv[++i]);
            std::cout << "[Config] Run duration: " << run_duration_sec << " seconds\n";
        } else {
            std::cerr << "[ERROR] Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    std::cout << "[Sensor Hub] Starting...\n";
    
    if (run_duration_sec == -1) {
        std::cout << "[Config] Running indefinitely (press Ctrl+C to stop)\n";
    }

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
    std::cout << "[DDS] Topic 'lab_telemetry' created\n";

    // Set QoS for reliable delivery with larger history
    dds_qos_t *qos = dds_create_qos();
    dds_qset_reliability(qos, DDS_RELIABILITY_RELIABLE, DDS_SECS(10));
    dds_qset_history(qos, DDS_HISTORY_KEEP_LAST, 100);

    dds_entity_t writer = dds_create_writer(participant, topic, qos, NULL);
    dds_delete_qos(qos);
    
    if (writer < 0) {
        std::cerr << "[ERROR] Failed to create DDS writer\n";
        dds_delete(topic);
        dds_delete(participant);
        return 1;
    }
    std::cout << "[DDS] Writer created with reliable QoS\n";

    // Initialize per-sensor sequences
    for(int i = 0; i < 3; ++i) {
        g_sensor_sequences[i] = 0;
    }

    // ========== START SENSOR THREADS ==========
    std::cout << "[Sensor Hub] Starting 3 sensor threads...\n";
    std::vector<std::thread> sensors;
    for(int i = 0; i < 3; ++i) {
        sensors.emplace_back(sensor_thread_func, i);
    }

    // Small delay to let threads start
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // ========== MAIN LOOP (Publisher) ==========
    SensorData incoming_data;
    auto start_time = std::chrono::steady_clock::now();

    std::cout << "[Main] Publishing data...\n";

    while(g_running) {
        if(g_data_queue.pop(incoming_data)) {
            // Get and increment the per-sensor sequence
            uint64_t sequence;
            {
                std::lock_guard<std::mutex> lock(g_sequence_mutex);
                sequence = g_sensor_sequences[incoming_data.id]++;
            }
            
            // Create JSON payload with PER-SENSOR sequence number
            nlohmann::json j;
            j["id"] = incoming_data.id;
            j["value"] = incoming_data.value;
            j["timestamp"] = incoming_data.timestamp;
            j["sequence"] = sequence;


            //serilization : COnert the in-memory json object 'j' into a string 
            std::string json_str = j.dump();//<<--cool this is serialization step Mr.

            // Create DDS message
            Telemetry_JsonMessage msg;
            msg.payload = dds_string_dup(json_str.c_str());

            // Publish via DDS-
            int ret = dds_write(writer, &msg);
            if (ret == DDS_RETCODE_OK) {
                g_message_count++;
                
                // Print every 25th message to reduce spam
                if (g_message_count % 25 == 0) {
                    std::cout << "[DDS] Published #" << g_message_count 
                              << " (Sensor " << incoming_data.id 
                              << ", seq: " << sequence << ")\n";
                }
            } else {
                std::cerr << "[ERROR] Failed to publish message (code: " << ret << ")\n";
            }

            // Free DDS allocated string
            dds_string_free(msg.payload);
        } else {
            // Queue is empty, brief sleep
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // Check timeout (if duration was specified)
        if (run_duration_sec > 0) {
            auto elapsed = std::chrono::steady_clock::now() - start_time;
            if (elapsed > std::chrono::seconds(run_duration_sec)) {
                std::cout << "[Main] Timeout reached (" << run_duration_sec 
                          << " seconds), shutting down...\n";
                g_running = false;
                g_data_queue.stop();
            }
        }
    }

    // ========== CLEANUP ==========
    std::cout << "[Main] Stopping sensor threads...\n";
    for(auto& t : sensors) {
        if(t.joinable()) t.join();
    }

    std::cout << "[DDS] Cleaning up...\n";
    dds_delete(writer);
    dds_delete(topic);
    dds_delete(participant);

    std::cout << "\n========== Summary ==========\n";
    std::cout << "Total messages published: " << g_message_count.load() << "\n";
    
    // Print final sequences per sensor
    std::cout << "Final sequences per sensor:\n";
    for (const auto& pair : g_sensor_sequences) {
        std::cout << "  Sensor " << pair.first << ": " << pair.second << " messages\n";
    }
    
    std::cout << "[Sensor Hub] Exited cleanly.\n";
    return 0;
}