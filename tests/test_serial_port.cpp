#include <doctest/doctest.h>

#include "serial_port.hpp"

#include <fcntl.h>
#include <poll.h>
#include <pty.h>
#include <termios.h>
#include <unistd.h>

#include <array>
#include <string>
#include <vector>

namespace {

class FileDescriptor {
public:
    explicit FileDescriptor(int fd = -1) : fd_(fd) {}
    ~FileDescriptor() { if (fd_ >= 0) ::close(fd_); }
    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;
    int get() const { return fd_; }
    void reset() {
        if (fd_ >= 0) ::close(fd_);
        fd_ = -1;
    }

private:
    int fd_;
};

} // namespace

TEST_CASE("unsupported baud rates are reported without opening a device") {
    hwmon::SerialPort serial;

    CHECK_FALSE(serial.open("/tmp/device-must-not-be-opened", 12345));
    CHECK_FALSE(serial.isOpen());
    CHECK(serial.lastError() == "Unsupported baud rate: 12345");
}

TEST_CASE("configures and writes bytes through a pseudo-terminal") {
    int master = -1;
    int slave = -1;
    char slave_name[128] = {};
    REQUIRE(openpty(&master, &slave, slave_name, nullptr, nullptr) == 0);
    FileDescriptor master_fd(master);
    FileDescriptor original_slave_fd(slave);
    original_slave_fd.reset();

    hwmon::SerialPort serial;
    REQUIRE(serial.open(slave_name, 115200));
    const std::vector<uint8_t> bytes{0xAA, 0x55, 0x00, 0xFF};
    CHECK(serial.write(bytes) == static_cast<ssize_t>(bytes.size()));

    pollfd event{master_fd.get(), POLLIN, 0};
    REQUIRE(poll(&event, 1, 1000) == 1);
    std::array<uint8_t, 4> received{};
    REQUIRE(::read(master_fd.get(), received.data(), received.size()) ==
            static_cast<ssize_t>(received.size()));
    const std::array<uint8_t, 4> expected{0xAA, 0x55, 0x00, 0xFF};
    CHECK(received == expected);

    FileDescriptor inspection_fd(::open(slave_name, O_RDWR | O_NOCTTY));
    REQUIRE(inspection_fd.get() >= 0);
    termios settings{};
    REQUIRE(tcgetattr(inspection_fd.get(), &settings) == 0);
    CHECK((settings.c_cflag & CSIZE) == CS8);
    CHECK((settings.c_cflag & PARENB) == 0);
    CHECK((settings.c_cflag & CSTOPB) == 0);
    CHECK(cfgetospeed(&settings) == B115200);
}
