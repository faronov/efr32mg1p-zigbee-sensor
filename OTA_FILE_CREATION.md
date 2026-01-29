# OTA File Creation Guide

## Overview

This guide explains how to create Zigbee OTA firmware files (`.gbl`, `.ota`, `.zigbee`) from the compiled firmware for over-the-air updates.

## File Formats

### 1. GBL (Gecko Bootloader) - `.gbl`
- **Format**: Silicon Labs proprietary format
- **Purpose**: Bootloader-compatible firmware image
- **Contains**: Application code, metadata, CRC checks
- **Created by**: `commander gbl create`

### 2. Zigbee OTA - `.ota` or `.zigbee`
- **Format**: Standard Zigbee Cluster Library (ZCL) OTA format
- **Purpose**: Zigbee-compliant OTA update file
- **Contains**: GBL wrapped with Zigbee OTA header
- **Zigbee Header includes**:
  - Manufacturer ID (0x10F2 for IKEA TRÅDFRI compatibility)
  - Image Type (0x0000 for application)
  - Firmware Version (e.g., 0x01000000 for v1.0.0)
  - Header String (human-readable description)
  - Image Size, CRC, etc.

**Note**: `.ota` and `.zigbee` are the same format, just different extensions. Some coordinators prefer `.zigbee`.

## Quick Start

### Create OTA files for TRÅDFRI variant:

```bash
# Build firmware first
bash tools/build.sh tradfri

# Create OTA files (version 1.0.0)
bash tools/create_ota_file.sh tradfri 1.0.0
```

**Output files** (in `build/tradfri/ota/`):
- `zigbee_bme280_sensor_tradfri-1.0.0.gbl` - Bootloader format
- `zigbee_bme280_sensor_tradfri-1.0.0.ota` - Zigbee OTA format
- `zigbee_bme280_sensor_tradfri-1.0.0.zigbee` - Same as .ota (for compatibility)

## Detailed Process

### Step 1: Build Firmware

```bash
# Build TRÅDFRI variant
bash tools/build.sh tradfri

# Or build BRD4151A variant
bash tools/build.sh brd4151a
```

This creates: `build/<variant>/release/zigbee_bme280_sensor_<variant>.s37`

### Step 2: Create GBL File

```bash
commander gbl create output.gbl \
    --app firmware.s37 \
    --device EFR32MG1P132F256GM32 \
    --metadata 0x02010000:0x01000000
```

**Metadata format**: `0x02010000:<version>`
- `0x02010000`: Metadata tag ID (firmware version)
- `<version>`: 4-byte version (e.g., `0x01000000` = v1.0.0)

**Version encoding**:
```
Version 1.2.3 → 0x01020300
├─ Major: 0x01 (1)
├─ Minor: 0x02 (2)
├─ Patch: 0x03 (3)
└─ Build: 0x00 (0)
```

### Step 3: Create Zigbee OTA File

```bash
commander gbl parse input.gbl \
    --ota output.ota \
    --manufacturer 0x10F2 \
    --imagetype 0x0000 \
    --version 0x01000000 \
    --device EFR32MG1P132F256GM32 \
    --string "BME280 Sensor v1.0.0"
```

**Parameters**:
- `--manufacturer`: Zigbee manufacturer code
  - `0x10F2`: IKEA TRÅDFRI (for compatibility)
  - `0x1049`: Silicon Labs
  - Custom: Register your own with Zigbee Alliance
- `--imagetype`: Image type identifier
  - `0x0000`: Application firmware (standard)
  - Custom values: 0x0001-0xFFFF
- `--version`: Must match GBL metadata version
- `--string`: Human-readable description (max 32 chars)

### Step 4: Copy to `.zigbee` Extension

```bash
cp output.ota output.zigbee
```

Some Zigbee coordinators (Zigbee2MQTT, deCONZ) prefer `.zigbee` extension.

## Manual Usage

### Using the script:

```bash
# Syntax
bash tools/create_ota_file.sh <variant> <version>

# Examples
bash tools/create_ota_file.sh tradfri 1.0.0
bash tools/create_ota_file.sh brd4151a 2.3.1
```

### Using commander directly:

```bash
# Step 1: Create GBL
commander gbl create build/tradfri/ota/firmware.gbl \
    --app build/tradfri/release/zigbee_bme280_sensor_tradfri.s37 \
    --device EFR32MG1P132F256GM32 \
    --metadata 0x02010000:0x01000000

# Step 2: Create OTA/Zigbee file
commander gbl parse build/tradfri/ota/firmware.gbl \
    --ota build/tradfri/ota/firmware.ota \
    --manufacturer 0x10F2 \
    --imagetype 0x0000 \
    --version 0x01000000 \
    --device EFR32MG1P132F256GM32 \
    --string "BME280 Sensor v1.0.0"

# Step 3: Copy to .zigbee
cp build/tradfri/ota/firmware.ota build/tradfri/ota/firmware.zigbee
```

## Version Numbering

### Recommended Versioning Scheme

**Format**: `MAJOR.MINOR.PATCH`

**Examples**:
- `1.0.0` - Initial release
- `1.0.1` - Bug fix (patch)
- `1.1.0` - New feature (minor)
- `2.0.0` - Breaking change (major)

**Encoding**:
```
Version → Hex
1.0.0   → 0x01000000
1.2.3   → 0x01020300
2.15.7  → 0x020F0700
```

**Important**: Zigbee OTA updates only install firmware with **higher version numbers**. Always increment version for new releases.

## Testing OTA Files

### 1. Verify GBL File

```bash
# Parse and inspect GBL contents
commander gbl parse firmware.gbl --verify
```

### 2. Verify OTA File

```bash
# Check OTA header
commander gbl parse firmware.gbl --ota firmware.ota --verify

# Or use hexdump to inspect header
hexdump -C firmware.ota | head -20
```

**Expected OTA header** (first 69 bytes):
```
Offset  Description                 Example
------  --------------------------  -------
0x0000  File Identifier             0x0BEEF11E (little-endian)
0x0004  Header Version              0x0100
0x0006  Header Length               0x0038 (56 bytes)
0x0008  Header Field Control        0x0000
0x000A  Manufacturer Code           0x10F2 (IKEA)
0x000C  Image Type                  0x0000
0x000E  File Version                0x01000000
0x0012  Zigbee Stack Version        0x0002
0x0014  Header String               "BME280 Sensor v1.0.0\0"
0x0034  Total Image Size            (4 bytes)
...     (rest of image data)
```

### 3. Test with Coordinator

**Zigbee2MQTT**:
1. Copy `.zigbee` file to `data` directory
2. Web UI → Devices → Select device → OTA Updates
3. Check for updates

**ZHA (Home Assistant)**:
1. Copy `.ota` file to `/config/ota/`
2. ZHA → Device → Reconfigure → Check for updates

**deCONZ**:
1. Copy `.ota` file to `otau` directory
2. Trigger update from deCONZ UI

## Troubleshooting

### "commander: command not found"

Install Simplicity Commander:
```bash
# Download from Silicon Labs website
# https://www.silabs.com/developers/mcu-programming-options

# Or use Docker container with commander pre-installed
docker run -v $(pwd):/workspace \
    ghcr.io/<your-container> \
    commander gbl create ...
```

### "File version too old" error

The device rejects firmware with version ≤ current version. Always increment version:
```bash
# If current firmware is v1.0.0, use v1.0.1 or higher
bash tools/create_ota_file.sh tradfri 1.0.1
```

### "Image verification failed"

Check that:
1. GBL was created with correct device target (EFR32MG1P132F256GM32)
2. Application includes bootloader interface component
3. Firmware was built for the correct hardware variant

### "OTA file not detected by coordinator"

Try these steps:
1. Use `.zigbee` extension instead of `.ota`
2. Check manufacturer code matches device profile
3. Verify OTA header with `hexdump -C firmware.ota | head`
4. Restart coordinator after copying file

## CI/CD Integration

### GitHub Actions

Add OTA file creation to workflow:

```yaml
- name: Create OTA files
  run: |
    bash tools/create_ota_file.sh tradfri ${{ github.ref_name }}
    bash tools/create_ota_file.sh brd4151a ${{ github.ref_name }}

- name: Upload OTA artifacts
  uses: actions/upload-artifact@v4
  with:
    name: ota-files-${{ github.sha }}
    path: |
      build/*/ota/*.gbl
      build/*/ota/*.ota
      build/*/ota/*.zigbee
```

### Automated Releases

```bash
# Create release with OTA files
git tag v1.0.0
git push origin v1.0.0

# Build and create OTA
bash tools/build.sh tradfri
bash tools/create_ota_file.sh tradfri 1.0.0

# Create GitHub release with OTA files
gh release create v1.0.0 \
    build/tradfri/ota/*.gbl \
    build/tradfri/ota/*.ota \
    build/tradfri/ota/*.zigbee \
    --title "v1.0.0 - BME280 Sensor" \
    --notes "Zigbee OTA update files"
```

## Security Considerations

### Signed Firmware (Production)

For production deployments, enable secure boot:

1. **Generate signing key**:
```bash
commander util genkey --type ecc-p256 --outfile signing-key.pem
```

2. **Create signed GBL**:
```bash
commander gbl create firmware.gbl \
    --app firmware.s37 \
    --device EFR32MG1P132F256GM32 \
    --sign signing-key.pem
```

3. **Configure bootloader**: Enable `BOOTLOADER_ENFORCE_SIGNED_UPGRADE` in SLCP

### Encrypted Firmware (Optional)

For additional security, encrypt GBL:
```bash
commander gbl create firmware.gbl \
    --app firmware.s37 \
    --device EFR32MG1P132F256GM32 \
    --encrypt encryption-key.txt
```

## References

- [Zigbee OTA Cluster Specification (ZCL 11)](https://zigbeealliance.org/wp-content/uploads/2019/11/docs-07-5123-05-zigbee-cluster-library-specification.pdf)
- [Silicon Labs UG266: Gecko Bootloader File Format](https://www.silabs.com/documents/public/user-guides/ug266-gecko-bootloader-api-reference.pdf)
- [Silicon Labs UG162: Simplicity Commander Reference](https://www.silabs.com/documents/public/user-guides/ug162-simplicity-commander-reference-guide.pdf)
- [Zigbee2MQTT OTA Updates](https://www.zigbee2mqtt.io/guide/usage/ota_updates.html)

## Summary

**Quick workflow**:
```bash
# 1. Build firmware
bash tools/build.sh tradfri

# 2. Create OTA files
bash tools/create_ota_file.sh tradfri 1.0.0

# 3. Upload to coordinator
cp build/tradfri/ota/*.zigbee <zigbee2mqtt-data-dir>/

# 4. Trigger OTA update from coordinator UI
```

**File extensions**:
- `.gbl` - Silicon Labs Gecko Bootloader format
- `.ota` - Zigbee OTA format (standard)
- `.zigbee` - Same as `.ota` (coordinator preference)

**Remember**: Always increment version number for successful OTA updates!
