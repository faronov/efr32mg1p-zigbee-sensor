# Bootloader Flashing Guide - Series 1 (EFR32MG1P)

## Overview

The EFR32MG1P (Series 1) uses a **two-stage bootloader architecture**:

- **First Stage** (2 KB at 0x000-0x7FF): Reset vector and bootloader updater
- **Main Stage** (14 KB at 0x800-0x3FFF): OTA logic and SPI flash storage

## Memory Layout

```
┌────────────────────────┐ 0x00000000
│  First Stage (2 KB)    │ ← Contains CPU reset vector at 0x000
├────────────────────────┤ 0x00000800
│  Main Bootloader (14KB)│ ← OTA logic, SPI flash driver
├────────────────────────┤ 0x00004000
│  Application (240 KB)  │ ← Your firmware
│                        │
├────────────────────────┤
│  NVM3 Storage (24 KB)  │ ← Network data
└────────────────────────┘ 0x0003FFFF (256 KB)
```

## Build Outputs

The build process generates TWO bootloader files:

### 1. `first_stage.s37`
- **Location**: `bootloader/tradfri-spiflash/build/autogen/first_stage.s37`
- **Address**: 0x00000000 - 0x000007FF
- **Size**: 2 KB
- **Purpose**: CPU entry point, allows main bootloader upgrades
- **Flash Once**: Rarely needs reflashing

### 2. `tradfri-bootloader-spiflash.s37`
- **Location**: `bootloader/tradfri-spiflash/build/tradfri-bootloader-spiflash.s37`
- **Address**: 0x00000800 - 0x00003FFF
- **Size**: ~14 KB
- **Purpose**: OTA update logic, SPI flash storage
- **Can Be Updated**: Via OTA or manual reflash

## Flashing Instructions

### Scenario 1: Initial Programming (New/Erased Chip)

After a full chip erase, you MUST flash BOTH stages:

```bash
# Step 1: Flash first stage bootloader
commander flash first_stage.s37 --device EFR32MG1P132F256GM32

# Step 2: Flash main bootloader
commander flash tradfri-bootloader-spiflash.s37 --device EFR32MG1P132F256GM32

# Step 3: Flash application
commander flash zigbee_bme280_sensor_tradfri.s37 --device EFR32MG1P132F256GM32

# Step 4: Verify
commander bootloader info --device EFR32MG1P132F256GM32
```

**Why Both?** After chip erase, the CPU reset vector at 0x00000000 is blank. Without the first stage, the device cannot boot.

### Scenario 2: Updating Main Bootloader Only

If you're just updating the main bootloader (first stage already present):

```bash
# Flash only the main bootloader
commander flash tradfri-bootloader-spiflash.s37 --device EFR32MG1P132F256GM32
```

### Scenario 3: Normal Firmware Updates

For regular firmware updates (bootloader already installed):

```bash
# Flash only your application
commander flash zigbee_bme280_sensor_tradfri.s37 --device EFR32MG1P132F256GM32
```

Or use OTA updates via Zigbee coordinator.

## Common Mistakes

### ❌ Mistake 1: Flashing only main bootloader after chip erase

**Symptom**: Device doesn't boot, no debug output

**Problem**: First stage (0x000-0x7FF) is blank - CPU can't find reset vector

**Solution**: Flash `first_stage.s37` first

### ❌ Mistake 2: Wrong flash order

**Symptom**: "Verification failed" or device doesn't boot

**Problem**: Flashing main bootloader before first stage

**Solution**: Always flash first_stage.s37 BEFORE tradfri-bootloader-spiflash.s37

### ❌ Mistake 3: Using wrong address ranges

**Symptom**: Bootloader overwrites application or vice versa

**Problem**: Manual address specification conflicts with S37 file addresses

**Solution**: Let commander read addresses from S37 files (don't use `--address` flag)

## Verification

After flashing both stages, verify with:

```bash
commander bootloader info --device EFR32MG1P132F256GM32
```

Expected output:
```
Bootloader version: 1.0.0
Type: Application Bootloader
Storage: SPI Flash (IS25LQ020B)
Bootloader start: 0x00000000
Bootloader size: 16384 bytes (16 KB)
Storage slots: 1
  Slot 0: 0x00000000-0x0003FFFF (256 KB)
```

## Finding the Files

### From GitHub Releases

Download the bootloader artifact from the release page. Extract the ZIP and you'll find:

```
bootloader-tradfri-<commit>/
├── bootloader/tradfri-spiflash/build/
│   ├── autogen/
│   │   └── first_stage.s37          ← First stage
│   └── tradfri-bootloader-spiflash.s37  ← Main stage
```

### From Local Build

After running `./tools/build_bootloader.sh`:

```bash
# First stage
bootloader/tradfri-spiflash/build/autogen/first_stage.s37

# Main stage
bootloader/tradfri-spiflash/build/tradfri-bootloader-spiflash.s37
```

## Advanced: Merging into Single File (Optional)

If you want a single file containing both stages:

```bash
# Method 1: Using commander (if available)
commander convert first_stage.s37 tradfri-bootloader-spiflash.s37 \
  -o bootloader-complete.s37

# Method 2: Flash both files in sequence (recommended)
commander flash first_stage.s37 tradfri-bootloader-spiflash.s37 \
  --device EFR32MG1P132F256GM32
```

## Why This Architecture?

The two-stage design protects against bricking:

1. **First stage is permanent**: Rarely modified, always at 0x000
2. **Main stage can be updated**: Via OTA without bricking risk
3. **First stage validates main stage**: Won't boot corrupted main bootloader
4. **Rollback protection**: First stage can restore working main bootloader

This is why your flash tool showed programming starting at 0x800 - it detected the first stage was already present and only updated the main stage!

## Quick Reference

| Scenario | Flash first_stage.s37? | Flash main bootloader? | Flash application? |
|----------|------------------------|------------------------|-------------------|
| New chip | ✅ Yes | ✅ Yes | ✅ Yes |
| After chip erase | ✅ Yes | ✅ Yes | ✅ Yes |
| Update bootloader | ❌ No (usually) | ✅ Yes | ❌ No |
| Update firmware | ❌ No | ❌ No | ✅ Yes |
| Factory reset | ✅ Yes | ✅ Yes | ✅ Yes |

## Troubleshooting

**Q: Device doesn't boot after flashing**
- A: Did you flash first_stage.s37? It's required after chip erase.

**Q: Flash tool says "Verification failed"**
- A: Flash first_stage.s37 first, then main bootloader.

**Q: Where is first_stage.s37 in the release?**
- A: It's in the `bootloader/tradfri-spiflash/build/autogen/` folder in the artifact ZIP.

**Q: Can I skip first_stage.s37?**
- A: Only if it's already present from a previous flash. After chip erase, you MUST flash it.

**Q: Do I need to reflash first_stage.s37 when updating firmware?**
- A: No, only when the chip is fully erased or you're doing a factory reset.
