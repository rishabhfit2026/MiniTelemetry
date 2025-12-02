#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>
#include <dds/dds.h> // Keep DDS for later

// Include our core files
#include "../core/telemetry_types.h"
#include "../core/thread_safe_queue.h"

// Global flag for threads to check
std::atomic<bool> g_running{true};

// The shared queue (Thread-safe!)
ThreadSafeQueue<SensorData> g_data_queue;

// This function runs inside a background thread
void sensor_thread_func(int id) {
    // std::cout << "[Sensor " << id << "] Started.\n";
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(20.0, 30.0);

    while(g_running) {
        SensorData data;
        data.id = id;
        data.value = dis(gen);
        data.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

        g_data_queue.push(data);

        // Sleep a bit
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int main(int argc, char** argv) {
    std::cout << "Starting Sensor Hub (JSON Mode)...\n";

    // 1. Start 3 Sensor Threads
    std::vector<std::thread> sensors;
    for(int i = 0; i < 3; ++i) {
        sensors.emplace_back(sensor_thread_func, i);
    }

    // 2. Main Loop (The Consumer)
    SensorData incoming_data;
    auto start_time = std::chrono::steady_clock::now();

    while(g_running) {
        // Wait for data
        if(g_data_queue.pop(incoming_data)) {
            // --- JSON SERIALIZATION HAPPENS HERE ---
            nlohmann::json j = incoming_data;
            std::string message_payload = j.dump(); 

            std::cout << "Sending: " << message_payload << "\n";
        } else {
            break; 
        }

        // Run for approx 5 seconds then stop
        if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(5)) {
            g_running = false;
            g_data_queue.stop();
        }
    }

    // 3. Cleanup Threads
    for(auto& t : sensors) {
        if(t.joinable()) t.join();
    }

    std::cout << "Sensor Hub Exited cleanly.\n";
    return 0;
}