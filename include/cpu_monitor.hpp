#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <chrono>

namespace hwmon {

struct CpuTimes {
    uint64_t user = 0;
    uint64_t nice = 0;
    uint64_t system = 0;
    uint64_t idle = 0;
    uint64_t iowait = 0;
    uint64_t irq = 0;
    uint64_t softirq = 0;
    uint64_t steal = 0;
};

class CpuMonitor {
public:
    CpuMonitor();

    // Must be called at least once before getLoads().
    bool initialize();

    // Returns number of logical CPU cores detected.
    size_t getCoreCount() const { return core_count_; }

    // Computes current load percentages (0-100).
    // overall: 0-100
    // per_core: size == core_count_, each 0-100
    bool getLoads(uint8_t& overall, std::vector<uint8_t>& per_core);

private:
    bool readProcStat(std::vector<CpuTimes>& out_times, uint8_t& overall_raw);

    static uint8_t calculateLoadPercent(const CpuTimes& prev, const CpuTimes& curr);

    std::vector<CpuTimes> prev_per_core_;
    CpuTimes prev_overall_;
    size_t core_count_ = 0;
    bool initialized_ = false;
};

// Pure function exposed for testing and reuse.
// Calculates CPU usage percentage (0-100) between two snapshots.
uint8_t calculate_cpu_load_percent(const CpuTimes& prev, const CpuTimes& curr);

} // namespace hwmon

