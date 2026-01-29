# EFR32MG1P BME280 Zigbee Sensor

A Zigbee 3.0 end device sensor application for Silicon Labs EFR32MG1P (Wireless Gecko Series 1) that reads temperature, humidity, and pressure data from a Bosch BME280 sensor via I2C and exposes the measurements through standard Zigbee clusters.

## Features

- **Target MCUs**:
  - Silicon Labs EFR32MG1P232F256GM48 (BRD4151A dev board)
  - EFR32MG1P132F256GM32 (IKEA TRÅDFRI board) with OTA support
- **SDK**: Gecko SDK (GSDK) 4.5 LTS
- **Sensor**: Bosch BME280 (Temperature, Humidity, Pressure) via I2C
- **Power**: 2xAAA batteries (3.0V nominal)
- **Zigbee Profile**: Zigbee 3.0 Sleepy End Device
- **Update Interval**: 1 minute (configurable, optimized for 2xAAA battery life)
- **Battery Monitoring**: Real-time voltage and percentage reporting via Power Configuration cluster
- **Power Optimization**:
  - Sensor readings suspended when network down
  - Exponential backoff for network join retries (30s → 10min)
  - LED auto-off after 3 seconds
  - Event-driven operation in low power mode
- **User Interface**:
  - Button (BTN0):
    - Short press (<2s): Trigger immediate sensor reading / Join network
    - Long press (≥2s): Leave and rejoin network
  - LED (LED0): Network status indication
- **OTA Updates**: TRÅDFRI variant includes OTA bootloader support with external SPI flash
- **Headless Build**: Fully reproducible CI/CD pipeline using GNU Arm GCC and SLC CLI

## Zigbee Clusters

The sensor exposes the following server clusters on Endpoint 1:

| Cluster ID | Cluster Name                | Attribute | Data Type | Unit | Update Rate |
|------------|----------------------------|-----------|-----------|------|-------------|
| 0x0000     | Basic                      | -         | -         | -    | -           |
| 0x0001     | Power Configuration        | 0x0020    | uint8     | 100mV | 1min       |
|            |                            | 0x0021    | uint8     | 0.5% | 1min        |
| 0x0003     | Identify                   | -         | -         | -    | -           |
| 0x0402     | Temperature Measurement    | 0x0000    | int16     | 0.01°C | 1min       |
| 0x0405     | Relative Humidity          | 0x0000    | uint16    | 0.01%RH | 1min      |
| 0x0403     | Pressure Measurement       | 0x0000    | int16     | kPa  | 1min         |

### Attribute Details

- **Temperature**: Signed 16-bit integer in hundredths of degrees Celsius (0.01°C resolution)
- **Humidity**: Unsigned 16-bit integer in hundredths of percent RH (0.01%RH resolution)
- **Pressure**: Signed 16-bit integer in kilopascals (kPa)
- **Battery Voltage (0x0020)**: Unsigned 8-bit integer in 100mV units (e.g., 30 = 3.0V)
- **Battery Percentage (0x0021)**: Unsigned 8-bit integer, 0-200 range (200 = 100%, 0.5% resolution)

### Battery Configuration

The device is configured for 2xAAA alkaline batteries:
- **Full**: 3.2V (2x 1.6V fresh alkaline) = 100% (200)
- **Nominal**: 3.0V (2x 1.5V) = ~86% (172)
- **Empty**: 1.8V (2x 0.9V depleted) = 0% (0)

Battery voltage is measured using the EFR32MG1P internal ADC and reported every minute or when voltage changes by ≥0.1V.

The device supports Zigbee attribute reporting (Configure Reporting), allowing a coordinator to receive automatic updates when values change.

## User Interface

### Button (BTN0)

**Short Press (<2 seconds):**
- **Not joined to network**: Start network joining (LED will blink)
- **Joined to network**: Trigger immediate sensor reading and report (LED will flash briefly)

**Long Press (≥2 seconds):**
- **Joined to network**: Leave current network and immediately rejoin (useful for switching coordinators)
- **Not joined**: No action

### LED (LED0)

- **Off**: Default state (not joined or joined and idle to save power)
- **Blinking (500ms)**: Network joining in progress
- **Solid on (3 seconds)**: Successfully joined to network (turns off after 3s to save power)
- **Brief flash (200ms)**: Sensor reading triggered by button press
- **Rapid flash (100ms x10)**: Sensor initialization error

The LED automatically turns off after joining to conserve battery power on this sleepy end device.

## Hardware Setup

This project supports two board variants:

### BRD4151A Development Board (Standard)

**Required Components:**
1. Silicon Labs EFR32MG1P Development Board (BRD4151A)
2. Bosch BME280 sensor module
3. 2x 4.7kΩ pull-up resistors (for I2C SDA and SCL lines)
4. Breadboard and jumper wires

**Wiring for BRD4151A:**

| BME280 Pin | EFR32MG1P Pin | Function | Notes |
|------------|---------------|----------|-------|
| VCC        | 3.3V          | Power    | 3.3V only |
| GND        | GND           | Ground   | -     |
| SDA        | PC10          | I2C Data | Add 4.7kΩ pull-up to 3.3V |
| SCL        | PC11          | I2C Clock | Add 4.7kΩ pull-up to 3.3V |
| SDO        | GND or 3.3V   | I2C Addr | GND=0x76, 3.3V=0x77 |

**Default Configuration**: The firmware is configured for BME280 I2C address 0x76 (SDO connected to GND).

To change pin assignments or I2C address, edit `include/bme280_board_config.h`.

### IKEA TRÅDFRI Board (Custom)

**For TRÅDFRI board setup, see [TRADFRI_SETUP.md](TRADFRI_SETUP.md)** for complete instructions including:
- Hardware pinout and connections
- BME280 wiring (same I2C pins: PC10/PC11)
- SWD flashing instructions
- OTA bootloader configuration
- Troubleshooting guide

The TRÅDFRI variant uses the same I2C pins (PC10/PC11) and includes OTA update support via external SPI flash.

## Prerequisites

### Local Development

To build this project locally, you need:

1. **Gecko SDK 4.5**
   ```bash
   git clone --depth 1 --branch gsdk_4.5 https://github.com/SiliconLabs/gecko_sdk.git
   ```

2. **Silicon Labs SLC CLI** (Simplicity Commander CLI)
   - Download from [Silicon Labs Developer Portal](https://www.silabs.com/developers/simplicity-studio)
   - Install and add to your PATH

3. **GNU Arm Embedded Toolchain** (version 10.3 or compatible)
   - Download from [ARM Developer](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm)
   - Or install via package manager:
     ```bash
     # Ubuntu/Debian
     sudo apt-get install gcc-arm-none-eabi

     # macOS
     brew install gcc-arm-embedded
     ```

4. **Build Tools**
   - make
   - git

## Building the Firmware

### Local Build

1. Clone this repository:
   ```bash
   git clone https://github.com/faronov/efr32mg1p-bme280-zigbee-sensor.git
   cd efr32mg1p-bme280-zigbee-sensor
   ```

2. Set environment variables:

   **For BRD4151A (standard dev board):**
   ```bash
   export GSDK_DIR=/path/to/gecko_sdk
   export BOARD=brd4151a
   # SLCP_FILE is optional, defaults to zigbee_bme280_sensor.slcp
   ```

   **For TRÅDFRI board:**
   ```bash
   export GSDK_DIR=/path/to/gecko_sdk
   export BOARD=custom
   export SLCP_FILE=zigbee_bme280_sensor_tradfri.slcp
   ```

3. Run the build script:
   ```bash
   ./tools/build.sh
   ```

4. Build outputs will be in `firmware/build/release/`:
   - `*.elf` - Executable and Linkable Format (for debugging)
   - `*.hex` - Intel HEX format (for flashing)
   - `*.s37` - Motorola S-record format
   - `*.bin` - Raw binary format
   - `*.map` - Memory map file

### CI/CD Build

The project includes a GitHub Actions workflow that automatically builds **both board variants** on every push or pull request. The workflow:

1. Builds Docker container with Silicon Labs tooling (cached for fast builds)
2. Builds **BRD4151A variant** (`zigbee_bme280_sensor.slcp`)
3. Builds **TRÅDFRI variant** (`zigbee_bme280_sensor_tradfri.slcp`) with OTA support
4. Uploads separate artifacts for each board

**Build artifacts** are available in the Actions tab for 30 days:
- `firmware-brd4151a-{sha}` - Standard development board firmware
- `firmware-tradfri-{sha}` - TRÅDFRI board firmware with OTA

Each artifact contains `.hex`, `.s37`, `.bin`, `.elf`, and `.map` files ready for flashing.

## Flashing the Firmware

Use Simplicity Commander or J-Link tools to flash the firmware:

```bash
# Using Simplicity Commander
commander flash firmware/build/debug/zigbee_bme280_sensor.s37

# Using J-Link
JLinkExe -device EFR32MG1P232F256GM48 -if SWD -speed 4000
> loadfile firmware/build/debug/zigbee_bme280_sensor.hex
> reset
> go
> exit
```

## Project Structure

```
efr32mg1p-bme280-zigbee-sensor/
├── .github/
│   └── workflows/
│       └── build-docker.yml          # CI/CD workflow (builds both variants)
├── config/
│   └── zcl/
│       └── zcl_config.zap            # Zigbee cluster configuration
├── firmware/                         # Generated project (gitignored)
├── include/
│   ├── bme280_board_config.h         # I2C config for BRD4151A
│   └── bme280_board_config_tradfri.h # I2C config for TRÅDFRI
├── src/
│   ├── app/
│   │   ├── app_sensor.c              # Sensor integration
│   │   └── app_sensor.h
│   └── drivers/
│       ├── hal_i2c.c                 # I2C HAL (multi-board support)
│       ├── hal_i2c.h
│       └── bme280/
│           ├── bme280_min.c          # BME280 driver
│           └── bme280_min.h
├── tools/
│   └── build.sh                      # Build automation script
├── zigbee_bme280_sensor.slcp         # BRD4151A project config
├── zigbee_bme280_sensor_tradfri.slcp # TRÅDFRI project config with OTA
├── Dockerfile                        # Docker build environment
├── POWER_OPTIMIZATION.md             # Battery optimization guide
├── TRADFRI_SETUP.md                  # TRÅDFRI board setup guide
├── .gitignore
├── LICENSE                           # MIT License
├── main.c                            # Application entry point
├── app.c                             # Application logic
└── README.md                         # This file
```

## Configuration

### Power Optimization

See [POWER_OPTIMIZATION.md](POWER_OPTIMIZATION.md) for detailed power saving strategies.

**Key power features**:
- **No sensor reads when network down**: Saves power when not connected
- **Exponential backoff**: Network join retries increase from 30s to 10min intervals
- **1-minute sensor interval**: Default reading interval optimized for 2xAAA battery life
- **LED auto-off**: Turns off after 3s to conserve power
- **Battery monitoring**: Real-time voltage and percentage tracking

**Estimated battery life on 2xAAA alkaline (~2400 mAh)**:
- Current configuration (1min updates): ~6-12 months
- With 5-minute updates: ~12-18 months
- Event-driven only: ~18-24 months

### Sensor Update Interval

To change the sensor reading interval, edit `src/app/app_sensor.h`:

```c
// For testing (30 seconds):
#define SENSOR_UPDATE_INTERVAL_MS   30000

// For responsive updates (1 minute - default for 2xAAA):
#define SENSOR_UPDATE_INTERVAL_MS   60000

// For balanced operation (5 minutes):
#define SENSOR_UPDATE_INTERVAL_MS   300000

// For maximum battery life (15 minutes):
#define SENSOR_UPDATE_INTERVAL_MS   900000
```

### I2C Pin Configuration

To use different I2C pins, edit `include/bme280_board_config.h`:

```c
#define BME280_I2C_SDA_PORT       gpioPortC
#define BME280_I2C_SDA_PIN        10

#define BME280_I2C_SCL_PORT       gpioPortC
#define BME280_I2C_SCL_PIN        11
```

### BME280 I2C Address

The default address is 0x76. To use 0x77, connect BME280 SDO pin to 3.3V and edit `include/bme280_board_config.h`:

```c
#define BME280_I2C_ADDR           0x77
```

## Troubleshooting

### Build Issues

**Problem**: SLC CLI not found
**Solution**: Ensure SLC CLI is installed and in your PATH. Run `slc --version` to verify.

**Problem**: ARM toolchain not found
**Solution**: Install gcc-arm-none-eabi and ensure it's in your PATH. Run `arm-none-eabi-gcc --version` to verify.

**Problem**: GSDK not found
**Solution**: Clone GSDK 4.5 and set `GSDK_DIR` environment variable correctly.

### Runtime Issues

**Problem**: BME280 initialization fails
**Solution**:
- Check I2C wiring (SDA, SCL, VCC, GND)
- Verify pull-up resistors are installed (4.7kΩ)
- Check I2C address (0x76 vs 0x77)
- Verify BME280 power supply is 3.3V (not 5V)

**Problem**: No sensor readings
**Solution**:
- Check serial console output for error messages
- Verify BME280 is properly initialized
- Ensure Zigbee network is joined

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.

**Note**: This project depends on the Silicon Labs Gecko SDK, which is licensed separately under the Zlib license. The GSDK is not included in this repository and must be obtained from:
https://github.com/SiliconLabs/gecko_sdk

## Contributing

Contributions are welcome! Please open an issue or submit a pull request.

## Resources

- [Silicon Labs EFR32MG1P Datasheet](https://www.silabs.com/wireless/zigbee/efr32mg1-series-1-socs)
- [Gecko SDK Documentation](https://docs.silabs.com/)
- [BME280 Datasheet](https://www.bosch-sensortec.com/products/environmental-sensors/humidity-sensors-bme280/)
- [Zigbee Cluster Library Specification](https://zigbeealliance.org/developer_resources/zigbee-cluster-library/)

## Author

Created by faronov - [GitHub](https://github.com/faronov)

## Support

For issues and questions, please open an issue on the [GitHub repository](https://github.com/faronov/efr32mg1p-bme280-zigbee-sensor/issues).
