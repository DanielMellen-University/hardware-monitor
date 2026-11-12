#pragma once

#include <string>

namespace hwmon {

struct Config {
    std::string serial_device = "/dev/ttyUSB0";
    int baud = 115200;
    int interval_ms = 2000;
    std::string fps_file = "/run/hwmon-daemon/current_fps";
};

enum class ConfigParseResult {
    success,
    help,
    error
};

ConfigParseResult parseArgs(int argc, char* argv[], Config& config, std::string& error);

} // namespace hwmon
