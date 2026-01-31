#!/bin/bash
# Build script for EFR32MG1P BME280 Zigbee Sensor
# This script generates the project using SLC CLI and builds with GNU Arm GCC

set -e  # Exit on error

# Default values for TRÅDFRI module
BOARD="${BOARD:-custom}"
SLCP_FILE="${SLCP_FILE:-zigbee_bme280_sensor_tradfri.slcp}"
PROJECT_NAME="zigbee_bme280_sensor_tradfri"

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
eval $SLC_CMD

# Find the generated Makefile (SLC may use sample name instead of our project name)
MAKEFILE=$(find "$FIRMWARE_DIR" -maxdepth 1 -name "*.Makefile" 2>/dev/null | head -1)

if [ -z "$MAKEFILE" ] || [ ! -f "$MAKEFILE" ]; then
  echo -e "${RED}Error: Project generation failed - no Makefile found${NC}"
  exit 1
fi

MAKEFILE_NAME=$(basename "$MAKEFILE")
echo -e "${GREEN}✓${NC} Project generated successfully: $MAKEFILE_NAME"

# Copy our custom source files
echo ""
echo -e "${GREEN}Copying custom source files...${NC}"

# Create source directory structure in firmware
mkdir -p "$FIRMWARE_DIR/src/app"
mkdir -p "$FIRMWARE_DIR/src/drivers/bme280"
mkdir -p "$FIRMWARE_DIR/include"

# Copy files
cp "$PROJECT_ROOT/src/app/"*.c "$FIRMWARE_DIR/src/app/" 2>/dev/null || true
cp "$PROJECT_ROOT/src/app/"*.h "$FIRMWARE_DIR/src/app/" 2>/dev/null || true
cp "$PROJECT_ROOT/src/drivers/"*.c "$FIRMWARE_DIR/src/drivers/" 2>/dev/null || true
cp "$PROJECT_ROOT/src/drivers/"*.h "$FIRMWARE_DIR/src/drivers/" 2>/dev/null || true
cp "$PROJECT_ROOT/src/drivers/bme280/"*.c "$FIRMWARE_DIR/src/drivers/bme280/" 2>/dev/null || true
cp "$PROJECT_ROOT/src/drivers/bme280/"*.h "$FIRMWARE_DIR/src/drivers/bme280/" 2>/dev/null || true
cp "$PROJECT_ROOT/include/"*.h "$FIRMWARE_DIR/include/" 2>/dev/null || true

echo -e "${GREEN}✓${NC} Custom source files copied"

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
# Build release configuration which has size optimizations
# Ensure GPIO/SPIDRV macros are visible during compilation to avoid config warnings.
EXTRA_CPPFLAGS=(
  "-DSL_SIMPLE_BUTTON_BTN0_PORT=gpioPortB"
  "-DSL_SIMPLE_BUTTON_BTN0_PIN=13"
  "-DSL_SIMPLE_LED_LED0_PORT=gpioPortA"
  "-DSL_SIMPLE_LED_LED0_PIN=0"
  "-DSL_SPIDRV_EXP_PERIPHERAL=USART0"
  "-DSL_SPIDRV_EXP_TX_LOC=4"
  "-DSL_SPIDRV_EXP_RX_LOC=4"
  "-DSL_SPIDRV_EXP_CLK_LOC=4"
)
EXTRA_CPPFLAGS_STR="${EXTRA_CPPFLAGS[*]}"

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
