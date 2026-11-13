#include <doctest/doctest.h>

#include "config.hpp"

#include <string>
#include <vector>

namespace {

hwmon::ConfigParseResult parse(
    std::vector<std::string> arguments,
    hwmon::Config& config,
    std::string& error)
{
    std::vector<char*> argv;
    argv.reserve(arguments.size());
    for (std::string& argument : arguments) {
        argv.push_back(const_cast<char*>(argument.c_str()));
    }
    return hwmon::parseArgs(static_cast<int>(argv.size()), argv.data(), config, error);
}

} // namespace

TEST_SUITE("Command line configuration") {

    TEST_CASE("parses supported options and defaults") {
        hwmon::Config config;
        std::string error;
        const auto result = parse(
            {"hwmon-daemon", "--serial", "/dev/ttyACM0", "--baud", "230400",
             "--interval", "1000", "--fps-file", "/tmp/fps"},
            config, error);

        CHECK(result == hwmon::ConfigParseResult::success);
        CHECK(config.serial_device == "/dev/ttyACM0");
        CHECK(config.baud == 230400);
        CHECK(config.interval_ms == 1000);
        CHECK(config.fps_file == "/tmp/fps");
        CHECK(error.empty());

        CHECK(parse({"hwmon-daemon"}, config, error) == hwmon::ConfigParseResult::success);
        CHECK(config.serial_device == "/dev/ttyUSB0");
        CHECK(config.baud == 115200);
        CHECK(config.interval_ms == 2000);
    }

    TEST_CASE("parses supported short option forms") {
        hwmon::Config config;
        std::string error;
        const auto result = parse(
            {"hwmon-daemon", "-s", "/dev/ttyACM1", "-b", "460800",
             "-i", "750", "-f", "/tmp/hwmon-fps"},
            config, error);

        CHECK(result == hwmon::ConfigParseResult::success);
        CHECK(config.serial_device == "/dev/ttyACM1");
        CHECK(config.baud == 460800);
        CHECK(config.interval_ms == 750);
        CHECK(config.fps_file == "/tmp/hwmon-fps");
        CHECK(error.empty());
    }

    TEST_CASE("rejects malformed numeric values and nonpositive intervals") {
        hwmon::Config config;
        std::string error;

        CHECK(parse({"hwmon-daemon", "--baud", "115200x"}, config, error) == hwmon::ConfigParseResult::error);
        CHECK(error == "Invalid baud rate: 115200x");
        CHECK(parse({"hwmon-daemon", "--interval", "nope"}, config, error) == hwmon::ConfigParseResult::error);
        CHECK(error == "Invalid interval: nope");
        CHECK(parse({"hwmon-daemon", "--interval", "0"}, config, error) == hwmon::ConfigParseResult::error);
        CHECK(error == "Interval must be greater than zero");
        CHECK(parse({"hwmon-daemon", "--interval", "-1"}, config, error) == hwmon::ConfigParseResult::error);
        CHECK(error == "Interval must be greater than zero");
    }

    TEST_CASE("rejects unsupported baud rates and unexpected arguments") {
        hwmon::Config config;
        std::string error;

        CHECK(parse({"hwmon-daemon", "--baud", "12345"}, config, error) == hwmon::ConfigParseResult::error);
        CHECK(error == "Unsupported baud rate: 12345");
        CHECK(parse({"hwmon-daemon", "extra"}, config, error) == hwmon::ConfigParseResult::error);
        CHECK(error == "Unexpected positional argument: extra");
        CHECK(parse({"hwmon-daemon", "--unknown"}, config, error) == hwmon::ConfigParseResult::error);
        CHECK(error == "Unknown option or missing option value");
        CHECK(parse({"hwmon-daemon", "--serial"}, config, error) == hwmon::ConfigParseResult::error);
    }

    TEST_CASE("reports help as a distinct parse result") {
        hwmon::Config config;
        std::string error;
        CHECK(parse({"hwmon-daemon", "--help"}, config, error) == hwmon::ConfigParseResult::help);
    }

    TEST_CASE("a parse that stops inside grouped short options does not leak into the next parse") {
        hwmon::Config config;
        std::string error;
        REQUIRE(parse({"hwmon-daemon", "-hx"}, config, error) == hwmon::ConfigParseResult::help);

        CHECK(parse({"hwmon-daemon", "-s", "/dev/ttyACM0"}, config, error) ==
              hwmon::ConfigParseResult::success);
        CHECK(error.empty());
        CHECK(config.serial_device == "/dev/ttyACM0");
    }
}
