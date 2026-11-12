#include <doctest/doctest.h>

#include "cpu_monitor.hpp"

using hwmon::CpuTimes;

TEST_SUITE("CPU Load Calculation") {

    TEST_CASE("parses aggregate and numbered CPU rows with optional counters") {
        CpuTimes aggregate{99, 99, 99, 99, 99, 99, 99, 99};
        REQUIRE(hwmon::parse_cpu_stat_line("cpu 10 2 3 40", aggregate));
        CHECK(aggregate.user == 10);
        CHECK(aggregate.nice == 2);
        CHECK(aggregate.system == 3);
        CHECK(aggregate.idle == 40);
        CHECK(aggregate.iowait == 0);
        CHECK(aggregate.steal == 0);

        CpuTimes core{};
        REQUIRE(hwmon::parse_cpu_stat_line("cpu12 1 2 3 4 5 6 7 8", core));
        CHECK(core.iowait == 5);
        CHECK(core.irq == 6);
        CHECK(core.softirq == 7);
        CHECK(core.steal == 8);
    }

    TEST_CASE("rejects malformed CPU row labels and counters") {
        CpuTimes parsed{};
        CHECK_FALSE(hwmon::parse_cpu_stat_line("cpufoo 1 2 3 4", parsed));
        CHECK_FALSE(hwmon::parse_cpu_stat_line("cpu0 1 2 invalid 4", parsed));
    }

    TEST_CASE("idle system produces 0% load") {
        // Only idle + iowait increase. All busy counters stay exactly the same.
        CpuTimes prev{1200, 80, 350, 9500, 180, 15, 25, 8};
        CpuTimes curr{1200, 80, 350, 10200, 260, 15, 25, 8};

        uint8_t load = hwmon::calculate_cpu_load_percent(prev, curr);
        CHECK(load == 0);
    }

    TEST_CASE("fully busy system produces ~100% load") {
        CpuTimes prev{1000, 50, 300, 100, 10, 5, 5, 0};
        CpuTimes curr{2000, 100, 600, 100, 10, 5, 5, 0};  // almost no idle growth

        uint8_t load = hwmon::calculate_cpu_load_percent(prev, curr);
        CHECK(load >= 98);  // allow tiny rounding
        CHECK(load <= 100);
    }

    TEST_CASE("exactly 50% load") {
        CpuTimes prev{1000, 0, 0, 1000, 0, 0, 0, 0};
        CpuTimes curr{1500, 0, 0, 1500, 0, 0, 0, 0};

        uint8_t load = hwmon::calculate_cpu_load_percent(prev, curr);
        CHECK(load == 50);
    }

    TEST_CASE("handles zero total delta gracefully") {
        CpuTimes same{500, 10, 20, 1000, 5, 1, 2, 0};
        uint8_t load = hwmon::calculate_cpu_load_percent(same, same);
        CHECK(load == 0);
    }

    TEST_CASE("handles counters decreasing after a CPU reset") {
        // user drops while idle grows, so the unsigned busy delta would wrap
        // and report a fully loaded CPU instead of an unknown sample.
        CpuTimes prev{1000, 0, 0, 1000, 0, 0, 0, 0};
        CpuTimes curr{900, 0, 0, 1300, 0, 0, 0, 0};

        CHECK(hwmon::calculate_cpu_load_percent(prev, curr) == 0);
    }

    TEST_CASE("ignores steal time correctly in calculation") {
        CpuTimes prev{1000, 0, 200, 2000, 0, 0, 0, 50};
        CpuTimes curr{1100, 0, 250, 2050, 0, 0, 0, 150}; // steal increased a lot

        // steal counts as used time
        uint8_t load = hwmon::calculate_cpu_load_percent(prev, curr);
        CHECK(load > 0);
    }

    TEST_CASE("clamps output to 0-100 even with weird numbers") {
        CpuTimes prev{0,0,0,0,0,0,0,0};
        CpuTimes curr{100000, 0, 0, 0, 0, 0, 0, 0};

        uint8_t load = hwmon::calculate_cpu_load_percent(prev, curr);
        CHECK(load == 100);
    }

    TEST_CASE("typical mixed workload gives reasonable result") {
        CpuTimes prev{12000, 300, 4500, 35000, 800, 120, 90, 50};
        CpuTimes curr{12550, 305, 4570, 35550, 815, 122, 92, 52};

        uint8_t load = hwmon::calculate_cpu_load_percent(prev, curr);
        // Should be in a plausible low-to-mid range for this delta
        CHECK(load >= 15);
        CHECK(load <= 60);
    }
}
