#include "config.hpp"

#include <charconv>
#include <getopt.h>
#include <string>
#include <system_error>

namespace hwmon {
namespace {

bool parseInteger(const char* text, int& value) {
    if (!text || *text == '\0') return false;
    const char* end = text;
    while (*end != '\0') ++end;
    const auto result = std::from_chars(text, end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

bool supportedBaud(int baud) {
    switch (baud) {
        case 9600:
        case 19200:
        case 38400:
        case 57600:
        case 115200:
        case 230400:
        case 460800:
        case 921600:
            return true;
        default:
            return false;
    }
}

} // namespace

ConfigParseResult parseArgs(int argc, char* argv[], Config& config, std::string& error) {
    static const struct option long_options[] = {
        {"serial", required_argument, nullptr, 's'},
        {"baud", required_argument, nullptr, 'b'},
        {"interval", required_argument, nullptr, 'i'},
        {"fps-file", required_argument, nullptr, 'f'},
        {"help", no_argument, nullptr, 'h'},
        {nullptr, 0, nullptr, 0}
    };

    config = Config{};
    error.clear();
    optind = 1;
    const int previous_opterr = opterr;
    opterr = 0;

    ConfigParseResult result = ConfigParseResult::success;
    int option = 0;
    while ((option = getopt_long(argc, argv, "s:b:i:f:h", long_options, nullptr)) != -1) {
        switch (option) {
            case 's':
                config.serial_device = optarg;
                break;
            case 'b':
                if (!parseInteger(optarg, config.baud)) {
                    error = "Invalid baud rate: " + std::string(optarg);
                    result = ConfigParseResult::error;
                } else if (!supportedBaud(config.baud)) {
                    error = "Unsupported baud rate: " + std::to_string(config.baud);
                    result = ConfigParseResult::error;
                }
                break;
            case 'i':
                if (!parseInteger(optarg, config.interval_ms)) {
                    error = "Invalid interval: " + std::string(optarg);
                    result = ConfigParseResult::error;
                } else if (config.interval_ms <= 0) {
                    error = "Interval must be greater than zero";
                    result = ConfigParseResult::error;
                }
                break;
            case 'f':
                config.fps_file = optarg;
                break;
            case 'h':
                result = ConfigParseResult::help;
                break;
            default:
                error = "Unknown option or missing option value";
                result = ConfigParseResult::error;
                break;
        }
        if (result != ConfigParseResult::success) break;
    }

    if (result == ConfigParseResult::success && optind < argc) {
        error = "Unexpected positional argument: " + std::string(argv[optind]);
        result = ConfigParseResult::error;
    }

    opterr = previous_opterr;
    return result;
}

} // namespace hwmon
