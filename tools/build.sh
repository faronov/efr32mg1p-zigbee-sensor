#!/bin/bash
# Build script for EFR32MG1P BME280 Zigbee Sensor
# This script generates the project using SLC CLI and builds with GNU Arm GCC

set -e  # Exit on error
set -o pipefail  # Preserve failures in pipelines

# Default values for TRÅDFRI module
BOARD="${BOARD:-custom}"
SLCP_FILE="${SLCP_FILE:-zigbee_bme280_sensor_tradfri.slcp}"

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}TRÅDFRI BME280 Zigbee Sensor Builder${NC}"
echo -e "${GREEN}========================================${NC}"

# Check required environment variables
if [ -z "$GSDK_DIR" ]; then
  echo -e "${RED}Error: GSDK_DIR environment variable not set${NC}"
  echo "Please set GSDK_DIR to the path of your Gecko SDK installation"
  echo "Example: export GSDK_DIR=/path/to/gecko_sdk"
  exit 1
fi

if [ ! -d "$GSDK_DIR" ]; then
  echo -e "${RED}Error: GSDK_DIR does not exist: $GSDK_DIR${NC}"
  exit 1
fi

# Get script directory and project root (needed for SLCP path)
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$( cd "$SCRIPT_DIR/.." && pwd )"
FIRMWARE_DIR="$PROJECT_ROOT/firmware"
SOURCE_ZCL_DIR="$PROJECT_ROOT/config/zcl"
SLC_ARGS_SOURCE="$SOURCE_ZCL_DIR/slc_args.json"
TEMP_SLC_ARGS_CREATED=0

cleanup_generated_files() {
  if [ "$TEMP_SLC_ARGS_CREATED" -eq 1 ] && [ -f "$SLC_ARGS_SOURCE" ]; then
    rm -f "$SLC_ARGS_SOURCE"
  fi
}

trap cleanup_generated_files EXIT

# Use TRÅDFRI SLCP if not provided
if [ -z "$SAMPLE_SLCP" ]; then
  SAMPLE_SLCP="$PROJECT_ROOT/$SLCP_FILE"

  if [ ! -f "$SAMPLE_SLCP" ]; then
    echo -e "${RED}Error: TRÅDFRI SLCP not found: $SAMPLE_SLCP${NC}"
    exit 1
  fi

  echo -e "${GREEN}Using TRÅDFRI project: $SAMPLE_SLCP${NC}"
else
  echo -e "${YELLOW}Using provided SLCP: $SAMPLE_SLCP${NC}"
fi

# Name the project based on the SLCP file unless explicitly overridden.
if [ -z "$PROJECT_NAME" ]; then
  PROJECT_NAME="$(basename "$SAMPLE_SLCP" .slcp)"
fi

# Check for required tools
echo ""
echo -e "${GREEN}Checking prerequisites...${NC}"

if ! command -v slc &> /dev/null; then
  echo -e "${RED}Error: 'slc' command not found${NC}"
  echo "Please install Silicon Labs SLC CLI and add it to your PATH"
  echo "Download from: https://www.silabs.com/developers/simplicity-studio"
  exit 1
fi
echo -e "${GREEN}✓${NC} slc found: $(which slc)"

if ! command -v arm-none-eabi-gcc &> /dev/null; then
  echo -e "${RED}Error: 'arm-none-eabi-gcc' not found${NC}"
  echo "Please install GNU Arm Embedded Toolchain and add it to your PATH"
  echo "Download from: https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm"
  exit 1
fi
echo -e "${GREEN}✓${NC} arm-none-eabi-gcc found: $(which arm-none-eabi-gcc)"

GCC_VERSION=$(arm-none-eabi-gcc --version | head -n1)
echo "  Version: $GCC_VERSION"

echo ""
echo -e "${GREEN}Configuration:${NC}"
echo "  Project Root: $PROJECT_ROOT"
echo "  GSDK: $GSDK_DIR"
echo "  Sample SLCP: $SAMPLE_SLCP"
echo "  Board: $BOARD"
echo "  Firmware Output: $FIRMWARE_DIR"

# Prepare slc_args.json so APACK/ZAP uses custom Zigbee ZCL data on first generation pass.
CUSTOM_ZCL_SRC="$SOURCE_ZCL_DIR/zcl-zap-custom.json"
CUSTOM_XML_SRC="$SOURCE_ZCL_DIR/openbme280-extensions.xml"
if [ -d "$SOURCE_ZCL_DIR" ] && [ -f "$CUSTOM_ZCL_SRC" ] && [ -f "$CUSTOM_XML_SRC" ]; then
  echo ""
  echo -e "${GREEN}Preparing source slc_args.json for custom ZCL...${NC}"
  cat > "$SLC_ARGS_SOURCE" <<EOF
{
  "sdkRoot": "$GSDK_DIR",
  "apackRoot": "",
  "partOpn": "EFR32MG1P132F256GM32",
  "boards": [
    "$BOARD"
  ],
  "zcl": {
    "zigbeeZclJsonFile": "./zcl-zap-custom.json",
    "zigbeeTemplateJsonFile": "$GSDK_DIR/protocol/zigbee/app/framework/gen-template/gen-templates.json",
    "matterZclJsonFile": "$GSDK_DIR/extension/matter_extension/src/app/zap-templates/zcl/zcl.json",
    "matterTemplateJsonFile": "$GSDK_DIR/extension/matter_extension/src/app/zap-templates/app-templates.json"
  }
}
EOF
  TEMP_SLC_ARGS_CREATED=1
fi

# Configure SLC CLI
echo ""
echo -e "${GREEN}Configuring SLC CLI...${NC}"

# Clear any existing SLC configuration to avoid stale paths
rm -rf ~/.slc 2>/dev/null || true

GCC_TOOLCHAIN_DIR="$(dirname $(which arm-none-eabi-gcc))/.."

# Configure SLC with current GCC location
if slc configuration --sdk="$GSDK_DIR" --gcc-toolchain="$GCC_TOOLCHAIN_DIR" 2>&1; then
  echo -e "${GREEN}✓${NC} SLC configuration successful"
else
  echo -e "${YELLOW}Warning: SLC configuration failed, but continuing anyway${NC}"
  echo "Will attempt project generation without explicit configuration"
fi

# Verify SLC configuration
echo "SLC configuration:"
slc configuration | grep -E "(sdk|gcc)" || true

# Trust the SDK (required for SLC CLI signature verification)
echo ""
echo -e "${GREEN}Trusting GSDK...${NC}"
if slc signature trust --sdk="$GSDK_DIR" 2>&1; then
  echo -e "${GREEN}✓${NC} SDK trusted successfully"
else
  echo -e "${YELLOW}Warning: SDK trust command failed, but continuing anyway${NC}"
fi

# Clean old firmware directory if it exists
if [ -d "$FIRMWARE_DIR" ]; then
  echo -e "${YELLOW}Cleaning old firmware directory...${NC}"
  rm -rf "$FIRMWARE_DIR"
fi

# Generate project
echo ""
echo -e "${GREEN}Generating project with SLC...${NC}"
cd "$PROJECT_ROOT"

# Build the SLC generate command
SLC_CMD="slc generate \"$SAMPLE_SLCP\" -np -d firmware -name \"$PROJECT_NAME\" -o makefile --configuration release"

# Handle TRÅDFRI device specification
# Use --with to specify device OPN directly (same approach as NabuCasa)
SLC_CMD="$SLC_CMD --with EFR32MG1P132F256GM32"
echo "Using TRÅDFRI device OPN: EFR32MG1P132F256GM32"

echo "Running: $SLC_CMD"
SLC_LOG="$(mktemp)"
if ! eval $SLC_CMD 2>&1 | tee "$SLC_LOG"; then
  if grep -q "Follow-up generation did not complete within" "$SLC_LOG"; then
    echo -e "${YELLOW}SLC follow-up generation timed out; retrying once...${NC}"
    eval $SLC_CMD
  else
    rm -f "$SLC_LOG"
    exit 1
  fi
fi
rm -f "$SLC_LOG"

# Find the generated Makefile (SLC may use sample name instead of our project name)
MAKEFILE=$(find "$FIRMWARE_DIR" -maxdepth 1 -name "*.Makefile" 2>/dev/null | head -1)

if [ -z "$MAKEFILE" ] || [ ! -f "$MAKEFILE" ]; then
  echo -e "${RED}Error: Project generation failed - no Makefile found${NC}"
  exit 1
fi

MAKEFILE_NAME=$(basename "$MAKEFILE")
echo -e "${GREEN}✓${NC} Project generated successfully: $MAKEFILE_NAME"

# Copy custom ZCL data/extension files into generated project and patch
# generated slc_args.json for custom Zigbee ZCL metadata.
ZCL_DIR="$FIRMWARE_DIR/config/zcl"
if [ -d "$ZCL_DIR" ] && [ -f "$CUSTOM_ZCL_SRC" ] && [ -f "$CUSTOM_XML_SRC" ]; then
  echo ""
  echo -e "${GREEN}Installing custom ZCL data into generated project...${NC}"
  cp "$CUSTOM_ZCL_SRC" "$ZCL_DIR/"
  cp "$CUSTOM_XML_SRC" "$ZCL_DIR/"

  GENERATED_SLC_ARGS="$ZCL_DIR/slc_args.json"
  if [ -f "$GENERATED_SLC_ARGS" ]; then
    echo -e "${GREEN}Patching generated slc_args.json with custom ZCL metadata...${NC}"
    cat > "$GENERATED_SLC_ARGS" <<EOF
{
  "sdkRoot": "$GSDK_DIR",
  "apackRoot": "",
  "partOpn": "EFR32MG1P132F256GM32",
  "boards": [
    "$BOARD"
  ],
  "zcl": {
    "zigbeeZclJsonFile": "./zcl-zap-custom.json",
    "zigbeeTemplateJsonFile": "$GSDK_DIR/protocol/zigbee/app/framework/gen-template/gen-templates.json",
    "matterZclJsonFile": "$GSDK_DIR/extension/matter_extension/src/app/zap-templates/zcl/zcl.json",
    "matterTemplateJsonFile": "$GSDK_DIR/extension/matter_extension/src/app/zap-templates/app-templates.json"
  }
}
EOF
  fi

  echo -e "${GREEN}Re-running SLC generation after slc_args patch...${NC}"
  SLC_REGEN_CMD="slc generate \"$FIRMWARE_DIR/$PROJECT_NAME.slcp\" -np -d \"$FIRMWARE_DIR\" -name \"$PROJECT_NAME\" -o makefile --configuration release --with EFR32MG1P132F256GM32"
  echo "Running: $SLC_REGEN_CMD"
  eval $SLC_REGEN_CMD

  MAKEFILE=$(find "$FIRMWARE_DIR" -maxdepth 1 -name "*.Makefile" 2>/dev/null | head -1)
  if [ -z "$MAKEFILE" ] || [ ! -f "$MAKEFILE" ]; then
    echo -e "${RED}Error: Regeneration failed - no Makefile found${NC}"
    exit 1
  fi
  MAKEFILE_NAME=$(basename "$MAKEFILE")
  echo -e "${GREEN}✓${NC} Regenerated project successfully: $MAKEFILE_NAME"

  echo -e "${GREEN}Forcing ZAP generation with custom Zigbee ZCL metadata...${NC}"
  ZAP_CLI_BIN="$(command -v zap-cli || true)"
  if [ -z "$ZAP_CLI_BIN" ] && [ -x "/root/.silabs/slt/installs/archive/zap/zap-cli" ]; then
    ZAP_CLI_BIN="/root/.silabs/slt/installs/archive/zap/zap-cli"
  fi
  if [ -z "$ZAP_CLI_BIN" ]; then
    echo -e "${RED}Error: zap-cli not found; cannot force custom ZCL generation${NC}"
    exit 1
  fi

  ZAP_INPUT_FILE="$(find "$ZCL_DIR" -maxdepth 1 -type f -name '*.zap' | head -1)"
  if [ -z "$ZAP_INPUT_FILE" ] || [ ! -f "$ZAP_INPUT_FILE" ]; then
    echo -e "${RED}Error: No .zap input file found in $ZCL_DIR${NC}"
    exit 1
  fi

  ZAP_OUTPUT_DIR="$FIRMWARE_DIR/autogen"
  mkdir -p "$ZAP_OUTPUT_DIR"
  echo "Using zap-cli: $ZAP_CLI_BIN"
  echo "ZAP input: $ZAP_INPUT_FILE"
  "$ZAP_CLI_BIN" generate \
    --tempState \
    --skipPostGeneration \
    -z "$ZCL_DIR/zcl-zap-custom.json" \
    -g "$GSDK_DIR/protocol/zigbee/app/framework/gen-template/gen-templates.json" \
    -o "$ZAP_OUTPUT_DIR" \
    "$ZAP_INPUT_FILE"
  echo -e "${GREEN}✓${NC} Custom ZAP generation completed"
fi

# Copy our custom source files
echo ""
echo -e "${GREEN}Copying custom source files...${NC}"

# Create source directory structure in firmware
mkdir -p "$FIRMWARE_DIR/src/app"
mkdir -p "$FIRMWARE_DIR/src/drivers/bme280"
mkdir -p "$FIRMWARE_DIR/src/drivers/hal"
mkdir -p "$FIRMWARE_DIR/include"

# Copy files
cp "$PROJECT_ROOT/src/app/"*.c "$FIRMWARE_DIR/src/app/" 2>/dev/null || true
cp "$PROJECT_ROOT/src/app/"*.h "$FIRMWARE_DIR/src/app/" 2>/dev/null || true
cp "$PROJECT_ROOT/src/drivers/"*.c "$FIRMWARE_DIR/src/drivers/" 2>/dev/null || true
cp "$PROJECT_ROOT/src/drivers/"*.h "$FIRMWARE_DIR/src/drivers/" 2>/dev/null || true
cp "$PROJECT_ROOT/src/drivers/bme280/"*.c "$FIRMWARE_DIR/src/drivers/bme280/" 2>/dev/null || true
cp "$PROJECT_ROOT/src/drivers/bme280/"*.h "$FIRMWARE_DIR/src/drivers/bme280/" 2>/dev/null || true
cp -R "$PROJECT_ROOT/include/"* "$FIRMWARE_DIR/include/" 2>/dev/null || true
# Some generated builds include app_profile.h from project root.
cp "$PROJECT_ROOT/include/"*.h "$FIRMWARE_DIR/" 2>/dev/null || true
# Sources under src/app include "app_profile.h" without include/ prefix.
# Keep a copy adjacent to those sources so all profile builds resolve it.
cp "$PROJECT_ROOT/include/app_profile.h" "$FIRMWARE_DIR/src/app/" 2>/dev/null || true
# Drivers include these headers without include/ prefix in some generated projects.
cp "$PROJECT_ROOT/include/bme280_board_config.h" "$FIRMWARE_DIR/src/drivers/" 2>/dev/null || true
cp "$PROJECT_ROOT/include/bme280_board_config.h" "$FIRMWARE_DIR/src/drivers/bme280/" 2>/dev/null || true
cp "$PROJECT_ROOT/include/bme280_board_config_tradfri.h" "$FIRMWARE_DIR/src/drivers/" 2>/dev/null || true
cp -R "$PROJECT_ROOT/include/hal/"* "$FIRMWARE_DIR/src/drivers/hal/" 2>/dev/null || true

echo -e "${GREEN}✓${NC} Custom source files copied"

# Suppress noisy config #warning lines in generated headers.
CONFIG_DIR="$FIRMWARE_DIR/config"
if [ -d "$CONFIG_DIR" ]; then
  for cfg in sl_simple_button_btn0_config.h sl_simple_led_led0_config.h sl_spidrv_exp_config.h sl_rail_util_pti_config.h; do
    if [ -f "$CONFIG_DIR/$cfg" ]; then
      sed -i.bak 's/^#warning /\/\/ #warning /' "$CONFIG_DIR/$cfg"
    fi
  done
fi

# Build the project
echo ""
echo -e "${GREEN}Building project (release mode for size optimization)...${NC}"
cd "$FIRMWARE_DIR"

# Determine number of parallel jobs
if command -v nproc &> /dev/null; then
  JOBS=$(nproc)
elif command -v sysctl &> /dev/null; then
  JOBS=$(sysctl -n hw.ncpu)
else
  JOBS=4
fi

echo "Building with $JOBS parallel jobs..."
# Build release configuration which has size optimizations.
DEBUG_CPPFLAGS=()
if [[ "$PROJECT_NAME" == *debug* ]] || [[ "$SAMPLE_SLCP" == *debug* ]]; then
  DEBUG_CPPFLAGS+=(
    "-DAPP_DEBUG_DIAG_ALWAYS=1"
    "-DAPP_DEBUG_MAIN_HEARTBEAT=1"
    "-DAPP_DEBUG_POLL_BUTTON=1"
  )
fi
EXTRA_CPPFLAGS_STR="${DEBUG_CPPFLAGS[*]}"

make -f "$MAKEFILE_NAME" -j"$JOBS" CONF=release CPPFLAGS="$CPPFLAGS $EXTRA_CPPFLAGS_STR"

# Find and report build outputs
echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Build completed successfully!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""

echo "Build outputs:"

# Find common output files
for ext in elf hex bin s37 out; do
  FILES=$(find "$FIRMWARE_DIR" -name "*.${ext}" 2>/dev/null || true)
  if [ -n "$FILES" ]; then
    echo "  ${ext^^} files:"
    echo "$FILES" | while read -r file; do
      SIZE=$(ls -lh "$file" | awk '{print $5}')
      echo "    - $file ($SIZE)"
    done
  fi
done

# Find map files
MAP_FILES=$(find "$FIRMWARE_DIR" -name "*.map" 2>/dev/null || true)
if [ -n "$MAP_FILES" ]; then
  echo "  MAP files:"
  echo "$MAP_FILES" | while read -r file; do
    echo "    - $file"
  done
fi

echo ""
echo -e "${GREEN}Build artifacts are ready for flashing to EFR32MG1P!${NC}"
