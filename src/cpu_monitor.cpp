#include "cpu_monitor.hpp"

#include <fstream>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <cctype>
#include <algorithm>

namespace hwmon {

bool parse_cpu_stat_line(const std::string& line, CpuTimes& times) {
    times = {};
    std::istringstream iss(line);
    std::string cpu_label;
    if (!(iss >> cpu_label) || cpu_label.rfind("cpu", 0) != 0) {
        return false;
    }
    if (cpu_label != "cpu" &&
        !std::all_of(cpu_label.begin() + 3, cpu_label.end(),
                     [](unsigned char ch) { return std::isdigit(ch) != 0; })) {
        return false;
    }

    if (!(iss >> times.user >> times.nice >> times.system >> times.idle)) {
        return false;
    }

    uint64_t* optional_fields[] = {
        &times.iowait, &times.irq, &times.softirq, &times.steal
    };
    for (uint64_t* field : optional_fields) {
        if (!(iss >> *field)) {
            if (iss.eof()) break;
            return false;
        }
    }
    return true;
}

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
        if (parse_cpu_stat_line(line, t)) {
            out_times.push_back(t);
        }
    }

    return !out_times.empty();
}

uint8_t CpuMonitor::calculateLoadPercent(const CpuTimes& prev, const CpuTimes& curr) {
    return calculate_cpu_load_percent(prev, curr);
}

uint8_t calculate_cpu_load_percent(const CpuTimes& prev, const CpuTimes& curr) {
    if (curr.user < prev.user || curr.nice < prev.nice ||
        curr.system < prev.system || curr.idle < prev.idle ||
        curr.iowait < prev.iowait || curr.irq < prev.irq ||
        curr.softirq < prev.softirq || curr.steal < prev.steal) {
        return 0;
    }

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
