#include <doctest/doctest.h>

#include "gpu_monitor.hpp"

TEST_SUITE("nvidia-smi parser") {

    TEST_CASE("parses a normal CSV row with surrounding whitespace") {
        hwmon::GpuStats stats;
        REQUIRE(hwmon::parse_nvidia_smi_line(" 1725, 48, 67\r\n", stats));
        CHECK(stats.available);
        CHECK(stats.clock_mhz == 1725);
        CHECK(stats.utilization == 48);
        CHECK(stats.temperature_c == 67);
    }

    TEST_CASE("clamps parsed values to protocol field limits") {
        hwmon::GpuStats stats;
        REQUIRE(hwmon::parse_nvidia_smi_line("70000, 240, 200", stats));
        CHECK(stats.clock_mhz == 65535);
        CHECK(stats.utilization == 100);
        CHECK(stats.temperature_c == 127);
    }

    TEST_CASE("rejects missing, malformed, and unavailable fields") {
        hwmon::GpuStats stats;
        CHECK_FALSE(hwmon::parse_nvidia_smi_line("N/A, 0, 40", stats));
        CHECK_FALSE(stats.available);
        CHECK_FALSE(hwmon::parse_nvidia_smi_line("1000, 80", stats));
        CHECK_FALSE(hwmon::parse_nvidia_smi_line("1000, 80x, 40", stats));
        CHECK_FALSE(hwmon::parse_nvidia_smi_line("1000, 80, 40, extra", stats));
    }

    TEST_CASE("command failures and empty output produce unavailable GPU data") {
        hwmon::GpuStats stats;
        CHECK_FALSE(hwmon::parse_nvidia_smi_result("1000, 80, 40\n", false, stats));
        CHECK_FALSE(stats.available);
        CHECK_FALSE(hwmon::parse_nvidia_smi_result("", true, stats));
        CHECK_FALSE(stats.available);
        CHECK_FALSE(hwmon::parse_nvidia_smi_result("not a CSV row\n", true, stats));
        CHECK_FALSE(stats.available);

        REQUIRE(hwmon::parse_nvidia_smi_result("1000, 80, 40\n", true, stats));
        CHECK(stats.available);
        CHECK(stats.clock_mhz == 1000);
    }
}
