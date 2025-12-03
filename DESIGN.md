# Design Document - Mini Telemetry System

## 1. Overview

A multi-process C++ telemetry system demonstrating sensor data publishing and monitoring using DDS middleware.

## 2. System Architecture

### 2.1 Process Model
```
┌─────────────────────┐          ┌─────────────────────┐
│   Sensor Hub        │          │      Monitor        │
│    (Publisher)      │          │    (Subscriber)     │
│                     │          │                     │
│  ┌───────────────┐ │          │  ┌───────────────┐ │
│  │ Sensor Thread │ ├─┐        │  │  DDS Reader   │ │
│  └───────────────┘ │ │        │  └───────┬───────┘ │
│  ┌───────────────┐ │ │        │          │         │
│  │ Sensor Thread │ ├─┤        │  ┌───────▼───────┐ │
│  └───────────────┘ │ │        │  │ Stats Engine  │ │
│  ┌───────────────┐ │ │   DDS  │  └───────┬───────┘ │
│  │ Sensor Thread │ ├─┼────────┼──────────┤         │
│  └───────────────┘ │ │Network │  ┌───────▼───────┐ │
│          │         │ │        │  │   Dashboard   │ │
│  ┌───────▼───────┐ │ │        │  └───────────────┘ │
│  │  Queue        │ │ │        │                     │
│  └───────┬───────┘ │ │        └─────────────────────┘
│          │         │ │
│  ┌───────▼───────┐ │ │
│  │  DDS Writer   │─┘─┘
│  └───────────────┘ │
└─────────────────────┘
```

### 2.2 Component Responsibilities

#### Sensor Hub
- Spawns 3 sensor threads
- Each thread generates random telemetry (20-30°C)
- Pushes data to thread-safe queue
- Main thread publishes via DDS

#### Monitor
- Subscribes to DDS topic
- Tracks sequence numbers
- Computes rolling statistics
- Displays real-time dashboard

## 3. Threading Model

### 3.1 Sensor Hub Threads
```cpp
Main Thread                 Sensor Thread 0         Sensor Thread 1
    │                             │                       │
    ├─ Init DDS                   │                       │
    ├─ Create Topic               │                       │
    ├─ Create Writer              │                       │
    │                             │                       │
    ├─ spawn ────────────────────>│                       │
    ├─ spawn ─────────────────────────────────────────────>│
    │                             │                       │
    │                             ├─ Generate data        │
    │                             ├─ queue.push()         │
    │                             ├─ sleep(100ms)         ├─ Generate data
    │                             │                       ├─ queue.push()
    ├─ queue.pop()                │                       ├─ sleep(100ms)
    ├─ Serialize JSON             │                       │
    ├─ dds_write()                │                       │
    │                             │                       │
    ├─ [timeout reached]          │                       │
    ├─ g_running = false          │                       │
    ├─ queue.stop()               │                       │
    │                             │                       │
    │                             ├─ exit loop            ├─ exit loop
    ├─ join() <───────────────────┘                       │
    ├─ join() <───────────────────────────────────────────┘
    │
    └─ Cleanup & exit
```

### 3.2 Synchronization Primitives

| Primitive | Usage | Location |
|-----------|-------|----------|
| `std::atomic<bool>` | Shutdown flag | `g_running` |
| `std::atomic<uint64_t>` | Message counter | `g_message_count` |
| `std::mutex` | Queue protection | `ThreadSafeQueue::mutex_` |
| `std::condition_variable` | Queue signaling | `ThreadSafeQueue::cond_var_` |

## 4. Data Flow

### 4.1 Message Structure
```cpp
struct SensorData {
    int id;             // Sensor identifier (0, 1, 2)
    double value;       // Reading value (20.0 - 30.0)
    long timestamp;     // Milliseconds since epoch
    uint64_t sequence;  // Per-sensor sequence number
};
```

### 4.2 Serialization
```
SensorData ──> JSON ──> DDS Message ──> Network ──> DDS Message ──> JSON ──> Processing
```

Example JSON:
```json
{
  "id": 1,
  "value": 25.42,
  "timestamp": 1764741649246,
  "sequence": 15
}
```

## 5. DDS Configuration

### 5.1 Topic Definition (IDL)
```idl
module Telemetry {
    @appendable
    struct JsonMessage {
        string payload;
    };
};
```

### 5.2 QoS Settings

| Parameter | Value | Reason |
|-----------|-------|--------|
| Reliability | RELIABLE | Ensure no message loss |
| History | KEEP_LAST(10) | Balance memory/latency |
| Domain | DEFAULT (0) | Standard configuration |

## 6. Error Handling & Detection

### 6.1 Dropped Message Detection
```
Expected Sequence: N
Received Sequence: N+k (k > 1)
→ Dropped: k-1 messages
```

### 6.2 Stale Data Detection
```
Current Timestamp - Last Timestamp > 500ms
→ Warning: Data staleness
```

### 6.3 Range Validation
```
Value < 19.0 || Value > 31.0
→ Warning: Out of range
```

## 7. Performance Characteristics

| Metric | Value |
|--------|-------|
| Message Rate | ~30 msg/sec (3 sensors × 10 Hz) |
| Latency | < 10ms (local) |
| Queue Depth | Dynamic (unbounded) |
| History Depth | 10 messages (DDS) |

## 8. Testing Strategy

### 8.1 Unit Tests
- Queue push/pop operations
- JSON serialization round-trip
- Sequence number tracking

### 8.2 Integration Tests
- Multi-threaded producer/consumer
- DDS pub/sub communication
- Late-joining subscriber

### 8.3 Stress Tests
- Artificial delay injection
- High message rates
- Extended run duration

## 9. Future Enhancements

- [ ] CSV logger process
- [ ] Configurable sensor count
- [ ] Web-based dashboard
- [ ] Persistent storage
- [ ] Alert system
- [ ] Performance metrics

## 10. Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| CycloneDDS | Latest | DDS middleware |
| nlohmann/json | 3.11.2 | JSON serialization |
| GoogleTest | 1.14.0 | Unit testing |
| C++ Standard | 17 | Core language |

## 11. Build System

- **CMake 3.15+**: Cross-platform build
- **Out-of-tree builds**: Clean separation
- **FetchContent**: Dependency management
- **CTest**: Test integration