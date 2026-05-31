#pragma once

#include <cstdint>
#include <string>
#include <optional>
#include <memory>

namespace hwmon {

// GPU statistics for the primary GPU (index 0).
struct GpuStats {
    uint16_t clock_mhz = 0;     // graphics/core clock
    uint8_t  utilization = 0;   // 0-100
    int8_t   temperature_c = 0; // -127..127, negative or 0 means unavailable
    bool     available = false;
};

class GpuMonitor {
public:
    GpuMonitor();
    ~GpuMonitor();

    // Returns stats for primary GPU. Populates available=false if none found.
    GpuStats getStats();

private:
    // NVML dynamic loading (no build dependency)
    struct NvmlFuncs;
    std::unique_ptr<NvmlFuncs> nvml_;

    bool nvml_available_ = false;
    void* nvml_lib_ = nullptr;

    bool tryInitNvml();
    void shutdownNvml();

    GpuStats readViaNvml();
    GpuStats readViaNvidiaSmi();   // fallback using popen
};

} // namespace hwmon
