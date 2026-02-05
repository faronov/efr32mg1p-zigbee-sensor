# TRÅDFRI Bootloader - SPI Flash Storage

Custom Gecko bootloader for IKEA TRÅDFRI board (EFR32MG1P132F256GM32) with external IS25LQ020B (256KB) SPI flash for OTA updates.

## Overview

This bootloader enables Over-The-Air (OTA) firmware updates by:
1. Storing downloaded images in external SPI flash
2. Verifying image integrity and signatures
3. Swapping images on reboot (new image → internal flash)
4. Providing rollback capability if update fails

## Hardware Configuration

### Target Device
- **MCU**: EFR32MG1P132F256GM32 (32-pin QFN)
- **Flash**: ISSI IS25LQ020B (256KB / 2Mbit SPI)
- **Board**: IKEA TRÅDFRI

### SPI Flash Connections

| Signal | EFR32 Pin | GPIO | Physical Pin |
|--------|-----------|------|--------------|
| CLK | PD13 | Port D, Pin 13 | 1 |
| MISO | PD14 | Port D, Pin 14 | 2 |
| MOSI | PD15 | Port D, Pin 15 | 3 |
| CS | PB11 | Port B, Pin 11 | 6 |

**SPI Controller**: USART0 hardware SPI (RXLOC21/TXLOC23/CLKLOC19)

### Optional GPIO

- **Button**: PB13 (Pin 42) - Hold during boot to enter bootloader mode
- **LED**: PA0 (Pin 24) - Indicates bootloader activity

## Features

### Storage
- **Slot 0**: Full 256KB external flash for OTA image storage
- **Address Range**: 0x0 to 0x3FFFF (262144 bytes)
- **Image Verification**: CRC and signature validation

### Security
- AES-128 encryption support
- ECDSA signature verification
- Secure boot capability (optional)

### Compression
- LZ4 compression support for smaller OTA images
- Decompression during image swap

### Debug
- UART debug output (optional)
- LED indication during bootloader operations

## Memory Layout

```
Internal Flash (256 KB):
┌────────────────────────┐ 0x00000000
│  Bootloader (16-24 KB) │  ← This bootloader
├────────────────────────┤ 0x00004000 / 0x00006000
│  Application Firmware  │  ← Your Zigbee application
│  (200-230 KB)          │
├────────────────────────┤
│  NVM3 Storage (24 KB)  │  ← Zigbee network data
└────────────────────────┘ 0x0003FFFF

External SPI Flash (256 KB):
┌────────────────────────┐ 0x00000000
│  Slot 0: OTA Download  │  ← Downloaded firmware images
│  Storage (256 KB)      │
└────────────────────────┘ 0x0003FFFF
```

## Building the Bootloader

### Prerequisites
- Gecko SDK 4.5
- ARM GCC toolchain
- SLC CLI

### Using Build Script

```bash
# Set GSDK path
export GSDK_DIR=/path/to/gecko_sdk_4.5.0

# Build bootloader
./tools/build_bootloader.sh
```

Build output will be in `bootloader/tradfri-spiflash/build/`:
- `tradfri-bootloader-spiflash.s37` - Main bootloader image
- `tradfri-bootloader-spiflash-crc.s37` - With CRC for production
- `tradfri-bootloader-spiflash.hex` - Intel HEX format

### Manual Build

```bash
cd bootloader/tradfri-spiflash

# Generate project with SLC
slc generate tradfri-bootloader-spiflash.slcp \
  -np -d build -o makefile \
  --with EFR32MG1P132F256GM32

# Build
cd build
make -f tradfri-bootloader-spiflash.Makefile -j4
```

## Flashing the Bootloader

### IMPORTANT: One-Time Installation

The bootloader must be flashed **once** before the application firmware.

```bash
# Using Simplicity Commander
commander flash bootloader/tradfri-spiflash/build/tradfri-bootloader-spiflash.s37 \
  --device EFR32MG1P132F256GM32

# Verify bootloader info
commander bootloader info --device EFR32MG1P132F256GM32
```

Expected output:
```
Bootloader version: 1.0.0
Storage: SPI Flash
Slots: 1
  Slot 0: 0x0 - 0x3FFFF (256 KB)
```

### Using J-Link

```bash
JLinkExe -device EFR32MG1P132F256GM32 -if SWD -speed 4000
> loadfile bootloader/tradfri-spiflash/build/tradfri-bootloader-spiflash.hex
> r
> g
> exit
```

## Testing Flash Communication

After flashing bootloader, test SPI flash communication:

```bash
# Read bootloader info (includes flash detection)
commander bootloader info --device EFR32MG1P132F256GM32

# Should show:
# Flash detected: IS25LQ020B (JEDEC ID: 0x9D4012)
# Flash size: 256 KB
```

If flash not detected, check:
1. SPI pin connections
2. Flash chip power (3.3V)
3. PF3 flash enable pin state (must be high)

## Using the Bootloader

### Normal Operation

When application firmware with OTA support is running:

1. **Download Phase**
   - Coordinator sends new firmware image
   - Application stores in external flash (Slot 0)
   - Progress reported via Zigbee

2. **Verification Phase**
   - Application verifies image CRC/signature
   - Marks image as "ready for update"

3. **Reboot Phase**
   - User reboots device (or automatic timeout)
   - Bootloader detects new image in Slot 0
   - LED blinks during image swap

4. **Swap Phase**
   - Bootloader backs up current application
   - Copies new image from external to internal flash
   - Verifies new image
   - Boots new application

5. **Rollback** (if new image fails to boot)
   - Bootloader detects boot failure
   - Restores previous image from backup
   - Boots previous working firmware

### Manual Bootloader Entry

Hold button (PB13) during power-up to enter bootloader mode:
- LED will blink rapidly
- Device ready for firmware upload via UART XMODEM (if enabled)
- Release button to continue normal boot

## Troubleshooting

### Bootloader Won't Build

**Error**: `Flash configuration invalid`
- **Solution**: Check `btl_spiflash_storage_cfg.h` values match IS25LQ020B specs

### Flash Not Detected

**Symptom**: `commander bootloader info` shows "No flash detected"

**Debug**:
1. Verify SPI pins are correctly configured
2. Check flash chip power (3.3V on VCC pin)
3. Verify JEDEC ID read command works
4. Test with logic analyzer if available

**Solutions**:
- Check `config/btl_spi_controller_usart_driver_cfg.h` pin/LOC assignments
- Ensure PF3 is driven high to enable the flash
- Ensure no conflicts with application GPIO usage

### OTA Update Fails

**Symptom**: Download completes but bootloader doesn't swap images

**Debug**:
```bash
# Check slot status
commander bootloader info --device EFR32MG1P132F256GM32

# Should show:
# Slot 0: Image present, verified, ready for update
```

**Solutions**:
- Ensure application firmware includes `bootloader_interface` component
- Verify application calls `bootloader_setImageToBootload(0)` after download
- Check that image size fits in 256KB flash

### Device Bricked

**Symptom**: Device won't boot after OTA

**Recovery**:
```bash
# 1. Connect via SWD
# 2. Mass erase
commander device pageerase --device EFR32MG1P132F256GM32

# 3. Reflash bootloader
commander flash tradfri-bootloader-spiflash.s37 --device EFR32MG1P132F256GM32

# 4. Reflash known-good application
commander flash known_good_firmware.s37 --device EFR32MG1P132F256GM32
```

## Configuration Files

### Key Files

- **tradfri-bootloader-spiflash.slcp** - Project configuration
- **config/btl_spiflash_storage_cfg.h** - Flash chip configuration
- **config/btl_spi_controller_usart_driver_cfg.h** - SPI pin and LOC configuration

### Customization

To modify for different hardware:

1. **Different SPI pins**: Edit `config/btl_spi_controller_usart_driver_cfg.h`
2. **Different flash chip**: Edit `btl_spiflash_storage_cfg.h`
3. **Different device**: Update `device:` in .slcp file

## Version History

- **v1.1.2** - Switch SPI flash to USART0
  - IS25LQ020B 256KB flash support
  - Hardware SPI via USART0 LOCs RX=21 / TX=23 / CLK=19
  - Single OTA slot (256KB)

## References

- [Gecko Bootloader Documentation](https://docs.silabs.com/gecko-platform/latest/bootloader/)
- [EFR32MG1P Reference Manual](https://www.silabs.com/documents/public/reference-manuals/efr32xg1-rm.pdf)
- [IS25LQ020B Datasheet](https://www.issi.com/WW/pdf/25LQ025B-512B-010B-020B-040B.pdf)

## License

Based on Silicon Labs Gecko Bootloader (Zlib license).
TRÅDFRI-specific modifications are provided under MIT license.
