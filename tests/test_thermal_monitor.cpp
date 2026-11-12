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
}
