#include "serial_port.hpp"

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <system_error>

namespace hwmon {

SerialPort::~SerialPort() {
    close();
}

bool SerialPort::open(const std::string& device, int baud_rate) {
    close();

    fd_ = ::open(device.c_str(), O_WRONLY | O_NOCTTY | O_SYNC);
    if (fd_ < 0) {
        last_error_ = "Failed to open " + device + ": " + std::strerror(errno);
        return false;
    }

    device_ = device;

    if (!configure(baud_rate)) {
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    return true;
}

void SerialPort::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    device_.clear();
}

bool SerialPort::configure(int baud_rate) {
    termios tty{};
    if (tcgetattr(fd_, &tty) != 0) {
        last_error_ = "tcgetattr failed: " + std::string(std::strerror(errno));
        return false;
    }

    // 8N1, no flow control, raw mode
    cfmakeraw(&tty);

    tty.c_cflag &= ~PARENB;        // No parity
    tty.c_cflag &= ~CSTOPB;        // 1 stop bit
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;            // 8 bits
    tty.c_cflag &= ~CRTSCTS;       // No hardware flow control
    tty.c_cflag |= CREAD | CLOCAL; // Enable receiver, ignore modem lines

    // Baud rate
    speed_t speed = baudToSpeed(baud_rate);
    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    // Timeouts (we don't read, but be clean)
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        last_error_ = "tcsetattr failed: " + std::string(std::strerror(errno));
        return false;
    }

    // Flush any garbage
    tcflush(fd_, TCIOFLUSH);
    return true;
}

int SerialPort::baudToSpeed(int baud_rate) const {
    switch (baud_rate) {
        case 9600:   return B9600;
        case 19200:  return B19200;
        case 38400:  return B38400;
        case 57600:  return B57600;
        case 115200: return B115200;
        case 230400: return B230400;
        case 460800: return B460800;
        case 921600: return B921600;
        default:     return B115200;
    }
}

ssize_t SerialPort::write(const std::vector<uint8_t>& data) {
    return write(data.data(), data.size());
}

ssize_t SerialPort::write(const uint8_t* data, size_t len) {
    if (fd_ < 0) {
        last_error_ = "Serial port not open";
        return -1;
    }

    ssize_t total = 0;
    while (total < static_cast<ssize_t>(len)) {
        ssize_t n = ::write(fd_, data + total, len - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            last_error_ = "write failed: " + std::string(std::strerror(errno));
            return -1;
        }
        total += n;
    }
    return total;
}

} // namespace hwmon
