#include "thermal_monitor.hpp"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <iostream>

namespace fs = std::filesystem;

namespace hwmon {

ThermalMonitor::ThermalMonitor() = default;

void ThermalMonitor::scanSensors() {
    if (scanned_) return;
    scanned_ = true;
    cpu_sensors_.clear();

    // 1. hwmon sensors (preferred)
    const std::string hwmon_root = "/sys/class/hwmon";
    if (fs::exists(hwmon_root)) {
        for (const auto& entry : fs::directory_iterator(hwmon_root)) {
            if (!entry.is_directory()) continue;

            std::string base = entry.path().string();

            // Read name
            std::string name;
            std::ifstream namef(base + "/name");
            if (namef) std::getline(namef, name);

            // Look for temp*_input files
            for (int i = 1; i <= 16; ++i) {
                std::string input = base + "/temp" + std::to_string(i) + "_input";
                if (!fs::exists(input)) break;

                Sensor s;
                s.temp_input_path = input;
                s.label_path = base + "/temp" + std::to_string(i) + "_label";
                s.name = name;

                if (looksLikeCpuSensor(s)) {
                    cpu_sensors_.push_back(s);
                }
            }
        }
    }

    // 2. Thermal zones fallback
    const std::string tz_root = "/sys/class/thermal";
    if (fs::exists(tz_root) && cpu_sensors_.empty()) {
        for (const auto& entry : fs::directory_iterator(tz_root)) {
            if (!entry.is_directory()) continue;
            std::string dir = entry.path().string();
            if (dir.find("thermal_zone") == std::string::npos) continue;

            std::string type_path = dir + "/type";
            std::string temp_path = dir + "/temp";

            if (!fs::exists(temp_path)) continue;

            std::string type;
            std::ifstream tf(type_path);
            if (tf) std::getline(tf, type);

            // Common CPU-related types
            if (type.find("cpu") != std::string::npos ||
                type.find("x86_pkg") != std::string::npos ||
                type.find("acpitz") != std::string::npos ||
                type.find("coretemp") != std::string::npos) {

                Sensor s;
                s.temp_input_path = temp_path;
                s.name = type;
                cpu_sensors_.push_back(s);
            }
        }
    }

    // If still nothing, add any temp1_input from hwmon as last resort
    if (cpu_sensors_.empty() && fs::exists(hwmon_root)) {
        for (const auto& entry : fs::directory_iterator(hwmon_root)) {
            if (!entry.is_directory()) continue;
            std::string input = entry.path().string() + "/temp1_input";
            if (fs::exists(input)) {
                Sensor s{input, "", ""};
                cpu_sensors_.push_back(s);
                break; // only one fallback
            }
        }
    }
}

bool ThermalMonitor::looksLikeCpuSensor(const Sensor& s) {
    std::string label;
    if (!s.label_path.empty()) {
        std::ifstream lf(s.label_path);
        if (lf) std::getline(lf, label);
    }

    std::string lower_name = s.name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
    std::string lower_label = label;
    std::transform(lower_label.begin(), lower_label.end(), lower_label.begin(), ::tolower);

    // Very common patterns
    if (lower_name.find("coretemp") != std::string::npos) return true;
    if (lower_name.find("k10temp") != std::string::npos) return true;
    if (lower_name.find("zenpower") != std::string::npos) return true;
    if (lower_name.find("cpu") != std::string::npos) return true;

    if (lower_label.find("package") != std::string::npos) return true;
    if (lower_label.find("tdie") != std::string::npos) return true;
    if (lower_label.find("tctl") != std::string::npos) return true;
    if (lower_label.find("cpu") != std::string::npos) return true;

    return false;
}

int ThermalMonitor::readTempFromPath(const std::string& path) const {
    std::ifstream f(path);
    if (!f) return -1;

    int millideg = -1;
    f >> millideg;
    return millideg;
}

std::optional<int> ThermalMonitor::readCpuTempC() {
    scanSensors();

    int best_temp = -1000;

    for (const auto& s : cpu_sensors_) {
        int millideg = readTempFromPath(s.temp_input_path);
        if (millideg > 0) {
            int c = millideg / 1000;
            if (c > best_temp) best_temp = c;
        }
    }

    if (best_temp <= -1000) {
        return std::nullopt;
    }

    // Clamp for signed char protocol field
    if (best_temp > 127) best_temp = 127;
    if (best_temp < -127) best_temp = -127;

    return best_temp;
}

} // namespace hwmon
