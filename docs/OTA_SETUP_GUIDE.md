# OTA Setup Guide for TRÅDFRI Board

This guide explains how to properly configure the external SPI flash and bootloader for Over-The-Air (OTA) firmware updates on the IKEA TRÅDFRI board.

## Table of Contents

1. [Flash Chip Identification](#flash-chip-identification)
2. [SPI Flash Pin Configuration](#spi-flash-pin-configuration)
3. [Bootloader Requirements](#bootloader-requirements)
4. [Creating a Custom Bootloader](#creating-a-custom-bootloader)
5. [Enabling OTA in Firmware](#enabling-ota-in-firmware)
6. [Testing OTA Updates](#testing-ota-updates)
7. [Troubleshooting](#troubleshooting)

---

## Flash Chip Identification

### Actual Flash Chip

**Confirmed**: IKEA TRÅDFRI board uses **ISSI IS25LQ020B** flash chip

| Specification | Value |
|---------------|-------|
| Model | IS25LQ020B |
| Capacity | **256 KB** (2 Mbit) |
| Interface | SPI (Serial Peripheral Interface) |
| Voltage | 2.3V - 3.6V (compatible with 3.3V TRÅDFRI) |
| Max Frequency | 104 MHz (we use 4 MHz) |
| Page Size | 256 bytes |
| Sector Size | 4 KB |
| Block Size | 32 KB / 64 KB |

**Note**: Some documentation incorrectly mentions MX25R8035F (1MB). The correct chip is **IS25LQ020B (256KB)**.

### Datasheet Reference

- [ISSI IS25LQ020B Datasheet](https://www.issi.com/WW/pdf/25LQ025B-512B-010B-020B-040B.pdf)

---

## SPI Flash Pin Configuration

### Physical Connections

The IS25LQ020B flash is already soldered on the TRÅDFRI board with the following connections:

| EFR32 Pin | GPIO | Function | IS25LQ020B Pin |
|-----------|------|----------|----------------|
| Pin 1 | PD13 | SPI CLK | Clock |
| Pin 2 | PD14 | SPI MISO | Data Out (SO) |
| Pin 3 | PD15 | SPI MOSI | Data In (SI) |
| Pin 6 | PB11 | SPI CS | Chip Select (/CS) |
| - | 3.3V | VCC | Power |
| - | GND | GND | Ground |

### SPI Wiring Diagram

```
EFR32MG1P132                             IS25LQ020B Flash
┌─────────────┐                         ┌────────────────┐
│             │                         │                │
│   PD13 ──────┼─────────────────────────┤ CLK            │
│             │    (Clock)              │                │
│   PD14 ──────┼─────────────────────────┤ SO (MISO)      │
│             │    (Data Out)           │                │
│   PD15 ──────┼─────────────────────────┤ SI (MOSI)      │
│             │    (Data In)            │                │
│   PB11 ──────┼─────────────────────────┤ /CS            │
│             │    (Chip Select)        │                │
│             │                         │                │
│   3.3V ──────┼─────────────────────────┤ VCC            │
│             │                         │                │
│   GND  ──────┼─────────────────────────┤ VSS            │
│             │                         │                │
└─────────────┘                         └────────────────┘
```

---

## Bootloader Requirements

### Why We Need a Custom Bootloader

The standard Gecko bootloader needs to be configured specifically for:

1. **Flash Type**: IS25LQ020B SPI flash
2. **SPI Pin Configuration**: Custom TRÅDFRI pins (not standard dev board)
3. **Storage Layout**: 256KB flash partition
4. **Device**: EFR32MG1P132F256GM32 (not the standard development board)

### Bootloader Types

Silicon Labs provides several bootloader variants:

| Bootloader Type | Storage | Use Case |
|-----------------|---------|----------|
| `bootloader-storage-internal` | Internal flash only | No OTA |
| `bootloader-storage-spiflash` | External SPI flash | **OTA (our choice)** |
| `bootloader-uart-xmodem` | UART | Serial update only |

We need: **`bootloader-storage-spiflash`** variant

---

## Creating a Custom Bootloader

### Option 1: Use Simplicity Studio (Recommended)

#### Step 1: Create Bootloader Project

1. Open **Simplicity Studio**
2. Click **File → New → Silicon Labs Project Wizard**
3. Select your device: **EFR32MG1P132F256GM32**
4. Select **Gecko Bootloader**
5. Choose template: **Bootloader - SoC SPI Flash Storage**
6. Name: `tradfri-bootloader-spiflash`

#### Step 2: Configure SPI Flash Component

1. Open **Software Components**
2. Find **SPI Flash Storage** component
3. Configure:
   - **Flash Type**: Select "ISSI IS25LQ020B" or "Generic SPI Flash"
   - **Size**: 256 KB (262144 bytes)
   - **Page Size**: 256 bytes
   - **Sector Size**: 4096 bytes

#### Step 3: Configure SPI Pins

1. Open **Pin Tool** (Hardware Configurator)
2. Navigate to **USART0** (used for SPI)
3. Configure pins:
   ```
   USART0_CLK  → PD13
   USART0_RX   → PD14 (MISO)
   USART0_TX   → PD15 (MOSI)
   USART0_CS   → PB11
   ```

4. **Location**: Find the correct USART0 location number for these pins
   - For EFR32MG1P, check datasheet for LOC mapping
   - Typically: Location 1 or Location 4

#### Step 4: Configure Storage Slots

1. Open **Bootloader Core** component
2. Configure **Application Image Storage**:
   - **Slot 0 Start**: 0x0000 (start of SPI flash)
   - **Slot 0 End**: 0x3FFFF (256KB - 1)
   - **Application Properties**: Enable verification

#### Step 5: Build Bootloader

1. Click **Build** (hammer icon)
2. Output file will be in:
   ```
   GNU ARM vX.X/tradfri-bootloader-spiflash.s37
   ```

3. **Important**: Keep both files:
   - `tradfri-bootloader-spiflash.s37` - Bootloader image
   - `tradfri-bootloader-spiflash-crc.s37` - With CRC for production

---

### Option 2: Modify Existing Bootloader (Advanced)

If you have a reference bootloader, modify it:

#### Required Files

```
bootloader/
├── bootloader-storage-spiflash-single/
│   ├── bootloader-storage-spiflash-single.slcp
│   ├── config/
│   │   ├── btl_storage_slot_cfg.h
│   │   └── btl_spiflash_storage_cfg.h
│   └── hardware/
│       └── kit/
│           └── config/
│               └── btl_spi_peripheral_usart_driver_cfg.h
```

#### Modify SPI Configuration

Edit `btl_spi_peripheral_usart_driver_cfg.h`:

```c
// SPI Flash Configuration for TRÅDFRI
#define SL_USART_EXTFLASH_PERIPHERAL          USART0
#define SL_USART_EXTFLASH_PERIPHERAL_NO       0

// Pin Configuration
#define SL_USART_EXTFLASH_TX_PORT             gpioPortD
#define SL_USART_EXTFLASH_TX_PIN              15
#define SL_USART_EXTFLASH_RX_PORT             gpioPortD
#define SL_USART_EXTFLASH_RX_PIN              14
#define SL_USART_EXTFLASH_CLK_PORT            gpioPortD
#define SL_USART_EXTFLASH_CLK_PIN             13
#define SL_USART_EXTFLASH_CS_PORT             gpioPortB
#define SL_USART_EXTFLASH_CS_PIN              11

// Location (check EFR32MG1P datasheet)
#define SL_USART_EXTFLASH_LOCATION            _USART_ROUTELOC0_TXLOC_LOC4

// Bitrate
#define SL_USART_EXTFLASH_BITRATE             4000000  // 4 MHz
```

#### Modify Flash Configuration

Edit `btl_spiflash_storage_cfg.h`:

```c
// Flash Device Configuration
#define SPIFLASH_DEVICE_SIZE_BYTES            262144  // 256 KB
#define SPIFLASH_PAGE_SIZE                    256
#define SPIFLASH_SECTOR_SIZE                  4096
#define SPIFLASH_DEVICE_JEDEC_ID              0x9D4012  // IS25LQ020B JEDEC ID

// Storage Slot Configuration
#define STORAGE_START_ADDRESS                 0x0
#define STORAGE_END_ADDRESS                   0x3FFFF  // 256KB - 1
```

---

## Enabling OTA in Firmware

### Step 1: Enable SPIDRV Component

The key issue is that SPIDRV needs proper pin routing. Here's how to fix it:

#### Method A: Use Board File (Recommended)

Create a custom board file: `board/brd_custom_tradfri.h`

```c
#ifndef BRD_CUSTOM_TRADFRI_H
#define BRD_CUSTOM_TRADFRI_H

#include "em_device.h"

// SPI Flash Configuration
#define BSP_EXTFLASH_USART          USART0
#define BSP_EXTFLASH_MOSI_PORT      gpioPortD
#define BSP_EXTFLASH_MOSI_PIN       15
#define BSP_EXTFLASH_MISO_PORT      gpioPortD
#define BSP_EXTFLASH_MISO_PIN       14
#define BSP_EXTFLASH_CLK_PORT       gpioPortD
#define BSP_EXTFLASH_CLK_PIN        13
#define BSP_EXTFLASH_CS_PORT        gpioPortB
#define BSP_EXTFLASH_CS_PIN         11

// USART Location for SPI (check datasheet)
#define BSP_EXTFLASH_USART_LOC      USART_ROUTELOC0_RXLOC_LOC4  | \
                                    USART_ROUTELOC0_TXLOC_LOC4  | \
                                    USART_ROUTELOC0_CLKLOC_LOC4

#endif // BRD_CUSTOM_TRADFRI_H
```

#### Method B: Direct SLCP Configuration

Modify `zigbee_bme280_sensor_tradfri.slcp`:

```yaml
component:
  # Enable OTA components
  - id: zigbee_ota_client_policy
  - id: zigbee_ota_storage_simple
  - id: zigbee_ota_storage_simple_eeprom
  - id: bootloader_interface
  - id: mx25_flash_shutdown_usart

  # Enable SPIDRV with custom instance
  - id: spidrv
    instance:
      - exp

configuration:
  # SPI Flash Pin Configuration
  - name: SL_SPIDRV_EXP_BITRATE
    value: 4000000

  # Chip Select
  - name: SL_SPIDRV_EXP_CS_PORT
    value: gpioPortB
  - name: SL_SPIDRV_EXP_CS_PIN
    value: 11

  # Clock
  - name: SL_SPIDRV_EXP_CLK_PORT
    value: gpioPortD
  - name: SL_SPIDRV_EXP_CLK_PIN
    value: 13

  # MOSI (Master Out)
  - name: SL_SPIDRV_EXP_MOSI_PORT
    value: gpioPortD
  - name: SL_SPIDRV_EXP_MOSI_PIN
    value: 15

  # MISO (Master In)
  - name: SL_SPIDRV_EXP_MISO_PORT
    value: gpioPortD
  - name: SL_SPIDRV_EXP_MISO_PIN
    value: 14

  # USART Location (CRITICAL - check datasheet!)
  - name: SL_SPIDRV_EXP_RX_LOC
    value: 4  # LOC4 for PD14
  - name: SL_SPIDRV_EXP_TX_LOC
    value: 4  # LOC4 for PD15
  - name: SL_SPIDRV_EXP_CLK_LOC
    value: 4  # LOC4 for PD13

  # OTA Storage Configuration
  - name: EMBER_AF_PLUGIN_OTA_STORAGE_SIMPLE_EEPROM_STORAGE_START
    value: 0
  - name: EMBER_AF_PLUGIN_OTA_STORAGE_SIMPLE_EEPROM_STORAGE_END
    value: 262144  # 256KB

  # MX25 Flash Shutdown CS Pin
  - name: SL_MX25_FLASH_SHUTDOWN_CS_PORT
    value: gpioPortB
  - name: SL_MX25_FLASH_SHUTDOWN_CS_PIN
    value: 11
```

### Step 2: Verify USART Location

**CRITICAL**: Check the EFR32MG1P132 datasheet for the correct USART0 location.

For pins PD13/PD14/PD15, the typical location is **LOC4**. Verify in datasheet:

```
USART0 Location Mapping (EFR32MG1P):
- LOC0: Default pins
- LOC1: Alternate 1
- LOC2: Alternate 2
- LOC3: Alternate 3
- LOC4: PD13 (CLK), PD14 (RX/MISO), PD15 (TX/MOSI)  ← Our configuration
```

### Step 3: Build Firmware

```bash
export GSDK_DIR=/path/to/gecko_sdk
export BOARD=custom
export SLCP_FILE=zigbee_bme280_sensor_tradfri.slcp
./tools/build.sh
```

---

## Complete Flash and Boot Procedure

### One-Time Bootloader Installation

**Important**: Bootloader must be flashed **before** the application firmware.

```bash
# 1. Erase the device (optional but recommended)
commander device pageerase --device EFR32MG1P132F256GM32

# 2. Flash the bootloader (one-time only)
commander flash tradfri-bootloader-spiflash.s37 \
  --device EFR32MG1P132F256GM32

# 3. Flash the application firmware
commander flash firmware/build/release/zigbee_bme280_sensor_tradfri.s37 \
  --device EFR32MG1P132F256GM32

# 4. Reset the device
commander device reset
```

### Memory Layout

After flashing, the device memory layout will be:

```
Internal Flash (256 KB):
┌────────────────────────┐ 0x00000000
│  Bootloader (16-24 KB) │
├────────────────────────┤ 0x00004000 / 0x00006000
│  Application Firmware  │
│  (200-230 KB)          │
├────────────────────────┤
│  NVM3 Storage (24 KB)  │
└────────────────────────┘ 0x0003FFFF

External SPI Flash (256 KB):
┌────────────────────────┐ 0x00000000
│  Slot 0: Download      │
│  Storage for OTA image │
│  (256 KB)              │
└────────────────────────┘ 0x0003FFFF
```

---

## Testing OTA Updates

### Prerequisites

1. Bootloader flashed to device
2. Application firmware with OTA support running
3. Zigbee coordinator (e.g., Zigbee2MQTT, ZHA)
4. New firmware image (`.ota` or `.gbl` file)

### Create OTA Image

Use Simplicity Commander to create an OTA-compatible image:

```bash
# Generate GBL file (Gecko Bootloader format)
commander gbl create \
  firmware/build/release/zigbee_bme280_sensor_tradfri.gbl \
  --app firmware/build/release/zigbee_bme280_sensor_tradfri.s37 \
  --device EFR32MG1P132F256GM32

# Convert GBL to Zigbee OTA format
commander gbl create \
  firmware/build/release/zigbee_bme280_sensor_tradfri.ota \
  --app firmware/build/release/zigbee_bme280_sensor_tradfri.s37 \
  --device EFR32MG1P132F256GM32 \
  --zigbee-ota
```

### Perform OTA Update

#### Using Zigbee2MQTT

1. Copy `.ota` file to Zigbee2MQTT OTA directory
2. In Zigbee2MQTT UI, navigate to device
3. Click **OTA** tab
4. Click **Check for update**
5. Click **Update**
6. Monitor progress

#### Using ZHA (Home Assistant)

1. Copy `.ota` file to `<config>/zigbee_ota/` directory
2. Navigate to device in ZHA
3. Select **Manage Zigbee Device** → **Update Firmware**
4. Select firmware file
5. Monitor progress

### OTA Update Process

```
1. Coordinator sends OTA image to device
   [====================] 100% (takes 5-15 minutes for 256KB)

2. Device stores image in external SPI flash
   Status: "Downloading" → "Downloaded" → "Validating"

3. Device requests reboot to apply update
   User must approve or device auto-reboots after timeout

4. On reboot, bootloader:
   - Verifies new image CRC
   - Backs up current application to flash
   - Copies new image from external flash to internal flash
   - Marks update complete
   - Boots new application

5. Device joins network with new firmware
   Check version number to confirm update succeeded
```

### Expected Timeline

| Phase | Duration | Notes |
|-------|----------|-------|
| Download | 5-15 min | Depends on Zigbee network quality |
| Storage | 1-2 min | Writing to external flash |
| Verification | 10-30 sec | CRC check |
| Reboot | 5 sec | User-initiated or auto |
| Bootloader Swap | 30-60 sec | Internal flash rewrite |
| Network Rejoin | 10-30 sec | Zigbee network join |
| **Total** | **~10-20 min** | End-to-end OTA update |

---

## Troubleshooting

### Issue: SPIDRV Component Won't Build

**Error**: `SPIDRV component requires proper pin routing configuration`

**Solution**:
1. Check that USART location (LOC) values are correct for your pins
2. Verify pin definitions match physical connections
3. Try using a board file instead of SLCP defines
4. Check EFR32MG1P132 datasheet for correct LOC values

### Issue: Flash Not Detected

**Symptoms**: Bootloader fails to initialize, OTA storage reports "no flash"

**Debug Steps**:

```bash
# 1. Test SPI communication manually (requires debugger)
# In GDB or Simplicity Debugger:
# - Verify CS pin toggles
# - Verify CLK signal present
# - Check MISO/MOSI data

# 2. Check power to flash chip
# - Measure VCC on flash (should be 3.3V)
# - Check GND connection

# 3. Verify JEDEC ID read
# - Send command 0x9F to read JEDEC ID
# - Expected: 0x9D4012 for IS25LQ020B
```

**Common Causes**:
- Wrong pin configuration
- Incorrect USART location (LOC)
- Poor solder connections on TRÅDFRI board
- Flash chip not powered

### Issue: OTA Download Fails

**Symptoms**: OTA starts but fails during download

**Solutions**:
1. **Check storage configuration**:
   - Ensure `STORAGE_END` is 262144 (256KB)
   - Verify bootloader flash size matches firmware config

2. **Check Zigbee signal strength**:
   - Poor signal = packet loss = failed download
   - Move device closer to coordinator during OTA

3. **Check firmware size**:
   - Firmware must fit in 256KB flash
   - Use `commander info` to check image size
   - If too large, reduce features or optimize

### Issue: Bootloader Won't Swap Images

**Symptoms**: Download completes, device reboots, but old firmware still running

**Debug**:

```bash
# Check bootloader info
commander bootloader info --device EFR32MG1P132F256GM32

# Expected output:
# Bootloader version: X.X.X
# Storage: SPI Flash
# Slots: 1
# Slot 0: 0x0 - 0x3FFFF (valid image present)
```

**Solutions**:
1. Verify bootloader is correctly configured for SPI flash
2. Check that application has `bootloader_interface` component
3. Ensure CRC/signature verification passes
4. Try reflashing bootloader

### Issue: Device Bricked After OTA

**Symptoms**: Device won't boot, no network activity

**Recovery**:

```bash
# 1. Connect J-Link debugger via SWD
# 2. Erase and reflash bootloader
commander device pageerase --device EFR32MG1P132F256GM32
commander flash tradfri-bootloader-spiflash.s37 --device EFR32MG1P132F256GM32

# 3. Reflash known-good application
commander flash backup_firmware.s37 --device EFR32MG1P132F256GM32

# 4. Reset
commander device reset
```

---

## Checking USART Location (Reference)

To find the correct USART location for your pins, check the EFR32MG1P132 datasheet:

**EFR32MG1 Reference Manual - USART Chapter**

For USART0 on EFR32MG1P132:

```c
// Location 0
#define USART0_TX_LOC0    _USART_ROUTELOC0_TXLOC_LOC0    // PF3
#define USART0_RX_LOC0    _USART_ROUTELOC0_RXLOC_LOC0    // PF4
#define USART0_CLK_LOC0   _USART_ROUTELOC0_CLKLOC_LOC0   // PF5

// Location 1
#define USART0_TX_LOC1    _USART_ROUTELOC0_TXLOC_LOC1    // PE10
#define USART0_RX_LOC1    _USART_ROUTELOC0_RXLOC_LOC1    // PE11
#define USART0_CLK_LOC1   _USART_ROUTELOC0_CLKLOC_LOC1   // PE12

// Location 4 (TRÅDFRI Configuration)
#define USART0_TX_LOC4    _USART_ROUTELOC0_TXLOC_LOC4    // PD15 ← MOSI
#define USART0_RX_LOC4    _USART_ROUTELOC0_RXLOC_LOC4    // PD14 ← MISO
#define USART0_CLK_LOC4   _USART_ROUTELOC0_CLKLOC_LOC4   // PD13 ← CLK
```

**Our configuration uses LOC4** because TRÅDFRI uses PD13/PD14/PD15 for SPI.

---

## Summary Checklist

### Before Enabling OTA

- [ ] Verify flash chip is IS25LQ020B (256KB)
- [ ] Confirm SPI pin connections: PD13, PD14, PD15, PB11
- [ ] Check EFR32MG1P132 datasheet for correct USART LOC
- [ ] Understand bootloader memory layout

### Bootloader Creation

- [ ] Create bootloader project in Simplicity Studio
- [ ] Configure for IS25LQ020B flash (256KB)
- [ ] Set SPI pins: PD13 (CLK), PD14 (MISO), PD15 (MOSI), PB11 (CS)
- [ ] Set USART location to LOC4
- [ ] Configure storage slot: 0x0 - 0x3FFFF
- [ ] Build bootloader and save `.s37` file

### Firmware Configuration

- [ ] Uncomment OTA components in `.slcp` file
- [ ] Configure SPIDRV with correct pins and LOC
- [ ] Set storage addresses to match bootloader
- [ ] Build firmware with OTA support enabled
- [ ] Verify firmware size < 230KB (to fit with bootloader)

### Flashing

- [ ] Flash bootloader first (one-time)
- [ ] Flash application firmware
- [ ] Verify device boots and joins network
- [ ] Check serial output for OTA init messages

### Testing

- [ ] Create OTA image using `commander gbl create`
- [ ] Upload to coordinator OTA directory
- [ ] Initiate OTA update from coordinator
- [ ] Monitor download progress
- [ ] Verify device reboots and applies update
- [ ] Check firmware version after update

---

## Additional Resources

- [EFR32MG1P Reference Manual](https://www.silabs.com/documents/public/reference-manuals/efr32xg1-rm.pdf)
- [Silicon Labs Bootloader Documentation](https://docs.silabs.com/gecko-platform/latest/bootloader/)
- [IS25LQ020B Datasheet](https://www.issi.com/WW/pdf/25LQ025B-512B-010B-020B-040B.pdf)
- [Zigbee OTA Upgrade Cluster Spec](https://zigbeealliance.org/wp-content/uploads/2019/12/07-5123-06-zigbee-cluster-library-specification.pdf)

---

**Note**: This is an advanced configuration. If you're unsure about any step, test with a development board first before modifying your TRÅDFRI device.
