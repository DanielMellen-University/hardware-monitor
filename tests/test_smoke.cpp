#include <doctest/doctest.h>

#include "cpu_monitor.hpp"
#include "thermal_monitor.hpp"
#include "protocol.hpp"
#include "framerate_provider.hpp"

#include <thread>
#include <chrono>
#include <fstream>
#include <cstdio>     // for std::remove
#include <unistd.h>   // for getpid()

TEST_SUITE("System Smoke Tests (real hardware)") {

    TEST_CASE("CpuMonitor can initialize and report reasonable load on this machine") {
        hwmon::CpuMonitor cpu;
        REQUIRE(cpu.initialize());

        size_t cores = cpu.getCoreCount();
        CHECK(cores >= 1);
        CHECK(cores <= 256);   // sanity

        uint8_t overall = 0;
        std::vector<uint8_t> per_core;

        // Give it a moment
        std::this_thread::sleep_for(std::chrono::milliseconds(150));

        bool ok = cpu.getLoads(overall, per_core);
        REQUIRE(ok);

        CHECK(overall <= 100);
        CHECK(per_core.size() == cores);

        for (auto c : per_core) {
            CHECK(c <= 100);
        }
    }

    TEST_CASE("ThermalMonitor returns plausible CPU temperature or nothing") {
        hwmon::ThermalMonitor thermal;
        auto temp = thermal.readCpuTempC();

        if (temp.has_value()) {
            // On real hardware this should be between -10 and 110 or so
            CHECK(*temp >= -10);
            CHECK(*temp <= 110);
        } else {
            // Acceptable on some minimal systems
            MESSAGE("No CPU temperature sensor detected on this system");
        }
    }

    TEST_CASE("Full packet roundtrip with real CPU + thermal data") {
        hwmon::CpuMonitor cpu;
        REQUIRE(cpu.initialize());

        uint8_t overall = 0;
        std::vector<uint8_t> per_core;
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
        REQUIRE(cpu.getLoads(overall, per_core));

        hwmon::ThermalMonitor thermal;
        int8_t cpu_temp = 0;
        if (auto t = thermal.readCpuTempC()) {
            cpu_temp = static_cast<int8_t>(*t);
        }

        // Use dummy GPU + FPS for this test (user has no dGPU)
        auto pkt = hwmon::protocol::buildPacket(
            overall,
            per_core,
            cpu_temp,
            0,      // no GPU
            0,
            0,
            0
        );

        REQUIRE(pkt.size() == 12 + per_core.size());

        // Basic sanity on packet
        CHECK(pkt[0] == 0xAA);
        CHECK(pkt[1] == 0x55);
        CHECK(pkt[3] == per_core.size());
        CHECK(pkt[4] == overall);
    }

    TEST_CASE("FramerateProvider can be updated externally during test") {
        // Use a temp file unique to this test
        std::string path = "/tmp/hwmon_smoke_fps_" + std::to_string(getpid());

        {
            std::ofstream f(path);
            f << "60";
        }

        hwmon::FramerateProvider fps(path);
        std::this_thread::sleep_for(std::chrono::milliseconds(550));

        CHECK(fps.getFps() == 60);

        // Update it
        {
            std::ofstream f(path);
            f << "165";
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(550));

        CHECK(fps.getFps() == 165);

        std::remove(path.c_str());
    }
}
