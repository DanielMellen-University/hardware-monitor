#include <doctest/doctest.h>

#include "thermal_monitor.hpp"

#include <filesystem>
#include <cstdlib>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {

class TempDirectory {
public:
    TempDirectory() {
        char pattern[] = "/tmp/hwmon-thermal-test-XXXXXX";
        char* created = mkdtemp(pattern);
        if (created) path_ = created;
    }

    ~TempDirectory() {
        if (!path_.empty()) fs::remove_all(path_);
    }

    const fs::path& path() const { return path_; }

private:
    fs::path path_;
};

void writeFile(const fs::path& path, const std::string& content) {
    fs::create_directories(path.parent_path());
    std::ofstream file(path);
    file << content;
}

} // namespace

TEST_SUITE("Thermal monitor discovery") {

    TEST_CASE("finds a CPU temperature from a named hwmon device") {
        TempDirectory temp;
        REQUIRE_FALSE(temp.path().empty());
        const fs::path hwmon = temp.path() / "hwmon";
        const fs::path thermal = temp.path() / "thermal";
        writeFile(hwmon / "hwmon0" / "name", "coretemp\n");
        writeFile(hwmon / "hwmon0" / "temp1_input", "42000\n");

        hwmon::ThermalMonitor monitor(hwmon.string(), thermal.string());
        const auto result = monitor.readCpuTempC();
        REQUIRE(result.has_value());
        CHECK(*result == 42);
    }

    TEST_CASE("uses a CPU thermal zone when no hwmon CPU sensor exists") {
        TempDirectory temp;
        REQUIRE_FALSE(temp.path().empty());
        const fs::path hwmon = temp.path() / "hwmon";
        const fs::path thermal = temp.path() / "thermal";
        writeFile(thermal / "thermal_zone0" / "type", "x86_pkg_temp\n");
        writeFile(thermal / "thermal_zone0" / "temp", "51000\n");

        hwmon::ThermalMonitor monitor(hwmon.string(), thermal.string());
        const auto result = monitor.readCpuTempC();
        REQUIRE(result.has_value());
        CHECK(*result == 51);
    }

    TEST_CASE("uses a CPU label when a generic hwmon name is present") {
        TempDirectory temp;
        REQUIRE_FALSE(temp.path().empty());
        const fs::path hwmon = temp.path() / "hwmon";
        const fs::path thermal = temp.path() / "thermal";
        writeFile(hwmon / "hwmon0" / "name", "acme_sensor\n");
        writeFile(hwmon / "hwmon0" / "temp1_label", "Package id 0\n");
        writeFile(hwmon / "hwmon0" / "temp1_input", "47000\n");

        hwmon::ThermalMonitor monitor(hwmon.string(), thermal.string());
        const auto result = monitor.readCpuTempC();
        REQUIRE(result.has_value());
        CHECK(*result == 47);
    }

    TEST_CASE("does not select unrelated hwmon devices or thermal zones") {
        TempDirectory temp;
        REQUIRE_FALSE(temp.path().empty());
        const fs::path hwmon = temp.path() / "hwmon";
        const fs::path thermal = temp.path() / "thermal";
        writeFile(hwmon / "hwmon0" / "name", "nvme\n");
        writeFile(hwmon / "hwmon0" / "temp1_input", "61000\n");
        writeFile(thermal / "thermal_zone0" / "type", "battery\n");
        writeFile(thermal / "thermal_zone0" / "temp", "39000\n");

        hwmon::ThermalMonitor monitor(hwmon.string(), thermal.string());
        CHECK_FALSE(monitor.readCpuTempC().has_value());
    }

    TEST_CASE("malformed, trailing, and disappeared readings are unavailable") {
        TempDirectory temp;
        REQUIRE_FALSE(temp.path().empty());
        const fs::path hwmon = temp.path() / "hwmon";
        const fs::path thermal = temp.path() / "thermal";
        const fs::path input = hwmon / "hwmon0" / "temp1_input";
        writeFile(hwmon / "hwmon0" / "name", "coretemp\n");
        writeFile(input, "not-a-temperature\n");

        hwmon::ThermalMonitor monitor(hwmon.string(), thermal.string());
        CHECK_FALSE(monitor.readCpuTempC().has_value());
        writeFile(input, "42000 junk\n");
        CHECK_FALSE(monitor.readCpuTempC().has_value());
        fs::remove(input);
        CHECK_FALSE(monitor.readCpuTempC().has_value());
    }

    TEST_CASE("sensor scans tolerate an invalid or disappearing root") {
        TempDirectory temp;
        REQUIRE_FALSE(temp.path().empty());
        const fs::path invalid_root = temp.path() / "hwmon-file";
        writeFile(invalid_root, "not a directory");
        const fs::path thermal = temp.path() / "thermal";

        hwmon::ThermalMonitor monitor(invalid_root.string(), thermal.string());
        CHECK_FALSE(monitor.readCpuTempC().has_value());
    }

    TEST_CASE("clamps readings to signed protocol temperature limits") {
        TempDirectory temp;
        REQUIRE_FALSE(temp.path().empty());
        const fs::path hwmon = temp.path() / "hwmon";
        const fs::path thermal = temp.path() / "thermal";
        const fs::path input = hwmon / "hwmon0" / "temp1_input";
        writeFile(hwmon / "hwmon0" / "name", "coretemp\n");

        hwmon::ThermalMonitor monitor(hwmon.string(), thermal.string());
        writeFile(input, "250000\n");
        REQUIRE(monitor.readCpuTempC().has_value());
        CHECK(*monitor.readCpuTempC() == 127);
        writeFile(input, "-200000\n");
        REQUIRE(monitor.readCpuTempC().has_value());
        CHECK(*monitor.readCpuTempC() == -127);
    }

    TEST_CASE("finds channels after a gap in hwmon numbering") {
        TempDirectory temp;
        REQUIRE_FALSE(temp.path().empty());
        const fs::path hwmon = temp.path() / "hwmon";
        const fs::path thermal = temp.path() / "thermal";
        writeFile(hwmon / "hwmon0" / "name", "k10temp\n");
        writeFile(hwmon / "hwmon0" / "temp2_input", "61000\n");

        hwmon::ThermalMonitor monitor(hwmon.string(), thermal.string());
        const auto result = monitor.readCpuTempC();
        REQUIRE(result.has_value());
        CHECK(*result == 61);
    }

    TEST_CASE("preserves sub-zero sensor readings") {
        TempDirectory temp;
        REQUIRE_FALSE(temp.path().empty());
        const fs::path hwmon = temp.path() / "hwmon";
        const fs::path thermal = temp.path() / "thermal";
        writeFile(hwmon / "hwmon0" / "name", "coretemp\n");
        writeFile(hwmon / "hwmon0" / "temp1_input", "-5000\n");

        hwmon::ThermalMonitor monitor(hwmon.string(), thermal.string());
        const auto result = monitor.readCpuTempC();
        REQUIRE(result.has_value());
        CHECK(*result == -5);
    }
}
