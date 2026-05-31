#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace hwmon {

// Reads CPU temperature in Celsius.
// Tries multiple common sources (hwmon + thermal zones).
class ThermalMonitor {
public:
    ThermalMonitor();

    // Returns temperature in °C or std::nullopt if nothing usable found.
    // The value is clamped to fit in signed char range for the protocol.
    std::optional<int> readCpuTempC();

private:
    struct Sensor {
        std::string temp_input_path;   // e.g. /sys/class/hwmon/hwmon2/temp1_input
        std::string label_path;        // optional
        std::string name;              // from name file
    };

    std::vector<Sensor> cpu_sensors_;
    bool scanned_ = false;

    void scanSensors();
    int readTempFromPath(const std::string& path) const; // returns millidegrees or -1
    static bool looksLikeCpuSensor(const Sensor& s);
};

} // namespace hwmon
