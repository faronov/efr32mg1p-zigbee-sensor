#!/bin/bash
# Create OTA firmware file for Zigbee updates
# Usage: ./tools/create_ota_file.sh <variant> <version>
# Example: ./tools/create_ota_file.sh tradfri 1.0.0

set -e

VARIANT=${1:-tradfri}
VERSION=${2:-1.0.0}

# Convert version to firmware version number (e.g., 1.0.0 → 0x01000000)
IFS='.' read -ra VER_PARTS <<< "$VERSION"
MAJOR=${VER_PARTS[0]:-1}
MINOR=${VER_PARTS[1]:-0}
PATCH=${VER_PARTS[2]:-0}
FW_VERSION=$(printf "0x%02X%02X%02X00" $MAJOR $MINOR $PATCH)

echo "=========================================="
echo "Creating OTA file for ${VARIANT} v${VERSION}"
echo "Firmware version: ${FW_VERSION}"
echo "=========================================="

# Paths - try multiple possible firmware locations (check release first, then debug)
if [ -f "firmware/build/release/zigbee_bme280_sensor_${VARIANT}.s37" ]; then
    FIRMWARE_DIR="firmware/build/release"
elif [ -f "build/${VARIANT}/release/zigbee_bme280_sensor_${VARIANT}.s37" ]; then
    FIRMWARE_DIR="build/${VARIANT}/release"
elif [ -f "firmware/build/debug/zigbee_bme280_sensor_${VARIANT}.s37" ]; then
    FIRMWARE_DIR="firmware/build/debug"
elif [ -f "build/${VARIANT}/debug/zigbee_bme280_sensor_${VARIANT}.s37" ]; then
    FIRMWARE_DIR="build/${VARIANT}/debug"
else
    FIRMWARE_DIR="firmware/build/release"  # Default to release build
fi

OUTPUT_DIR="build/${VARIANT}/ota"
S37_FILE="${FIRMWARE_DIR}/zigbee_bme280_sensor_${VARIANT}.s37"
GBL_FILE="${OUTPUT_DIR}/zigbee_bme280_sensor_${VARIANT}-${VERSION}.gbl"
OTA_FILE="${OUTPUT_DIR}/zigbee_bme280_sensor_${VARIANT}-${VERSION}.ota"
ZIGBEE_FILE="${OUTPUT_DIR}/zigbee_bme280_sensor_${VARIANT}-${VERSION}.zigbee"

# Create output directory
mkdir -p "${OUTPUT_DIR}"

# Check if firmware exists
if [ ! -f "${S37_FILE}" ]; then
    echo "Error: Firmware file not found: ${S37_FILE}"
    echo "Please build the firmware first using: bash tools/build.sh ${VARIANT}"
    exit 1
fi

echo ""
echo "Step 1: Creating GBL file from S37..."
echo "Input:  ${S37_FILE}"
echo "Output: ${GBL_FILE}"

# Create GBL file using commander
# GBL is Silicon Labs Gecko Bootloader format
# Note: Metadata is optional and can cause issues with some commander versions
commander gbl create "${GBL_FILE}" \
    --app "${S37_FILE}" \
    --device EFR32MG1P132F256GM32

if [ $? -eq 0 ]; then
    echo "✓ GBL file created successfully"
    ls -lh "${GBL_FILE}"
else
    echo "✗ Failed to create GBL file"
    exit 1
fi

echo ""
echo "Step 2: Creating Zigbee OTA file from GBL..."
echo "Input:  ${GBL_FILE}"
echo "Output: ${OTA_FILE}"

# Create proper Zigbee OTA file with ZCL header using Simplicity Commander
# This wraps the GBL in a Zigbee OTA format with manufacturer ID, version, etc.
# Using Silicon Labs manufacturer ID (0x1049) since this is DIY firmware for EFR32 chip
commander ota create \
    --upgrade-image "${GBL_FILE}" \
    --manufacturer-id 0x1049 \
    --image-type 0x0000 \
    --firmware-version ${FW_VERSION} \
    --string "BME280 Sensor v${VERSION}" \
    -o "${OTA_FILE}"

if [ $? -eq 0 ]; then
    echo "✓ Zigbee OTA file created successfully"
    ls -lh "${OTA_FILE}"

    # Show first few bytes to verify OTA header (should start with 0x0BEEF11E)
    echo ""
    echo "OTA file header (first 32 bytes):"
    hexdump -C "${OTA_FILE}" | head -3
else
    echo "✗ Failed to create OTA file"
    exit 1
fi

echo ""
echo "Step 3: Creating .zigbee file (copy of .ota)..."
cp "${OTA_FILE}" "${ZIGBEE_FILE}"
echo "✓ Created ${ZIGBEE_FILE}"
ls -lh "${ZIGBEE_FILE}"

echo ""
echo "=========================================="
echo "OTA Files Summary:"
echo "=========================================="
echo "GBL file:    ${GBL_FILE}"
echo "OTA file:    ${OTA_FILE}"
echo "Zigbee file: ${ZIGBEE_FILE}"
echo ""
echo "File sizes:"
find "${OUTPUT_DIR}" -type f \( -name "*.gbl" -o -name "*.ota" -o -name "*.zigbee" \) -exec ls -lh {} \;
echo ""
echo "=========================================="
echo "Upload Instructions:"
echo "=========================================="
echo "For Zigbee2MQTT:"
echo "  1. Copy ${ZIGBEE_FILE} to Zigbee2MQTT data directory"
echo "  2. Go to Zigbee2MQTT web UI → Device → OTA Updates"
echo "  3. Select the .zigbee file and start update"
echo ""
echo "For ZHA (Home Assistant):"
echo "  1. Copy ${OTA_FILE} to /config/ota directory"
echo "  2. Go to ZHA → Device → Reconfigure → Check for updates"
echo ""
echo "For deCONZ:"
echo "  1. Copy ${OTA_FILE} to deCONZ otau directory"
echo "  2. Trigger OTA update from deCONZ UI"
echo "=========================================="
