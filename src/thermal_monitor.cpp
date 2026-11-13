#include "thermal_monitor.hpp"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <utility>

namespace fs = std::filesystem;

namespace hwmon {

ThermalMonitor::ThermalMonitor(std::string hwmon_root, std::string thermal_root)
    : hwmon_root_(std::move(hwmon_root)), thermal_root_(std::move(thermal_root)) {}

void ThermalMonitor::scanSensors() {
    if (scanned_) return;
    cpu_sensors_.clear();
    bool scan_complete = true;

    // 1. hwmon sensors (preferred)
    try {
        if (fs::exists(hwmon_root_)) {
            for (const auto& entry : fs::directory_iterator(hwmon_root_)) {
                if (!entry.is_directory()) continue;

                std::string base = entry.path().string();

                // Read name
                std::string name;
                std::ifstream namef(base + "/name");
                if (namef) std::getline(namef, name);

                // Look for temp*_input files
                for (int i = 1; i <= 16; ++i) {
                    std::string input = base + "/temp" + std::to_string(i) + "_input";
                    if (!fs::exists(input)) continue;

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
    } catch (const fs::filesystem_error&) {
        // Keep searching the fallback source and retry discovery on the next read.
        scan_complete = false;
    }

    // 2. Thermal zones fallback
    if (cpu_sensors_.empty()) {
        try {
            if (fs::exists(thermal_root_)) {
                for (const auto& entry : fs::directory_iterator(thermal_root_)) {
                    if (!entry.is_directory()) continue;
                    std::string dir = entry.path().string();
                    if (dir.find("thermal_zone") == std::string::npos) continue;

                    std::string type_path = dir + "/type";
                    std::string temp_path = dir + "/temp";

                    if (!fs::exists(temp_path)) continue;

                    std::string type;
                    std::ifstream tf(type_path);
                    if (tf) std::getline(tf, type);
                    std::transform(type.begin(), type.end(), type.begin(),
                                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

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
        } catch (const fs::filesystem_error&) {
            // Retain any fallback candidates found so far and retry next time.
            scan_complete = false;
        }
    }

    scanned_ = scan_complete;
}

bool ThermalMonitor::looksLikeCpuSensor(const Sensor& s) {
    std::string label;
    if (!s.label_path.empty()) {
        std::ifstream lf(s.label_path);
        if (lf) std::getline(lf, label);
    }

    std::string lower_name = s.name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    std::string lower_label = label;
    std::transform(lower_label.begin(), lower_label.end(), lower_label.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

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

std::optional<int> ThermalMonitor::readTempFromPath(const std::string& path) const {
    std::ifstream f(path);
    if (!f) return std::nullopt;

    int millideg = 0;
    if (!(f >> millideg)) return std::nullopt;
    char trailing = 0;
    if (f >> trailing) return std::nullopt;
    return millideg;
}

std::optional<int> ThermalMonitor::readCpuTempC() {
    scanSensors();

    std::optional<int> best_temp;

    for (const auto& s : cpu_sensors_) {
        const auto millideg = readTempFromPath(s.temp_input_path);
        if (!millideg) continue;

        const int celsius = *millideg / 1000;
        if (!best_temp || celsius > *best_temp) best_temp = celsius;
    }

    if (!best_temp) return std::nullopt;

    // Clamp for signed char protocol field
    if (*best_temp > 127) best_temp = 127;
    if (*best_temp < -127) best_temp = -127;

    return *best_temp;
}

} // namespace hwmon
