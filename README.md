# hardware-monitor

Lightweight C++ daemon that streams real-time hardware statistics over a serial port every 2 seconds. Designed for driving external displays, Arduino/ESP32 projects, custom PC status panels, etc.

## Features

- Overall + per-core CPU load (%)
- CPU temperature (°C)
- GPU clock (MHz), utilization (%), temperature (°C) — NVIDIA primary (via NVML + nvidia-smi fallback)
- Framerate (FPS) — updated live from a text file (easy integration with games/scripts)
- Compact binary protocol (easy to parse on microcontrollers)
- Pure C++17, minimal dependencies (uses POSIX + dlopen for optional NVIDIA)
- Proper signal handling + systemd integration

## Packet Protocol (v1)

**Sent every 2 seconds (configurable).**

### Wire Format

```
[0xAA][0x55][version][num_cores][overall_cpu][core0][core1]...[coreN-1][cpu_temp][gpu_clock_lo][gpu_clock_hi][gpu_util][gpu_temp][fps][checksum]
```

| Field              | Size   | Type          | Description                              |
|--------------------|--------|---------------|------------------------------------------|
| Sync               | 2      | bytes         | 0xAA 0x55                                |
| Version            | 1      | uint8         | 1                                        |
| num_cores          | 1      | uint8         | Number of logical CPU cores              |
| overall_cpu        | 1      | uint8         | 0–100 %                                  |
| per_core_cpu       | N      | uint8[N]      | One byte per core (0–100)                |
| cpu_temp           | 1      | int8          | °C (or 0/-127 if unavailable)            |
| gpu_clock          | 2      | uint16 LE     | MHz                                      |
| gpu_util           | 1      | uint8         | 0–100 %                                  |
| gpu_temp           | 1      | int8          | °C                                       |
| fps                | 1      | uint8         | 0–255                                    |
| checksum           | 1      | uint8         | XOR of all bytes before it               |

**Total size** = `12 + num_cores` bytes.

**Checksum calculation**: XOR of every byte from the first sync byte up to (but **not** including) the checksum byte.

### Example Parser (Arduino / C)

```cpp
// Pseudocode
if (readByte() == 0xAA && readByte() == 0x55) {
    uint8_t ver = readByte();
    uint8_t cores = readByte();
    uint8_t cpu_total = readByte();
    for (int i = 0; i < cores; i++) cpu_cores[i] = readByte();
    int8_t cpu_temp = (int8_t)readByte();
    uint16_t gpu_clock = readByte() | (readByte() << 8);
    // ... etc
    uint8_t rx_checksum = readByte();
    // verify XOR
}
```

## Building

```bash
cd Documents/GitHub/hardware-monitor

mkdir build && cd build
cmake ..
make -j$(nproc)

# Optional: install
sudo make install
```

Binary: `build/hwmon-daemon`

## Running Manually

```bash
# Default: /dev/ttyUSB0 @ 115200
./hwmon-daemon

# Custom device + faster updates
./hwmon-daemon --serial /dev/ttyACM0 --baud 230400 --interval 1000
```

## Updating Framerate (FPS)

The daemon continuously watches a file for the current FPS value.

```bash
# Create the file (or let the daemon create the directory)
sudo mkdir -p /run/hwmon-daemon
echo 144 > /run/hwmon-daemon/current_fps
```

You can update it from:
- Game launch wrapper scripts
- MangoHud + a small post-processing script
- Any monitoring tool you already use

The value is read every ~500 ms.

## systemd Service (Recommended)

1. Copy or install the unit:

```bash
sudo cp systemd/hardware-monitor.service /etc/systemd/system/
sudo systemctl daemon-reload
```

2. Edit the unit (change serial port if needed):

```bash
sudo systemctl edit hardware-monitor
```

3. Enable + start:

```bash
sudo systemctl enable --now hardware-monitor
journalctl -u hardware-monitor -f
```

**Note**: The service runs as root by default so it can access `/dev/tty*` and all sensors. You can create a dedicated `hwmon` user and add it to the `dialout` group.

## Configuration

All options are available via command-line flags (see `--help`).

For persistent configuration, modify the systemd unit or wrap it in a small shell script.

## Supported Hardware

| Component     | Method                              | Notes                                      |
|---------------|-------------------------------------|--------------------------------------------|
| CPU Load      | `/proc/stat` delta calculation      | Very accurate, works everywhere            |
| CPU Temp      | sysfs hwmon + thermal zones         | Auto-detects coretemp, k10temp, zenpower, etc. |
| NVIDIA GPU    | NVML (preferred) or `nvidia-smi`    | Zero build dependency (dlopen)             |
| AMD / Intel   | Not yet implemented                 | Contributions welcome (sysfs or rocm-smi)  |
| Framerate     | File watcher                        | You control the source                     |

## Design Notes / Future Work

- Add AMD GPU support (via sysfs or `rocm-smi`)
- Add Intel GPU support
- Optional config file (TOML or simple key=value)
- Prometheus / statsd exporter mode
- Multiple GPU support (send data for GPU 0 and GPU 1)
- CRC16 instead of simple XOR (if needed)
- Automatic serial device discovery

## License

MIT (or your preferred license).

## Contributing

Pull requests welcome — especially for additional GPU vendors and better temperature heuristics.

---

**Status**: Functional v0.1 — ready for integration with custom hardware displays. (Test push from GitHub Desktop)
