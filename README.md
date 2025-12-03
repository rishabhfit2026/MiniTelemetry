# Mini Telemetry System over DDS

A multi-process C++ telemetry system demonstrating real-time sensor data publishing and monitoring using DDS (Data Distribution Service).

## 🎯 Features

- **Multi-threaded sensor simulation** - 3 concurrent sensor threads
- **Thread-safe communication** - Lock-based queue with condition variables
- **DDS publish/subscribe** - CycloneDDS with reliable QoS
- **Real-time statistics** - Min/Max/Avg computation per sensor
- **Sequence tracking** - Automatic dropped message detection
- **JSON serialization** - Structured data format
- **Comprehensive testing** - GoogleTest unit tests
- **Clean shutdown** - Graceful thread termination

## 🏗️ Architecture
```
┌─────────────────────────────────────────┐
│         Sensor Hub Process              │
│                                         │
│  ┌──────────┐  ┌──────────┐           │
│  │ Sensor 0 │  │ Sensor 1 │  ...      │
│  └────┬─────┘  └────┬─────┘           │
│       │             │                  │
│       └─────┬───────┘                  │
│             ▼                          │
│   ┌──────────────────┐                │
│   │ Thread-Safe Queue│                │
│   └────────┬─────────┘                │
│            ▼                           │
│   ┌──────────────────┐                │
│   │  DDS Publisher   │                │
│   └────────┬─────────┘                │
└────────────┼────────────────────────────┘
             │
        DDS Network
             │
             ▼
┌─────────────────────────────────────────┐
│         Monitor Process                  │
│                                         │
│   ┌──────────────────┐                │
│   │  DDS Subscriber  │                │
│   └────────┬─────────┘                │
│            ▼                           │
│   ┌──────────────────┐                │
│   │ Stats Processor  │                │
│   └────────┬─────────┘                │
│            ▼                           │
│   ┌──────────────────┐                │
│   │   Dashboard      │                │
│   └──────────────────┘                │
└─────────────────────────────────────────┘
```

## 📦 Prerequisites
```bash
# Ubuntu/Debian
sudo apt update
sudo apt install -y cmake g++ cyclonedds libcyclonedds-dev

# Verify installation
cmake --version  # Should be 3.15+
g++ --version    # Should support C++17
```

## 🔨 Build Instructions
```bash
# Clone the repository
git clone https://github.com/YOUR_USERNAME/MiniTelemetry.git
cd MiniTelemetry

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build (parallel compilation)
make -j4

# Optional: Build in Release mode for better performance
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j4
```

## 🚀 Usage

### Basic Usage

**Terminal 1 - Start Monitor:**
```bash
cd build/src/apps
./monitor_process
```

**Terminal 2 - Start Sensor Hub:**
```bash
cd build/src/apps
./sensor_hub_process
```

### Advanced Options

**Custom run duration:**
```bash
./sensor_hub_process --duration 30
```

**Add artificial delay (for testing race conditions):**
```bash
./sensor_hub_process --delay 50 --duration 20
```

**Show help:**
```bash
./sensor_hub_process --help
```

### Late-Joining Test

Verify DDS reliable QoS:

1. Start sensor hub first
2. Wait 5 seconds
3. Start monitor → It should receive new messages immediately

## 🧪 Running Tests
```bash
cd build

# Run all tests
ctest --verbose

# Or run test executable directly
./test_main

# Run specific test
./test_main --gtest_filter=QueueTest.BasicPushPop
```

## 📁 Project Structure
```
MiniTelemetry/
├── CMakeLists.txt           # Root CMake configuration
├── README.md                # This file
├── DESIGN.md                # Architecture documentation
├── src/
│   ├── core/                # Core libraries
│   │   ├── CMakeLists.txt
│   │   ├── telemetry.idl    # DDS message definition
│   │   ├── telemetry_types.h
│   │   ├── telemetry_core.h
│   │   ├── telemetry_core.cpp
│   │   ├── thread_safe_queue.h
│   │   └── thread_safe_queue.cpp
│   └── apps/                # Executable applications
│       ├── CMakeLists.txt
│       ├── sensor_hub.cpp   # Publisher process
│       └── monitor.cpp      # Subscriber process
├── tests/                   # Unit tests
│   ├── CMakeLists.txt
│   ├── test_main.cpp
│   └── test_queue.cpp
└── build/                   # Build artifacts (generated)
```

## 📊 Example Output

**Monitor Output:**
```
[Monitor] Starting...
[DDS] Subscribed to 'lab_telemetry'

=== TELEMETRY DASHBOARD ===
================================================================================
[Sensor 0] Value:   25.42 | Min:   20.46 | Max:   29.56 | Avg:   25.24 | Count:   42
[Sensor 1] Value:   28.57 | Min:   20.05 | Max:   29.68 | Avg:   24.87 | Count:   41
[Sensor 2] Value:   27.95 | Min:   20.21 | Max:   29.97 | Avg:   24.93 | Count:   41
```

**Sensor Hub Output:**
```
[Sensor Hub] Starting...
[DDS] Topic 'lab_telemetry' created
[Thread] Sensor 0 started
[Thread] Sensor 1 started
[Thread] Sensor 2 started
[DDS] Published #50 (Sensor 1, seq: 16)
[DDS] Published #100 (Sensor 2, seq: 33)

========== Summary ==========
Total messages published: 124
```

## 🔍 Technical Details

### Message Format (JSON)
```json
{
  "id": 0,
  "value": 25.42,
  "timestamp": 1764741649246,
  "sequence": 15
}
```

### DDS QoS Configuration
- **Reliability**: RELIABLE
- **History**: KEEP_LAST(10)
- **Topic**: `lab_telemetry`

### Thread Model
- **3 sensor threads**: Generate data every 100ms
- **1 main thread**: Publishes via DDS
- **Monitor thread**: Receives and processes via DDS

## ⚠️ Warnings & Alerts

The monitor detects:
- ⚠️ Dropped messages (sequence gaps)
- ⚠️ Stale data (time gaps > 500ms)
- ⚠️ Out-of-range values (< 19.0 or > 31.0)

## 🛠️ Troubleshooting

**Issue: "Failed to create DDS participant"**
```bash
# Check if CycloneDDS is installed
dpkg -l | grep cyclonedds

# Reinstall if needed
sudo apt install --reinstall libcyclonedds-dev
```

**Issue: Compilation errors**
```bash
# Clean rebuild
rm -rf build
mkdir build && cd build
cmake ..
make -j4
```

## 📄 License

MIT License - See LICENSE file for details

## 👤 Author

Rishabh - Mini Telemetry System Project

## 🙏 Acknowledgments

- CycloneDDS for DDS implementation
- nlohmann/json for JSON serialization
- GoogleTest for unit testing framework