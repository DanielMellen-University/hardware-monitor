#include "protocol.hpp"
#include <cstddef>

namespace hwmon {
namespace protocol {

std::vector<uint8_t> buildPacket(
    uint8_t overall_cpu,
    const std::vector<uint8_t>& per_core_cpu,
    int8_t cpu_temp_c,
    uint16_t gpu_clock_mhz,
    uint8_t gpu_util,
    int8_t gpu_temp_c,
    uint8_t fps)
{
    const uint8_t num_cores = static_cast<uint8_t>(per_core_cpu.size());

    std::vector<uint8_t> pkt;
    pkt.reserve(12 + num_cores);

    // Header
    pkt.push_back(SYNC1);
    pkt.push_back(SYNC2);
    pkt.push_back(VERSION);
    pkt.push_back(num_cores);
    pkt.push_back(overall_cpu);

    // Per-core loads
    pkt.insert(pkt.end(), per_core_cpu.begin(), per_core_cpu.end());

    // CPU temp
    pkt.push_back(static_cast<uint8_t>(cpu_temp_c));

    // GPU clock (LE)
    pkt.push_back(static_cast<uint8_t>(gpu_clock_mhz & 0xFF));
    pkt.push_back(static_cast<uint8_t>((gpu_clock_mhz >> 8) & 0xFF));

    // GPU util + temp
    pkt.push_back(gpu_util);
    pkt.push_back(static_cast<uint8_t>(gpu_temp_c));

    // FPS
    pkt.push_back(fps);

    // Compute checksum (XOR of everything so far)
    uint8_t checksum = 0;
    for (uint8_t b : pkt) {
        checksum ^= b;
    }
    pkt.push_back(checksum);

    return pkt;
}

} // namespace protocol
} // namespace hwmon
