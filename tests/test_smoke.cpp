#include <doctest/doctest.h>

#include "protocol.hpp"

#include <cstdint>
#include <vector>

TEST_CASE("synthetic readings decode from a complete protocol packet") {
    const std::vector<uint8_t> cores{12, 34, 56};
    const auto packet = hwmon::protocol::buildPacket(78, cores, -12, 0x3456, 91, -7, 144);

    REQUIRE(packet.size() == 12 + cores.size());
    CHECK(packet[0] == hwmon::protocol::SYNC1);
    CHECK(packet[1] == hwmon::protocol::SYNC2);
    CHECK(packet[2] == hwmon::protocol::VERSION);
    CHECK(packet[3] == cores.size());
    CHECK(packet[4] == 78);
    CHECK(packet[5] == 12);
    CHECK(packet[6] == 34);
    CHECK(packet[7] == 56);
    CHECK(static_cast<int8_t>(packet[8]) == -12);
    CHECK(packet[9] == 0x56);
    CHECK(packet[10] == 0x34);
    CHECK(packet[11] == 91);
    CHECK(static_cast<int8_t>(packet[12]) == -7);
    CHECK(packet[13] == 144);

    uint8_t decoded_checksum = 0;
    for (size_t i = 0; i + 1 < packet.size(); ++i) decoded_checksum ^= packet[i];
    CHECK(packet.back() == decoded_checksum);
}
