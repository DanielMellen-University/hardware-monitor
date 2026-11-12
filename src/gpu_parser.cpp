#include "gpu_monitor.hpp"

#include <charconv>
#include <cctype>
#include <string_view>

namespace hwmon {
namespace {

std::string_view trim(std::string_view value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.remove_suffix(1);
    }
    return value;
}

bool parseUnsigned(std::string_view text, unsigned int& value) {
    text = trim(text);
    if (text.empty()) return false;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

} // namespace

bool parse_nvidia_smi_line(const std::string& line, GpuStats& stats) {
    stats = GpuStats{};
    const std::string_view row(line);
    const size_t first_comma = row.find(',');
    if (first_comma == std::string_view::npos) return false;
    const size_t second_comma = row.find(',', first_comma + 1);
    if (second_comma == std::string_view::npos ||
        row.find(',', second_comma + 1) != std::string_view::npos) {
        return false;
    }

    unsigned int clock = 0;
    unsigned int utilization = 0;
    unsigned int temperature = 0;
    if (!parseUnsigned(row.substr(0, first_comma), clock) ||
        !parseUnsigned(row.substr(first_comma + 1, second_comma - first_comma - 1), utilization) ||
        !parseUnsigned(row.substr(second_comma + 1), temperature)) {
        return false;
    }

    stats.clock_mhz = (clock > 65535) ? 65535 : static_cast<uint16_t>(clock);
    stats.utilization = (utilization > 100) ? 100 : static_cast<uint8_t>(utilization);
    stats.temperature_c = (temperature > 127) ? 127 : static_cast<int8_t>(temperature);
    stats.available = true;
    return true;
}

} // namespace hwmon
