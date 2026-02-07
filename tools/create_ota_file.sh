#!/bin/bash
# Create Zigbee OTA firmware files for TRADFRI build outputs.
#
# Usage:
#   ./tools/create_ota_file.sh 1.2.3
#   ./tools/create_ota_file.sh 1.2.3 --variant tradfri_debug --image-type 0x0001

set -euo pipefail

VERSION="${1:-1.0.0}"
shift || true

VARIANT="tradfri"
MANUFACTURER_ID=""
IMAGE_TYPE=""
STACK_VERSION="0x0002"
DEVICE_OPN="EFR32MG1P132F256GM32"
S37_FILE=""
MANUFACTURER_ID_SOURCE=""
IMAGE_TYPE_SOURCE=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --variant)
      VARIANT="${2:?missing value for --variant}"
      shift 2
      ;;
    --manufacturer-id)
      MANUFACTURER_ID="${2:?missing value for --manufacturer-id}"
      shift 2
      ;;
    --image-type)
      IMAGE_TYPE="${2:?missing value for --image-type}"
      shift 2
      ;;
    --stack-version)
      STACK_VERSION="${2:?missing value for --stack-version}"
      shift 2
      ;;
    --s37)
      S37_FILE="${2:?missing value for --s37}"
      shift 2
      ;;
    *)
      echo "Unknown option: $1" >&2
      exit 1
      ;;
  esac
done

normalize_hex() {
  local raw="$1"
  local trimmed
  trimmed="$(echo "$raw" | tr -d '[:space:]')"
  if [[ "$trimmed" =~ ^0[xX][0-9a-fA-F]+$ ]]; then
    echo "0x${trimmed#0[xX]}"
    return 0
  fi
  if [[ "$trimmed" =~ ^[0-9]+$ ]]; then
    printf "0x%X" "$trimmed"
    return 0
  fi
  return 1
}

infer_manufacturer_id_from_zap() {
  local zap="config/zcl/zcl_config.zap"
  local raw=""
  if [[ -f "$zap" ]]; then
    raw="$(rg -No '"key"\s*:\s*"manufacturerCodes"\s*,\s*"value"\s*:\s*"([^"]+)"' "$zap" -r '$1' | head -n1 || true)"
    if [[ -n "$raw" ]]; then
      normalize_hex "$raw" || true
    fi
  fi
}

infer_image_type_for_variant() {
  # Keep stable image type per firmware family unless explicitly overridden.
  # Debug and release variants can still override with --image-type.
  if [[ "$VARIANT" == "tradfri_debug" ]]; then
    echo "0x0001"
  else
    echo "0x0000"
  fi
}

if [[ ! "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "Version must be semver (MAJOR.MINOR.PATCH), got: $VERSION" >&2
  exit 1
fi

# Convert semver to uint32 firmware version for Zigbee OTA header.
IFS='.' read -r MAJOR MINOR PATCH <<< "$VERSION"
if (( MAJOR > 255 || MINOR > 255 || PATCH > 255 )); then
  echo "Version fields must be in range 0..255, got: $VERSION" >&2
  exit 1
fi
FW_VERSION=$(printf "0x%02X%02X%02X00" "$MAJOR" "$MINOR" "$PATCH")

if [[ -n "$MANUFACTURER_ID" ]]; then
  MANUFACTURER_ID="$(normalize_hex "$MANUFACTURER_ID")"
  MANUFACTURER_ID_SOURCE="flag"
elif [[ -n "${OTA_MANUFACTURER_ID:-}" ]]; then
  MANUFACTURER_ID="$(normalize_hex "${OTA_MANUFACTURER_ID}")"
  MANUFACTURER_ID_SOURCE="env"
else
  MANUFACTURER_ID="$(infer_manufacturer_id_from_zap || true)"
  if [[ -n "$MANUFACTURER_ID" ]]; then
    MANUFACTURER_ID_SOURCE="zcl_config.zap"
  else
    MANUFACTURER_ID="0x1049"
    MANUFACTURER_ID_SOURCE="default"
  fi
fi

if [[ -n "$IMAGE_TYPE" ]]; then
  IMAGE_TYPE="$(normalize_hex "$IMAGE_TYPE")"
  IMAGE_TYPE_SOURCE="flag"
elif [[ -n "${OTA_IMAGE_TYPE:-}" ]]; then
  IMAGE_TYPE="$(normalize_hex "${OTA_IMAGE_TYPE}")"
  IMAGE_TYPE_SOURCE="env"
else
  IMAGE_TYPE="$(infer_image_type_for_variant)"
  IMAGE_TYPE_SOURCE="variant-default"
fi

if command -v commander >/dev/null 2>&1; then
  CMD="commander"
elif command -v commander-cli >/dev/null 2>&1; then
  CMD="commander-cli"
elif [[ -x "/Applications/Commander-cli.app/Contents/MacOS/commander-cli" ]]; then
  CMD="/Applications/Commander-cli.app/Contents/MacOS/commander-cli"
else
  echo "Commander CLI not found (commander/commander-cli)." >&2
  exit 1
fi

if [[ -z "$S37_FILE" ]]; then
  CANDIDATES=(
    "firmware/build/release/zigbee_bme280_sensor_${VARIANT}.s37"
    "build/${VARIANT}/release/zigbee_bme280_sensor_${VARIANT}.s37"
    "firmware/build/debug/zigbee_bme280_sensor_${VARIANT}.s37"
    "build/${VARIANT}/debug/zigbee_bme280_sensor_${VARIANT}.s37"
  )
  for candidate in "${CANDIDATES[@]}"; do
    if [[ -f "$candidate" ]]; then
      S37_FILE="$candidate"
      break
    fi
  done
fi

if [[ -z "$S37_FILE" || ! -f "$S37_FILE" ]]; then
  echo "Firmware .s37 not found for variant '${VARIANT}'." >&2
  echo "Build first, or pass explicit --s37 <path>." >&2
  exit 1
fi

OUTPUT_DIR="build/${VARIANT}/ota"
mkdir -p "$OUTPUT_DIR"

BASENAME="zigbee_bme280_sensor_${VARIANT}-${VERSION}"
GBL_FILE="${OUTPUT_DIR}/${BASENAME}.gbl"
OTA_FILE="${OUTPUT_DIR}/${BASENAME}.ota"
ZIGBEE_FILE="${OUTPUT_DIR}/${BASENAME}.zigbee"

echo "=========================================="
echo "Creating OTA package"
echo "Variant:          ${VARIANT}"
echo "Input S37:        ${S37_FILE}"
echo "Version:          ${VERSION}"
echo "Firmware version: ${FW_VERSION}"
echo "Manufacturer ID:  ${MANUFACTURER_ID} (${MANUFACTURER_ID_SOURCE})"
echo "Image type:       ${IMAGE_TYPE} (${IMAGE_TYPE_SOURCE})"
echo "Stack version:    ${STACK_VERSION}"
echo "Commander:        ${CMD}"
echo "=========================================="

echo
echo "Step 1/3: Create GBL"
"$CMD" gbl create "$GBL_FILE" \
  --app "$S37_FILE" \
  --device "$DEVICE_OPN"
ls -lh "$GBL_FILE"

echo
echo "Step 2/3: Wrap GBL into Zigbee OTA"
"$CMD" ota create \
  --type zigbee \
  --upgrade-image "$GBL_FILE" \
  --manufacturer-id "$MANUFACTURER_ID" \
  --image-type "$IMAGE_TYPE" \
  --firmware-version "$FW_VERSION" \
  --stack-version "$STACK_VERSION" \
  --string "OpenBME280 ${VERSION}" \
  -o "$OTA_FILE"
ls -lh "$OTA_FILE"

echo
echo "Step 3/3: Create .zigbee alias and validate header"
cp "$OTA_FILE" "$ZIGBEE_FILE"
ls -lh "$ZIGBEE_FILE"
"$CMD" ota parse "$OTA_FILE" >/dev/null
echo "✓ OTA header parse OK"

echo
echo "=========================================="
echo "Generated files:"
echo "  $GBL_FILE"
echo "  $OTA_FILE"
echo "  $ZIGBEE_FILE"
echo "=========================================="
