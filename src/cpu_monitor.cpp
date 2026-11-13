#include "cpu_monitor.hpp"

#include <fstream>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <cctype>
#include <algorithm>
#include <utility>

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
    for (uint64_t* field : optional_fields) *field = 0;
    for (uint64_t* field : optional_fields) {
        // Stop only when the row has really ended; a value that is present but
        // fails to convert (for example an overflow at the end of the line) is an error.
        iss >> std::ws;
        if (iss.eof()) break;
        if (!(iss >> *field)) return false;
    }
    return true;
}

CpuMonitor::CpuMonitor(std::string proc_stat_path)
    : proc_stat_path_(std::move(proc_stat_path)) {}

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

    // A usable snapshot always has the aggregate row and at least one core.
    // Do not let an incomplete sample trigger a topology rebase.
    if (curr_times.size() < 2) {
        return false;
    }

    if (curr_times.size() != prev_per_core_.size() + 1) {
        // Rebase counters on the new topology. Loads are undefined for this
        // transition sample, so report zero and calculate deltas next time.
        core_count_ = curr_times.size() - 1;
        prev_overall_ = curr_times[0];
        prev_per_core_.assign(curr_times.begin() + 1, curr_times.end());
        overall = 0;
        per_core.assign(core_count_, 0);
        return true;
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
    std::ifstream file(proc_stat_path_);
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

    const uint64_t deltas[] = {
        curr.user - prev.user, curr.nice - prev.nice,
        curr.system - prev.system, curr.idle - prev.idle,
        curr.iowait - prev.iowait, curr.irq - prev.irq,
        curr.softirq - prev.softirq, curr.steal - prev.steal
    };
    long double total_delta = 0;
    for (uint64_t delta : deltas) total_delta += static_cast<long double>(delta);
    if (total_delta == 0) return 0;

    const long double idle_delta = static_cast<long double>(deltas[3]) +
                                   static_cast<long double>(deltas[4]);
    const long double used = total_delta - idle_delta;
    int percent = static_cast<int>((used * 100.0L) / total_delta);
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    return static_cast<uint8_t>(percent);
}

} // namespace hwmon
