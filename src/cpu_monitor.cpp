#include "cpu_monitor.hpp"

#include <fstream>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>

namespace hwmon {

namespace {

bool parseCpuLine(const std::string& line, CpuTimes& times) {
    std::istringstream iss(line);
    std::string cpu_label;
    iss >> cpu_label;

    if (!(iss >> times.user >> times.nice >> times.system >> times.idle >>
               times.iowait >> times.irq >> times.softirq >> times.steal)) {
        return false;
    }
    return true;
}

} // anonymous namespace

CpuMonitor::CpuMonitor() = default;

bool CpuMonitor::initialize() {
    std::vector<CpuTimes> times;
    uint8_t dummy_overall;
    if (!readProcStat(times, dummy_overall)) {
        return false;
    }

    if (times.size() < 2) { // at least overall + 1 core
        return false;
    }

    // times[0] is overall "cpu", times[1..] are cpu0, cpu1...
    core_count_ = times.size() - 1;
    prev_per_core_.assign(times.begin() + 1, times.end());
    prev_overall_ = times[0];
    initialized_ = true;

    // Small sleep so first real read has a delta
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return true;
}

bool CpuMonitor::getLoads(uint8_t& overall, std::vector<uint8_t>& per_core) {
    if (!initialized_) {
        if (!initialize()) return false;
    }

    std::vector<CpuTimes> curr_times;
    uint8_t raw_overall_unused;
    if (!readProcStat(curr_times, raw_overall_unused)) {
        return false;
    }

    if (curr_times.size() != prev_per_core_.size() + 1) {
        // Core count changed (hotplug) - re-init
        initialized_ = false;
        return getLoads(overall, per_core);
    }

    // Overall
    CpuTimes curr_overall = curr_times[0];
    overall = calculateLoadPercent(prev_overall_, curr_overall);
    prev_overall_ = curr_overall;

    // Per core
    per_core.resize(core_count_);
    for (size_t i = 0; i < core_count_; ++i) {
        per_core[i] = calculateLoadPercent(prev_per_core_[i], curr_times[i + 1]);
        prev_per_core_[i] = curr_times[i + 1];
    }

    return true;
}

bool CpuMonitor::readProcStat(std::vector<CpuTimes>& out_times, uint8_t& /*overall_raw*/) {
    std::ifstream file("/proc/stat");
    if (!file.is_open()) {
        return false;
    }

    out_times.clear();
    std::string line;

    while (std::getline(file, line)) {
        if (line.rfind("cpu", 0) != 0) {
            break; // cpu lines are first
        }

        CpuTimes t{};
        if (parseCpuLine(line, t)) {
            out_times.push_back(t);
        }
    }

    return !out_times.empty();
}

uint8_t CpuMonitor::calculateLoadPercent(const CpuTimes& prev, const CpuTimes& curr) {
    return calculate_cpu_load_percent(prev, curr);
}

uint8_t calculate_cpu_load_percent(const CpuTimes& prev, const CpuTimes& curr) {
    auto prev_total = prev.user + prev.nice + prev.system + prev.idle + prev.iowait +
                      prev.irq + prev.softirq + prev.steal;

    auto curr_total = curr.user + curr.nice + curr.system + curr.idle + curr.iowait +
                      curr.irq + curr.softirq + curr.steal;

    auto prev_idle = prev.idle + prev.iowait;
    auto curr_idle = curr.idle + curr.iowait;

    uint64_t total_delta = curr_total - prev_total;
    uint64_t idle_delta  = curr_idle  - prev_idle;

    if (total_delta == 0) return 0;

    uint64_t used = total_delta - idle_delta;
    // Avoid overflow / weird values
    if (used > total_delta) used = total_delta;

    int percent = static_cast<int>((used * 100ULL) / total_delta);
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    return static_cast<uint8_t>(percent);
}

} // namespace hwmon
