#include "telemetry_types.h"
#include <atomic>
#include <iostream>

namespace telemetry {

std::atomic<bool> g_shutdown_flag{false};

void request_shutdown() {
    g_shutdown_flag.store(true, std::memory_order_release);
    std::cout << "[Core] Shutdown requested\n";
}

bool should_shutdown() {
    return g_shutdown_flag.load(std::memory_order_acquire);
}

} // namespace telemetry