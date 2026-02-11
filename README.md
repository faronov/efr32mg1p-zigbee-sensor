# EFR32MG1P BME280 Zigbee Sensor

[![Release](https://img.shields.io/github/v/release/faronov/efr32mg1p-zigbee-sensor)](https://github.com/faronov/efr32mg1p-zigbee-sensor/releases)
[![Build](https://img.shields.io/github/actions/workflow/status/faronov/efr32mg1p-zigbee-sensor/build-docker.yml?branch=main)](https://github.com/faronov/efr32mg1p-zigbee-sensor/actions)
[![License](https://img.shields.io/github/license/faronov/efr32mg1p-zigbee-sensor)](LICENSE)

A Zigbee 3.0 sleepy end device sensor for Silicon Labs EFR32MG1P (Wireless Gecko Series 1) that reads temperature, humidity, and pressure data from a Bosch BME280 sensor and reports via standard Zigbee clusters. Features OTA firmware updates, optimized power consumption, and automated CI/CD builds.

## Quick Start

```bash
# Clone repository
git clone https://github.com/faronov/efr32mg1p-zigbee-sensor.git
cd efr32mg1p-zigbee-sensor

# Build firmware (local)
bash tools/build.sh

# Or download pre-built release
# See: https://github.com/faronov/efr32mg1p-zigbee-sensor/releases/latest
```

## Features

### Hardware Support
- **IKEA TRÅDFRI module** (EFR32MG1P132F256GM32) with OTA support

### Sensor & Power
- **BME280 sensor** via I2C (temperature, humidity, pressure)
- **2xAAA batteries** (3.0V nominal, ~2.0V cutoff)
- **Battery monitoring** with real-time voltage and percentage reporting
- **Configurable sensor read interval** (default 10s, range 10..3600s)
- **Automatic sleep** between measurements

### Zigbee 3.0 Compliance
- **Sleepy end device** with optimized power consumption
- **Standard clusters**: Temperature (0x0402), Humidity (0x0405), Pressure (0x0403), Power Configuration (0x0001)
- **Coordinator-side binding** ready for Zigbee2MQTT, ZHA, deCONZ
- **Optimized network rejoin**: 138ms (7x faster than default 2.2s)
- **Automatic network recovery** with exponential backoff (30s → 10min)

### OTA Firmware Updates
- **On-module SPI flash** storage (IS25LQ020B 256KB already present on TRÅDFRI module)
- **Custom bootloader** (35KB) with application upgrade support
- **Standard Zigbee OTA format** (.gbl, .ota, .zigbee files)
- **Compressed images** with LZMA support
- **Compatible** with Zigbee2MQTT, Home Assistant ZHA, deCONZ

### User Interface
- **Button (BTN0)**:
  - Short press (<2s): Trigger immediate sensor reading and report
  - Long press (≥2s): Leave and rejoin network
  - Network join trigger when not connected
- **LED (LED0)**:
  - Network status indication
  - Auto-off after 30 seconds to save power

### Build System
- **GSDK 4.5.0 LTS** (Long Term Support)
- **Headless CI/CD** with GitHub Actions
- **Docker containerization** for reproducible builds
- **Automated OTA file generation** on tagged releases
- **Release optimization** for minimal binary size

## Zigbee Clusters

Sensor exposes the following server clusters on **Endpoint 1**:

| Cluster ID | Cluster Name                | Attribute | Data Type | Unit |
|------------|----------------------------|-----------|-----------|------|
| 0x0000     | Basic                      | -         | -         | -    |
| 0x0001     | Power Configuration        | 0x0020    | uint8     | 100mV |
|            |                            | 0x0021    | uint8     | 0.5% |
| 0x0003     | Identify                   | -         | -         | -    |
| 0x0402     | Temperature Measurement    | 0x0000    | int16     | 0.01°C |
| 0x0405     | Relative Humidity          | 0x0000    | uint16    | 0.01%RH |
| 0x0403     | Pressure Measurement       | 0x0000    | int16     | kPa  |

### Attribute Details

- **Temperature**: Signed 16-bit, hundredths of °C (e.g., 2345 = 23.45°C)
- **Humidity**: Unsigned 16-bit, hundredths of %RH (e.g., 4520 = 45.20%RH)
- **Pressure**: Signed 16-bit, kilopascals (e.g., 101 = 101 kPa = 1010 mbar)
- **Battery Voltage (0x0020)**: Unsigned 8-bit, 100mV units (e.g., 30 = 3.0V)
- **Battery Percentage (0x0021)**: Unsigned 8-bit, 0-200 range (200 = 100%)

### Reportable Attributes

Reportable attributes are configured in ZAP and can be overridden by coordinator-side
`Configure Reporting` (ZHA, Zigbee2MQTT, deCONZ).

## Hardware Setup

### IKEA TRÅDFRI Module

```
BME280 → TRÅDFRI Module (QFN32)
  VCC  → AVDD (Pin 5) - 3.3V
  GND  → GND (Pin 6, 14, 17, 32)
  SDA  → PC10 (Pin 24)
  SCL  → PC11 (Pin 25)

External Flash: IS25LQ020B (256KB SPI)
  CS   → PB11 (Pin 16)
  CLK  → PD13 (Pin 27)
  MISO → PD14 (Pin 28)
  MOSI → PD15 (Pin 29)

LED0: PA0 (Pin 2)
BTN0: PB13 (Pin 19)
```

See [PINOUT.md](PINOUT.md) for detailed hardware connections.

## Building Firmware

### Prerequisites

- **Simplicity Studio v5** with GSDK 4.5.0 LTS
- **SLC CLI** (Simplicity Commander Line Interface)
- **GNU Arm Embedded Toolchain** (version 12.2.rel1 or compatible)
- **Git** for version control

### Local Build

```bash
# Build TRÅDFRI firmware
bash tools/build.sh

# Outputs: firmware/build/release/*.s37
```

### Build Bootloader

```bash
# Build custom bootloader for TRÅDFRI
bash tools/build_bootloader.sh

# Output: bootloader/tradfri-spiflash/build/**/*.s37
```

### Create OTA Files

```bash
# Create OTA update files (.gbl, .ota, .zigbee)
bash tools/create_ota_file.sh 1.0.0

# Outputs: build/tradfri/ota/*.{gbl,ota,zigbee}
```

### CI/CD Builds

Automated builds run on every push to `main` and on version tags:

```bash
# Create release
git tag v1.0.1 -m "Release v1.0.1"
git push origin v1.0.1

# Download artifacts from GitHub Actions
# See: https://github.com/faronov/efr32mg1p-zigbee-sensor/actions
```

## Flashing Firmware

### Flash with Simplicity Commander

```bash
# Flash bootloader (TRÅDFRI only)
commander flash bootloader/tradfri-spiflash/build/**/*.s37 \
  --device EFR32MG1P132F256GM32

# Flash application
commander flash firmware/build/release/zigbee_bme280_sensor_tradfri.s37 \
  --device EFR32MG1P132F256GM32
```

### Flash with J-Link

```bash
# Using JLinkExe
JLinkExe -device EFR32MG1P132F256GM32 -if SWD -speed 4000
  > erase
  > loadfile firmware.s37
  > r
  > q
```

## OTA Firmware Updates

### Zigbee2MQTT

```bash
# 1. Copy OTA file to Zigbee2MQTT data directory
cp build/tradfri/ota/*.zigbee ~/zigbee2mqtt/data/

# 2. Web UI → Devices → Select sensor → OTA Updates → Check for updates
```

### Home Assistant (ZHA)

```bash
# 1. Copy OTA file to Home Assistant
cp build/tradfri/ota/*.ota /config/ota/

# 2. ZHA → Device → Reconfigure → Check for updates
```

### deCONZ

```bash
# 1. Copy OTA file to deCONZ otau directory
cp build/tradfri/ota/*.ota ~/.local/share/dresden-elektronik/deCONZ/otau/

# 2. Trigger update from deCONZ UI
```

See [OTA_FILE_CREATION.md](OTA_FILE_CREATION.md) for detailed OTA setup.

## Network Integration

### Pairing Mode

1. **Power on device** (or press button if already powered)
2. **LED blinks** indicating network search
3. **Put coordinator in pairing mode**
4. **Device joins** automatically (LED stays on)
5. **LED turns off** after 30 seconds (power saving)

### Force Rejoin

- **Long press button** (≥2s) to leave and rejoin network
- Useful after coordinator replacement or network issues

### Binding Clusters

The sensor supports **coordinator-side binding** for direct reporting:

**Zigbee2MQTT:**
```bash
mosquitto_pub -t 'zigbee2mqtt/bridge/request/device/bind' \
  -m '{"from":"sensor_ieee","to":"coordinator","clusters":["msTemperatureMeasurement","msRelativeHumidity","msPressureMeasurement"]}'
```

**ZHA/deCONZ:** Use GUI binding options

See [BINDING_GUIDE.md](BINDING_GUIDE.md) for details.

## Power Consumption

### Battery Life Notes

Battery life depends on:
- Sensor read interval (`sensor_read_interval`)
- Reporting configuration (min/max/change)
- Link quality / retries / rejoin frequency
- Debug mode (`APP_DEBUG_NO_SLEEP`)

### Power Optimization Features

- **Sleepy end device** mode (deep sleep between measurements)
- **Sensor power-down** when network unavailable
- **LED auto-off** after 30 seconds
- **Exponential backoff** for join retries (reduces radio activity)
- **Optimized rejoin** - single channel attempt first (138ms vs 2.2s)
- **Event-driven** operation (no polling loops)

See [POWER_OPTIMIZATION.md](POWER_OPTIMIZATION.md) for analysis.

## Project Structure

```
.
├── app.c                      # Main application logic
├── main.c                     # Entry point
├── src/
│   ├── app/
│   │   ├── app_sensor.c       # Sensor reading and reporting
│   │   └── app_sensor.h
│   └── drivers/
│       ├── bme280/            # BME280 driver (minimal)
│       ├── hal_i2c.c          # I2C hardware abstraction
│       └── battery.c          # Battery monitoring (ADC)
├── config/
│   └── zcl/zcl_config.zap     # Zigbee cluster configuration
├── bootloader/
│   └── tradfri-spiflash/      # Custom OTA bootloader
├── tools/
│   ├── build.sh               # Firmware build script
│   ├── build_bootloader.sh    # Bootloader build script
│   └── create_ota_file.sh     # OTA file generator
├── .github/workflows/         # CI/CD automation
└── README.md                  # This file
```

## Documentation

- **Canonical project state**:
  - **[PROJECT_STATE.md](PROJECT_STATE.md)** - active profiles, files, current behavior
  - **[RUNBOOK.md](RUNBOOK.md)** - CI/artifacts/flash/SWO operational flow
  - **[DECISIONS.md](DECISIONS.md)** - accepted technical decisions
- **[PINOUT.md](PINOUT.md)** - Hardware connections and pin mappings
- **[TRADFRI_SETUP.md](TRADFRI_SETUP.md)** - TRÅDFRI module specific setup
- **[OTA_FILE_CREATION.md](OTA_FILE_CREATION.md)** - Creating OTA update files
- **[OTA_SETUP_GUIDE.md](OTA_SETUP_GUIDE.md)** - OTA bootloader configuration
- **[BINDING_GUIDE.md](BINDING_GUIDE.md)** - Zigbee cluster binding setup
- **[POWER_OPTIMIZATION.md](POWER_OPTIMIZATION.md)** - Power consumption analysis
- **[BATTERY_AND_BUTTON_FEATURES.md](BATTERY_AND_BUTTON_FEATURES.md)** - UI features
- **[docs/zha_quirk_v2.py](docs/zha_quirk_v2.py)** - ZHA Quirk v2 (single config: `sensor_read_interval`)
- **[docs/zigbee2mqtt-converter.js](docs/zigbee2mqtt-converter.js)** - Zigbee2MQTT converter (single config: `sensor_read_interval`)

Historical notes are kept under `docs/archive/` and may describe superseded implementations.

## Troubleshooting

### Device Won't Join Network

1. **Check LED**: Should blink during join
2. **Reset device**: Long press button (≥2s)
3. **Check coordinator**: Ensure pairing mode is active
4. **Check distance**: Move closer to coordinator
5. **Check logs**: Use Zigbee sniffer or coordinator logs

### Sensor Values Not Updating

1. **Check I2C connections**: Verify SDA/SCL wiring
2. **Check power**: Ensure BME280 has 3.3V
3. **Check binding**: Verify cluster bindings in coordinator
4. **Check battery**: Low battery (<2.1V) may cause issues

### OTA Update Fails

1. **Check flash chip**: Verify SPI connections (TRÅDFRI only)
2. **Check file format**: Must be proper Zigbee OTA format (.ota or .zigbee)
3. **Check version**: OTA only upgrades to higher version numbers
4. **Check bootloader**: Ensure custom bootloader is flashed first

### Build Errors

1. **Check GSDK version**: Must be 4.5.0 LTS
2. **Check toolchain**: GCC Arm version 12.2.rel1 recommended
3. **Clean build**: Delete `firmware/build/` and rebuild
4. **Check SLC**: Ensure SLC CLI is in PATH

## Development

### Modify Sensor Update Interval

Runtime-configure via manufacturer-specific Basic attribute `0xF000`
(`sensor_read_interval`, seconds, range `10..3600`, default `10`).

### Add Custom Clusters

1. Edit `config/zcl/zcl_config.zap` using Simplicity Studio ZAP tool
2. Regenerate with `slc generate`
3. Implement handlers in `app.c`

### Debug Output

Enable UART debug output via SWO:
```bash
commander studio trace --device EFR32MG1P132F256GM32
```

## Contributing

Contributions welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Commit changes with clear messages
4. Submit a pull request

## License

This project is licensed under the **Apache License 2.0** - see [LICENSE](LICENSE) file.

## Acknowledgments

- **Silicon Labs** for GSDK 4.5.0 LTS and development tools
- **Bosch Sensortec** for BME280 sensor documentation
- **IKEA** for TRÅDFRI module hardware (used as platform, not affiliated)
- **Zigbee Alliance** for Zigbee 3.0 specification

## Release Notes

### v1.0.0 (2026-01-29)

**Initial Release**

Features:
- BME280 sensor support (temperature, humidity, pressure)
- Zigbee 3.0 sleepy end device implementation
- 2xAAA battery support with monitoring
- OTA firmware updates via SPI flash
- Custom bootloader (35KB) for TRÅDFRI
- Optimized network rejoin (7x faster)
- Coordinator-side cluster binding
- Runtime-configurable sensor interval (default 10s)
- Automated CI/CD builds

Hardware:
- IKEA TRÅDFRI module (EFR32MG1P132F256GM32)

Technical:
- GSDK 4.5.0 LTS
- Release builds with size optimization
- Firmware size: depends on selected profile/build variant

## Contact

- **Issues**: https://github.com/faronov/efr32mg1p-zigbee-sensor/issues
- **Discussions**: https://github.com/faronov/efr32mg1p-zigbee-sensor/discussions
