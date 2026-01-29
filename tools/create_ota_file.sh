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

# Paths - try both possible firmware locations
if [ -f "firmware/build/release/zigbee_bme280_sensor_${VARIANT}.s37" ]; then
    FIRMWARE_DIR="firmware/build/release"
elif [ -f "build/${VARIANT}/release/zigbee_bme280_sensor_${VARIANT}.s37" ]; then
    FIRMWARE_DIR="build/${VARIANT}/release"
else
    FIRMWARE_DIR="firmware/build/release"  # Default to GitHub Actions path
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
commander gbl create "${GBL_FILE}" \
    --app "${S37_FILE}" \
    --device EFR32MG1P132F256GM32 \
    --metadata 0x02010000:${FW_VERSION}

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

# Manufacturer code for custom/test devices (use 0x10F2 for IKEA TRÅDFRI compatibility)
# For production, use your allocated manufacturer code
MANUFACTURER_CODE="0x10F2"  # IKEA TRÅDFRI
IMAGE_TYPE="0x0000"          # Application image
HEADER_STRING="BME280 Sensor v${VERSION}"

# Create Zigbee OTA file using commander
commander gbl parse "${GBL_FILE}" \
    --ota "${OTA_FILE}" \
    --manufacturer "${MANUFACTURER_CODE}" \
    --imagetype "${IMAGE_TYPE}" \
    --version ${FW_VERSION} \
    --device EFR32MG1P132F256GM32 \
    --string "${HEADER_STRING}"

if [ $? -eq 0 ]; then
    echo "✓ Zigbee OTA file created successfully"
    ls -lh "${OTA_FILE}"
else
    echo "✗ Failed to create Zigbee OTA file"
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
