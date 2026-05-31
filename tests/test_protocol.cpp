#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "protocol.hpp"
#include <cstdint>
#include <vector>

using namespace hwmon::protocol;

TEST_SUITE("Protocol Packet Builder") {

    TEST_CASE("packet has correct sync bytes and version") {
        std::vector<uint8_t> per_core = {10, 20, 30};
        auto pkt = buildPacket(42, per_core, 55, 1500, 75, 60, 144);

        REQUIRE(pkt.size() >= 4);
        CHECK(pkt[0] == 0xAA);
        CHECK(pkt[1] == 0x55);
        CHECK(pkt[2] == 0x01);  // VERSION
    }

    TEST_CASE("packet size is 12 + num_cores") {
        std::vector<uint8_t> cores_4 = {1,2,3,4};
        auto pkt4 = buildPacket(50, cores_4, 40, 1000, 30, 45, 60);

        CHECK(pkt4.size() == 12 + 4);   // 16 bytes

        std::vector<uint8_t> cores_12(12, 5);
        auto pkt12 = buildPacket(60, cores_12, 50, 2000, 80, 70, 120);
        CHECK(pkt12.size() == 12 + 12); // 24 bytes
    }

    TEST_CASE("num_cores field matches input vector size") {
        std::vector<uint8_t> cores{5, 10, 15, 20, 25, 30};
        auto pkt = buildPacket(55, cores, 38, 800, 40, 55, 90);

        CHECK(pkt[3] == 6);  // num_cores at offset 3
    }

    TEST_CASE("all scalar fields are placed correctly") {
        std::vector<uint8_t> per_core = {99};
        auto pkt = buildPacket(88, per_core, -12, 0xABCD, 66, -5, 77);

        // Layout after sync + ver + num_cores:
        // overall at [4]
        CHECK(pkt[4] == 88);

        // first core at [5]
        CHECK(pkt[5] == 99);

        // cpu_temp at [6]
        CHECK(static_cast<int8_t>(pkt[6]) == -12);

        // gpu_clock LE at [7][8]
        CHECK(pkt[7] == 0xCD);
        CHECK(pkt[8] == 0xAB);

        // gpu_util [9]
        CHECK(pkt[9] == 66);

        // gpu_temp [10]
        CHECK(static_cast<int8_t>(pkt[10]) == -5);

        // fps [11]
        CHECK(pkt[11] == 77);
    }

    TEST_CASE("checksum is valid XOR of all preceding bytes") {
        std::vector<uint8_t> per_core = {10, 20};
        auto pkt = buildPacket(30, per_core, 40, 500, 50, 60, 70);

        uint8_t expected_xor = 0;
        for (size_t i = 0; i < pkt.size() - 1; ++i) {
            expected_xor ^= pkt[i];
        }

        CHECK(pkt.back() == expected_xor);
    }

    TEST_CASE("different core counts produce valid packets") {
        for (int n = 1; n <= 32; ++n) {
            std::vector<uint8_t> cores(n, 42);
            auto pkt = buildPacket(55, cores, 35, 1200, 65, 50, 100);

            REQUIRE(pkt.size() == static_cast<size_t>(12 + n));
            CHECK(pkt[3] == static_cast<uint8_t>(n));
            // Last byte must be valid checksum
            uint8_t xor_val = 0;
            for (size_t i = 0; i < pkt.size() - 1; ++i) xor_val ^= pkt[i];
            CHECK(pkt.back() == xor_val);
        }
    }

    TEST_CASE("edge values are preserved (0, 100, -127, 65535, 255)") {
        std::vector<uint8_t> per_core = {0, 100};
        auto pkt = buildPacket(0, per_core, -127, 65535, 100, 127, 255);

        CHECK(pkt[4] == 0);
        CHECK(pkt[5] == 0);
        CHECK(pkt[6] == 100);

        CHECK(static_cast<int8_t>(pkt[7]) == -127);
        CHECK(pkt[8] == 0xFF);
        CHECK(pkt[9] == 0xFF);
        CHECK(pkt[10] == 100);
        CHECK(static_cast<int8_t>(pkt[11]) == 127);
        CHECK(pkt[12] == 255);
    }
}
