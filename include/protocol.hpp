#pragma once

#include <cstdint>
#include <vector>
#include <cstddef>

namespace hwmon {
namespace protocol {

// Wire protocol v1 constants
constexpr uint8_t SYNC1     = 0xAA;
constexpr uint8_t SYNC2     = 0x55;
constexpr uint8_t VERSION   = 0x01;

// Builds a complete binary packet ready to transmit over serial.
// Layout (little-endian for multi-byte fields):
//   [SYNC1][SYNC2][ver][num_cores][overall][core0]...[coreN-1][cpu_temp][gpu_clock_lo][gpu_clock_hi][gpu_util][gpu_temp][fps][checksum]
//
// checksum = XOR of every byte from SYNC1 up to (but not including) the checksum byte itself.
std::vector<uint8_t> buildPacket(
    uint8_t overall_cpu,
    const std::vector<uint8_t>& per_core_cpu,   // one entry per logical core
    int8_t cpu_temp_c,
    uint16_t gpu_clock_mhz,
    uint8_t gpu_util,
    int8_t gpu_temp_c,
    uint8_t fps
);

} // namespace protocol
} // namespace hwmon
