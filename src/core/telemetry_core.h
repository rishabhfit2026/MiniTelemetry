#pragma once

namespace telemetry {

// Global shutdown flag functions
void request_shutdown();
bool should_shutdown();

} // namespace telemetry