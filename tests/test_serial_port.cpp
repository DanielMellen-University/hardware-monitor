#include <doctest/doctest.h>

#include "serial_port.hpp"

TEST_CASE("unsupported baud rates are reported without opening a device") {
    hwmon::SerialPort serial;

    CHECK_FALSE(serial.open("/tmp/device-must-not-be-opened", 12345));
    CHECK_FALSE(serial.isOpen());
    CHECK(serial.lastError() == "Unsupported baud rate: 12345");
}
