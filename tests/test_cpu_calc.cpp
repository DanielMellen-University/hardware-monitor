#include <doctest/doctest.h>

#include <limits>
#include <filesystem>
#include <fstream>
#include <cstdlib>

#include "cpu_monitor.hpp"

using hwmon::CpuTimes;

namespace {

class CpuStatFile {
public:
    CpuStatFile() {
        char pattern[] = "/tmp/hwmon-cpu-test-XXXXXX";
        if (char* created = mkdtemp(pattern)) path_ = created;
    }
    ~CpuStatFile() {
        if (!path_.empty()) std::filesystem::remove_all(path_);
    }
    bool write(const std::string& contents) const {
        std::ofstream file(path_ / "stat");
        file << contents;
        return file.good();
    }
    std::string path() const { return (path_ / "stat").string(); }
    bool valid() const { return !path_.empty(); }

private:
    std::filesystem::path path_;
};

} // namespace

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

    TEST_CASE("CPU snapshots need an aggregate row and at least one core") {
        CpuStatFile snapshot;
        REQUIRE(snapshot.valid());
        REQUIRE(snapshot.write("cpu 10 0 5 100\n"));
        hwmon::CpuMonitor aggregate_only(snapshot.path());
        CHECK_FALSE(aggregate_only.initialize());

        REQUIRE(snapshot.write("\nmeminfo 1\n"));
        hwmon::CpuMonitor malformed(snapshot.path());
        CHECK_FALSE(malformed.initialize());

        REQUIRE(snapshot.write(""));
        hwmon::CpuMonitor empty(snapshot.path());
        CHECK_FALSE(empty.initialize());

        REQUIRE(snapshot.write("cpu0 10 0 5 100\ncpu1 10 0 5 100\n"));
        hwmon::CpuMonitor numbered_only(snapshot.path());
        CHECK_FALSE(numbered_only.initialize());
    }

    TEST_CASE("retries CPU initialization when proc stat becomes available") {
        CpuStatFile snapshot;
        REQUIRE(snapshot.valid());
        hwmon::CpuMonitor monitor(snapshot.path());
        CHECK_FALSE(monitor.initialize());

        REQUIRE(snapshot.write("cpu 100 0 20 300\ncpu0 50 0 10 150\n"));
        uint8_t overall = 0;
        std::vector<uint8_t> per_core;
        REQUIRE(monitor.getLoads(overall, per_core));
        CHECK(monitor.getCoreCount() == 1);
        CHECK(per_core.size() == 1);
    }

    TEST_CASE("CPU topology changes rebase once and resize synthetic core loads") {
        CpuStatFile snapshot;
        REQUIRE(snapshot.valid());
        REQUIRE(snapshot.write("cpu 100 0 20 300\ncpu0 50 0 10 150\ncpu1 50 0 10 150\n"));
        hwmon::CpuMonitor monitor(snapshot.path());
        REQUIRE(monitor.initialize());
        CHECK(monitor.getCoreCount() == 2);

        REQUIRE(snapshot.write("cpu 110 0 22 330\ncpu0 54 0 11 165\ncpu1 53 0 11 165\ncpu2 3 0 0 8\n"));
        uint8_t overall = 99;
        std::vector<uint8_t> per_core;
        REQUIRE(monitor.getLoads(overall, per_core));
        CHECK(overall == 0);
        CHECK(monitor.getCoreCount() == 3);
        REQUIRE(per_core.size() == 3);
        CHECK(per_core == std::vector<uint8_t>{0, 0, 0});

        REQUIRE(snapshot.write("cpu 120 0 24 360\ncpu0 58 0 12 180\ncpu1 56 0 12 180\ncpu2 4 0 0 9\n"));
        // The next sample is measured against the rebased three-core snapshot.
        REQUIRE(monitor.getLoads(overall, per_core));
        CHECK(overall == 28);
        CHECK(per_core == std::vector<uint8_t>{25, 21, 50});
    }

    TEST_CASE("rejects aggregate-only samples without losing the established core count") {
        CpuStatFile snapshot;
        REQUIRE(snapshot.valid());
        REQUIRE(snapshot.write("cpu 100 0 20 300\ncpu0 50 0 10 150\n"));
        hwmon::CpuMonitor monitor(snapshot.path());
        REQUIRE(monitor.initialize());
        REQUIRE(monitor.getCoreCount() == 1);

        REQUIRE(snapshot.write("cpu 110 0 22 330\n"));
        uint8_t overall = 0;
        std::vector<uint8_t> per_core;
        CHECK_FALSE(monitor.getLoads(overall, per_core));
        CHECK(monitor.getCoreCount() == 1);
    }

    TEST_CASE("rejects malformed CPU row labels and counters") {
        CpuTimes parsed{};
        CHECK_FALSE(hwmon::parse_cpu_stat_line("cpufoo 1 2 3 4", parsed));
        CHECK_FALSE(hwmon::parse_cpu_stat_line("cpu0 1 2 invalid 4", parsed));
        CHECK_FALSE(hwmon::parse_cpu_stat_line("cpu 1 2 3 4 18446744073709551616", parsed));
        CHECK_FALSE(hwmon::parse_cpu_stat_line("cpu 1 2 3 4 5 6 bad", parsed));
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

    TEST_CASE("calculates load when aggregate counter sums exceed uint64 range") {
        // The busy and idle deltas are each UINT64_MAX, so their sum does not fit
        // in 64 bits and wrapped arithmetic cannot recover the real 50% split.
        const uint64_t max = std::numeric_limits<uint64_t>::max();
        CpuTimes prev{0, 0, 0, 0, 0, 0, 0, 0};
        CpuTimes curr{max, 0, 0, max, 0, 0, 0, 0};

        CHECK(hwmon::calculate_cpu_load_percent(prev, curr) == 50);
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
