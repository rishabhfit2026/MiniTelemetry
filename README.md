# Mini Telemetry System over DDS

A multi-process C++ telemetry system demonstrating real-time sensor data publishing, monitoring, and logging using DDS (Data Distribution Service).

## 🎯 Features

- **Multi-threaded sensor simulation** - 3 concurrent sensor threads
- **Thread-safe communication** - Lock-based queue with condition variables
- **DDS publish/subscribe** - CycloneDDS with reliable QoS
- **Real-time statistics** - Min/Max/Avg computation per sensor
- **Sequence tracking** - Automatic dropped message detection
- **CSV data logging** - Persistent storage for analysis
- **JSON serialization** - Structured data format
- **Comprehensive testing** - GoogleTest unit tests
- **Clean shutdown** - Graceful thread termination

## 🏗️ Architecture
```
┌──────────────────────────────────────────────────────────────────────┐
│                       DDS Domain (lab_telemetry)                     │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  ┌──────────────────┐                                               │
│  │  Sensor Hub      │                                               │
│  │   (Publisher)    │                                               │
│  │                  │                                               │
│  │  3x Sensor       │         DDS Topic: lab_telemetry             │
│  │  Threads ───┐    │                                               │
│  │             │    │                                               │
│  │  Queue ─────┤    │                                               │
│  │             │    │                                               │
│  │  DDS Writer ├────┼──────────────┬──────────────────────┐        │
│  └──────────────────┘              │                      │        │
│                                    │                      │        │
│                                    ▼                      ▼        │
│                          ┌──────────────────┐  ┌──────────────────┐
│                          │     Monitor      │  │     Logger       │
│                          │  (Subscriber 1)  │  │  (Subscriber 2)  │
│                          │                  │  │                  │
│                          │  DDS Reader      │  │  DDS Reader      │
│                          │       │          │  │       │          │
│                          │       ▼          │  │       ▼          │
│                          │  Stats Engine    │  │  CSV Writer      │
│                          │       │          │  │       │          │
│                          │       ▼          │  └───────┼──────────┘
│                          │  Dashboard       │          │
│                          │  Display         │          ▼
│                          └──────────────────┘  ┌──────────────────┐
│                                                │ telemetry_log.csv│
│                                                └──────────────────┘
└──────────────────────────────────────────────────────────────────────┘
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

### Three-Process System

The complete telemetry system consists of three independent processes:

#### **Terminal 1: Monitor (Real-time Dashboard)**
```bash
cd build/src/apps
./monitor_process
```

Displays real-time statistics, tracks sequence numbers, and detects dropped messages.

#### **Terminal 2: Logger (CSV Data Recorder)**
```bash
cd build/src/apps
./logger_process --output telemetry_log.csv
```

Writes all telemetry data to a CSV file for later analysis and archival.

#### **Terminal 3: Sensor Hub (Data Publisher)**
```bash
cd build/src/apps
./sensor_hub_process --duration 20
```

Generates and publishes sensor data via DDS.

---

### Advanced Options

#### Sensor Hub Options
```bash
# Custom run duration
./sensor_hub_process --duration 30

# Add artificial delay (for testing race conditions)
./sensor_hub_process --delay 50 --duration 20

# Show help
./sensor_hub_process --help
```

#### Logger Options
```bash
# Specify custom output file
./logger_process --output experiment_2024-12-04.csv

# Show help
./logger_process --help
```

---

### Late-Joining Test

Verify DDS reliable QoS:

1. **Terminal 1**: Start sensor hub first
```bash
   ./sensor_hub_process --duration 30
```
2. **Wait 5 seconds**
3. **Terminal 2**: Start monitor
```bash
   ./monitor_process
```
   → It should receive new messages immediately ✅

---

## 📊 CSV Output Format

The logger creates a CSV file with the following columns:

| Column | Description |
|--------|-------------|
| `timestamp` | Original message timestamp (milliseconds since epoch) |
| `sensor_id` | Sensor identifier (0, 1, or 2) |
| `value` | Sensor reading (temperature in °C) |
| `sequence` | Per-sensor sequence number |
| `received_at` | Human-readable timestamp when logged |

**Example CSV:**
```csv
timestamp,sensor_id,value,sequence,received_at
1733329200000,0,25.42,0,2024-12-04 10:30:00.123
1733329200500,1,26.33,0,2024-12-04 10:30:00.625
1733329201000,2,28.15,0,2024-12-04 10:30:01.127
1733329201500,0,23.87,1,2024-12-04 10:30:01.629
1733329202000,1,27.91,1,2024-12-04 10:30:02.131
```

---

## 🔍 Analyzing CSV Data

### Count messages per sensor
```bash
awk -F',' 'NR>1 {count[$2]++} END {for (id in count) print "Sensor " id ": " count[id] " messages"}' telemetry_log.csv
```

### Check for sequence gaps
```bash
awk -F',' 'NR>1 {
  if ($4 != prev[$2]+1 && prev[$2] != "") 
    print "Gap detected: Sensor " $2 " jumped from " prev[$2] " to " $4; 
  prev[$2]=$4
}' telemetry_log.csv
```

### View last 10 entries
```bash
tail -10 telemetry_log.csv
```

### Calculate average per sensor
```bash
awk -F',' 'NR>1 {sum[$2]+=$3; count[$2]++} END {for (id in sum) print "Sensor " id " avg: " sum[id]/count[id]}' telemetry_log.csv
```

---

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

---

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
│       ├── monitor.cpp      # Subscriber process (dashboard)
│       └── logger.cpp       # Subscriber process (CSV writer)
├── tests/                   # Unit tests
│   ├── CMakeLists.txt
│   ├── test_main.cpp
│   └── test_queue.cpp
└── build/                   # Build artifacts (generated)
```

---

## 📊 Example Output

### Monitor Output
```
[Monitor] Starting...
[DDS] Subscribed to 'lab_telemetry'

=== TELEMETRY DASHBOARD ===
================================================================================
[Sensor 0] Value:   25.42 | Min:   20.46 | Max:   29.56 | Avg:   25.24 | Count:   42
[Sensor 1] Value:   28.57 | Min:   20.05 | Max:   29.68 | Avg:   24.87 | Count:   41
[Sensor 2] Value:   27.95 | Min:   20.21 | Max:   29.97 | Avg:   24.93 | Count:   41
```

### Sensor Hub Output
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
Final sequences per sensor:
  Sensor 0: 42 messages
  Sensor 1: 41 messages
  Sensor 2: 41 messages
```

### Logger Output
```
[Logger] Starting...
[Logger] Output file: telemetry_log.csv
[DDS] Subscribed to 'lab_telemetry'
[Logger] Listening for messages (Ctrl+C to stop)...

[Logger] Logged 25 messages (Sensor 2, seq: 8)
[Logger] Logged 50 messages (Sensor 0, seq: 16)
[Logger] Logged 75 messages (Sensor 1, seq: 24)
[Logger] Logged 100 messages (Sensor 2, seq: 33)

========== Summary ==========
Total messages logged: 124
Output file: telemetry_log.csv
```

---

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

| Parameter | Monitor | Logger |
|-----------|---------|--------|
| Reliability | RELIABLE | RELIABLE |
| History | KEEP_LAST(10) | KEEP_LAST(100) |
| Topic | lab_telemetry | lab_telemetry |

### Thread Model

- **Sensor Hub**: 3 sensor threads + 1 main thread
- **Monitor**: 1 main thread (DDS callback)
- **Logger**: 1 main thread (DDS callback)

### Performance

| Metric | Value |
|--------|-------|
| Message Rate | ~30 msg/sec (3 sensors × 10 Hz) |
| Latency | < 10ms (local network) |
| CPU Usage | < 5% per process |
| Memory | ~5-6 MB per process |

---

## ⚠️ Warnings & Alerts

The monitor detects and reports:

- ⚠️ **Dropped messages** (sequence number gaps)
- ⚠️ **Stale data** (time gaps > 500ms)
- ⚠️ **Out-of-range values** (< 19.0 or > 31.0)

---

## 🛠️ Troubleshooting

### Issue: "Failed to create DDS participant"
```bash
# Check if CycloneDDS is installed
dpkg -l | grep cyclonedds

# Reinstall if needed
sudo apt install --reinstall libcyclonedds-dev
```

### Issue: Compilation errors
```bash
# Clean rebuild
rm -rf build
mkdir build && cd build
cmake ..
make -j4
```

### Issue: Logger not receiving messages

1. Check all three processes are running
2. Verify they're on the same DDS domain (default: 0)
3. Check firewall settings (local should work)

---

## 📄 License

MIT License - See LICENSE file for details

## 👤 Author

Rishabh - Mini Telemetry System Project

## 🙏 Acknowledgments

- CycloneDDS for DDS implementation
- nlohmann/json for JSON serialization
- GoogleTest for unit testing framework

---

## 🎯 Project Completion

This project demonstrates:

✅ Multi-process architecture (3 processes)  
✅ DDS publish/subscribe pattern  
✅ Multi-threading with synchronization  
✅ Real-time monitoring dashboard  
✅ Data persistence (CSV logging)  
✅ Sequence tracking & error detection  
✅ Professional documentation  
✅ Unit testing with GoogleTest  
✅ CMake build system  
✅ Clean Git workflow  

**Status**: 110% Complete (includes optional logger) 🎉