# IKEA TRÅDFRI Board Setup Guide

This guide covers how to build and flash the BME280 sensor firmware on the IKEA TRÅDFRI board (EFR32MG1P132F256GM32).

## Hardware Overview

**IKEA TRÅDFRI ICC-1 Board:**
- **MCU**: EFR32MG1P132F256GM32 (32-pin package)
- **Flash**: 256KB internal + 256KB external SPI (IS25LQ020B)
- **RAM**: 32KB
- **Zigbee**: 2.4GHz radio built-in

### Pin Configuration

| Function | Pin | Notes |
|----------|-----|-------|
| **I2C SDA** | PC10 | For BME280 sensor |
| **I2C SCL** | PC11 | For BME280 sensor |
| **Button** | PB13 | Join network / trigger reading |
| **LED** | PA0 | Status indication |
| **SPI Flash CS** | PB11 | For OTA updates |
| **SPI Flash CLK** | PD13 | For OTA updates |
| **SPI Flash MISO** | PD14 | For OTA updates |
| **SPI Flash MOSI** | PD15 | For OTA updates |
| **SWCLK** | PF0 | SWD Debug |
| **SWDIO** | PF1 | SWD Debug |
| **SWO** | PF2 | SWD Debug (optional) |
| **RESET** | RESET | Hardware reset |

### BME280 Sensor Wiring

Connect BME280 to TRÅDFRI board:

```
BME280 VCC  → 3.3V (VCC pin)
BME280 GND  → GND
BME280 SDA  → PC10 (with 4.7kΩ pull-up to 3.3V)
BME280 SCL  → PC11 (with 4.7kΩ pull-up to 3.3V)
BME280 SDO  → GND (for I2C address 0x76)
```

**Important**: The 4.7kΩ pull-up resistors are **required** for reliable I2C operation!

## Building Firmware

### Option 1: Using GitHub Actions (Recommended)

The GitHub Actions workflow automatically builds both BRD4151A and TRÅDFRI variants.

1. Fork or clone the repository
2. Push changes to trigger build
3. Download artifacts from Actions tab

### Option 2: Local Build

1. **Install Prerequisites** (see main README.md)

2. **Set environment variables**:
   ```bash
   export GSDK_DIR=/path/to/gecko_sdk_4.5.0
   export BOARD=custom  # or leave unset for TRÅDFRI
   export SLCP_FILE=zigbee_bme280_sensor_tradfri.slcp
   ```

3. **Run build**:
   ```bash
   ./tools/build.sh
   ```

4. **Output** will be in `firmware/build/debug/`:
   - `zigbee_bme280_sensor_tradfri.hex` - Intel HEX format
   - `zigbee_bme280_sensor_tradfri.s37` - Motorola S-record
   - `zigbee_bme280_sensor_tradfri.bin` - Raw binary

## Flashing the TRÅDFRI Board

### Hardware Setup

You need a **SWD debugger/programmer**:
- Segger J-Link (recommended)
- Silicon Labs Debug Adapter
- Black Magic Probe
- ST-Link (with OpenOCD)

### Connecting the Debugger

**TRÅDFRI to J-Link connections:**

```
TRÅDFRI   J-Link   Function
PF0    →  SWCLK  → Clock
PF1    →  SWDIO  → Data
PF2    →  SWO    → Trace (optional)
RESET  →  RESET  → Reset (optional)
GND    →  GND    → Ground
VCC    →  VTref  → Voltage reference (3.3V)
```

**Physical Connection Options:**

1. **Test clips** - Clip onto TRÅDFRI connector pins
2. **Soldering** - Solder wires to test points
3. **Pogo pins** - Spring-loaded contact probes
4. **Custom adapter** - PCB adapter for TRÅDFRI connector

### Method 1: Simplicity Commander (Recommended)

Silicon Labs Simplicity Commander is the official tool.

1. **Install Simplicity Commander**:
   - Download from [Silicon Labs](https://www.silabs.com/developers/mcu-programming-options)
   - Or install via `slt` CLI:
     ```bash
     slt install commander
     ```

2. **Erase chip** (first time only):
   ```bash
   commander device pageerase --device EFR32MG1P132F256GM32
   ```

3. **Flash firmware**:
   ```bash
   commander flash firmware/build/debug/zigbee_bme280_sensor_tradfri.s37 \
     --device EFR32MG1P132F256GM32
   ```

4. **Verify**:
   ```bash
   commander verify firmware/build/debug/zigbee_bme280_sensor_tradfri.s37 \
     --device EFR32MG1P132F256GM32
   ```

5. **Reset device**:
   ```bash
   commander device reset --device EFR32MG1P132F256GM32
   ```

### Method 2: J-Link Commander

If using Segger J-Link:

1. **Start J-Link Commander**:
   ```bash
   JLinkExe -device EFR32MG1P132F256GM32 -if SWD -speed 4000
   ```

2. **In J-Link prompt**:
   ```
   connect
   erase
   loadfile firmware/build/debug/zigbee_bme280_sensor_tradfri.hex
   verifybin firmware/build/debug/zigbee_bme280_sensor_tradfri.bin 0
   reset
   go
   exit
   ```

### Method 3: OpenOCD

For ST-Link or other debuggers:

1. **Create OpenOCD config** (`tradfri.cfg`):
   ```tcl
   source [find interface/stlink.cfg]
   transport select hla_swd

   set CHIPNAME efr32mg1p132f256gm32
   source [find target/efm32.cfg]

   adapter speed 1000
   ```

2. **Flash firmware**:
   ```bash
   openocd -f tradfri.cfg \
     -c "program firmware/build/debug/zigbee_bme280_sensor_tradfri.hex verify reset exit"
   ```

## OTA Updates (Using External Flash)

The TRÅDFRI variant includes OTA bootloader support using the external IS25LQ020B flash.

### Setup OTA

1. **Build with bootloader**:
   - The `.slcp` already includes bootloader interface
   - OTA storage configured for 256KB external flash

2. **Flash bootloader** (one-time):
   ```bash
   # Download appropriate bootloader from Silicon Labs
   commander flash bootloader-storage-spiflash-single-512k.s37 \
     --device EFR32MG1P132F256GM32
   ```

3. **OTA Update Process**:
   - Coordinator sends new firmware image
   - Image stored in external SPI flash (IS25LQ020B)
   - Bootloader swaps images on next reset
   - Old image backed up in external flash

### OTA Configuration

The external flash is configured in the `.slcp`:
- **Start address**: 0x0
- **End address**: 0x40000 (262144 bytes = 256KB)
- **Chip select**: PB11
- **SPI pins**: PD13 (CLK), PD14 (MISO), PD15 (MOSI)

## Testing the Firmware

### LED Status Indicators

After flashing, the LED (PA0) will indicate:
- **Off**: Not joined to network
- **Blinking (500ms)**: Joining network
- **Solid 3s then off**: Successfully joined
- **Brief flash**: Sensor reading triggered
- **Rapid blink 10x**: Sensor initialization error

### Button Functions (PB13)

- **Short press**: Immediate sensor read (or join if not joined)
- **Long press**: Leave/rejoin flow

Note: firmware includes guard/debounce logic to reduce false triggers on noisy boards.

### Debug Output

Connect UART to see debug messages:
- **TX**: Check GSDK default UART pins for EFR32MG1P132
- **Baud**: 115200
- **Format**: 8N1

## Troubleshooting

### Flash Failed

**Problem**: Cannot connect to chip
**Solutions**:
- Check SWD connections (SWCLK, SWDIO, GND)
- Verify 3.3V power supply
- Try lower SWD clock speed: `--speed 1000` or `--speed 500`
- Hold RESET while connecting

### OTA Not Working

**Problem**: OTA updates fail
**Solutions**:
- Verify SPI flash connections (PB11, PD13-15)
- Check external flash with: `commander flash --verify`
- Ensure bootloader is flashed first
- Check OTA cluster configuration in ZCL

### Sensor Not Reading

**Problem**: BME280 initialization fails
**Solutions**:
- Check I2C wiring (PC10, PC11)
- Verify 4.7kΩ pull-up resistors installed
- Check BME280 I2C address (SDO pin grounding)
- Try lower I2C speed: Change `BME280_I2C_FREQ` to 50000 (50 kHz)

### Network Join Fails

**Problem**: Cannot join Zigbee network
**Solutions**:
- Ensure coordinator permits joining
- Check antenna connection
- Verify 2.4GHz channel is correct
- Press button to manually trigger join

## Pin Usage Summary

**Available for expansion:**
- PA1 - GPIO
- PB12 - GPIO
- PB14 - GPIO
- PB15 - GPIO

**Reserved (in use):**
- PA0 - LED
- PB11 - SPI Flash CS
- PB13 - Button
- PC10 - I2C SDA (BME280)
- PC11 - I2C SCL (BME280)
- PD13-15 - SPI Flash
- PF0-2 - SWD Debug

## References

- [EFR32MG1P Datasheet](https://www.silabs.com/documents/public/data-sheets/efr32mg1-datasheet.pdf)
- [TRÅDFRI Hacking](https://github.com/basilfx/TRADFRI-Hacking)
- [Gecko SDK Documentation](https://docs.silabs.com/)
- [Simplicity Commander User Guide](https://www.silabs.com/documents/public/user-guides/ug162-simplicity-commander-reference-guide.pdf)
