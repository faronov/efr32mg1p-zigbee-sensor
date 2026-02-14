# Bootloaders

This directory contains custom bootloader projects for different hardware configurations.

## Available Bootloaders

### tradfri-spiflash

**Custom bootloader for IKEA TRÅDFRI board with IS25LQ020B SPI flash**

- Target: EFR32MG1P132F256GM32
- Storage: 256KB external SPI flash (IS25LQ020B)
- Features: OTA update support, image verification, rollback protection
- Documentation: [tradfri-spiflash/README_BOOTLOADER.md](tradfri-spiflash/README_BOOTLOADER.md)

**Quick Start:**
```bash
# Build bootloader
export GSDK_DIR=/path/to/gecko_sdk
./tools/build_bootloader.sh

# Flash bootloader (one-time)
commander flash bootloader/tradfri-spiflash/build/tradfri-bootloader-spiflash.s37 \
  --device EFR32MG1P132F256GM32
```

## Why Custom Bootloaders?

Standard Silicon Labs bootloaders are configured for development boards with standard pin assignments. Custom boards like TRÅDFRI require:

1. **Custom Pin Routing**: TRÅDFRI uses different GPIO pins for SPI flash
2. **Specific Flash Configuration**: IS25LQ020B has specific JEDEC ID and timing requirements
3. **Memory Layout**: Optimized for 256KB flash with OTA storage

## Bootloader vs Application Firmware

**Bootloader** (this directory):
- Flashed **once** to device
- Manages firmware updates
- Cannot be updated via OTA (requires SWD programmer)
- Small size (16-24 KB)

**Application Firmware** (main project):
- Can be updated via OTA
- Contains your Zigbee application
- Larger size (200-230 KB)

## Memory Map

```
┌─────────────────────────────────┐ 0x00000000
│  Bootloader (16-24 KB)          │ ← Built from this directory
│  - SPI flash drivers            │
│  - Image verification           │
│  - Image swap logic             │
├─────────────────────────────────┤ 0x00004000 / 0x00006000
│  Application (200-230 KB)       │ ← Main project firmware
│  - Zigbee stack                 │
│  - Sensor logic                 │
│  - OTA client                   │
├─────────────────────────────────┤
│  NVM3 (24 KB)                   │ ← Network data
│  - Zigbee keys                  │
│  - Network info                 │
└─────────────────────────────────┘ 0x0003FFFF
```

## Building Bootloaders

### Using Build Script (Recommended)

```bash
# Set GSDK path
export GSDK_DIR=/path/to/gecko_sdk_4.5.0

# Build all bootloaders
./tools/build_bootloader.sh

# Or build specific bootloader
cd bootloader/tradfri-spiflash
../../tools/build_bootloader.sh
```

### Manual Build

```bash
cd bootloader/tradfri-spiflash

# Generate project
slc generate tradfri-bootloader-spiflash.slcp \
  -np -d build -o makefile \
  --with EFR32MG1P132F256GM32

# Build
cd build
make -f tradfri-bootloader-spiflash.Makefile -j4
```

## Flashing Bootloaders

**IMPORTANT**: Flash bootloader **before** application firmware.

### Using Simplicity Commander

```bash
# 1. Erase device (optional but recommended)
commander device pageerase --device EFR32MG1P132F256GM32

# 2. Flash bootloader
commander flash bootloader/tradfri-spiflash/build/tradfri-bootloader-spiflash.s37 \
  --device EFR32MG1P132F256GM32

# 3. Verify bootloader
commander bootloader info --device EFR32MG1P132F256GM32
```

Expected output:
```
Bootloader version: 1.0.0
Storage type: SPI Flash
Flash detected: IS25LQ020B (JEDEC ID: 0x9D4012)
Storage slots: 1
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

## Verifying Bootloader

After flashing, verify the bootloader is working:

```bash
# Check bootloader info
commander bootloader info --device EFR32MG1P132F256GM32

# Check for these items:
# ✓ Bootloader version displayed
# ✓ Storage type: SPI Flash
# ✓ Flash detected: IS25LQ020B
# ✓ Slot 0 configured correctly
```

If flash is **not detected**:
1. Check SPI pin configuration in `config/btl_spi_controller_usart_driver_cfg.h`
2. Verify USART routing (RXLOC21/TXLOC23/CLKLOC19 for TRÅDFRI)
3. Check flash chip power (3.3V)
4. Verify physical connections

## Creating New Bootloaders

To create a bootloader for different hardware:

1. **Copy existing bootloader**:
   ```bash
   cp -r bootloader/tradfri-spiflash bootloader/my-board-spiflash
   ```

2. **Modify configuration**:
   - Edit `.slcp` file: update device, project name
   - Edit `config/btl_spi_controller_usart_driver_cfg.h`: change pins and USART location
   - Edit `config/btl_spiflash_storage_cfg.h`: update flash chip specs

3. **Build and test**:
   ```bash
   cd bootloader/my-board-spiflash
   ../../tools/build_bootloader.sh
   ```

## Troubleshooting

### Bootloader Won't Build

**Issue**: SLC generation fails
- Check GSDK_DIR is set correctly
- Verify device part number in .slcp matches your hardware
- Ensure all config files are present

### Flash Not Detected

**Issue**: `commander bootloader info` shows "No flash detected"
- Verify SPI pin configuration matches hardware
- Check USART location is correct for your pins
- Test SPI signals with logic analyzer
- Verify flash chip power and connections

### Bootloader Too Large

**Issue**: Bootloader >24KB, doesn't fit
- Disable debug UART output
- Remove optional components (compression, GPIO activation)
- Use release build configuration

### OTA Updates Fail

**Issue**: Download completes but image doesn't swap
- Ensure application includes `bootloader_interface` component
- Verify application calls `bootloader_setImageToBootload(0)`
- Check slot configuration matches between bootloader and application

## Reference Documentation

- [Gecko Bootloader Developer Guide](https://docs.silabs.com/gecko-platform/latest/bootloader/)
- [OTA Setup Guide](../docs/OTA_SETUP_GUIDE.md)
- [TRÅDFRI Bootloader README](tradfri-spiflash/README_BOOTLOADER.md)

## License

Based on Silicon Labs Gecko Bootloader (Zlib license).
Custom modifications provided under MIT license.
