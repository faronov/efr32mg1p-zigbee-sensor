#!/bin/bash
# Build script for TRÅDFRI Bootloader
# Builds the custom bootloader with SPI flash storage for OTA updates

set -e  # Exit on error

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}TRÅDFRI Bootloader Builder${NC}"
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

# Get script directory and project root
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$( cd "$SCRIPT_DIR/.." && pwd )"
BOOTLOADER_DIR="$PROJECT_ROOT/bootloader/tradfri-spiflash"
BUILD_DIR="$BOOTLOADER_DIR/build"

echo ""
echo -e "${GREEN}Configuration:${NC}"
echo "  Project Root: $PROJECT_ROOT"
echo "  GSDK: $GSDK_DIR"
echo "  Bootloader Dir: $BOOTLOADER_DIR"
echo "  Build Output: $BUILD_DIR"

# Check for required tools
echo ""
echo -e "${GREEN}Checking prerequisites...${NC}"

if ! command -v slc &> /dev/null; then
  echo -e "${RED}Error: 'slc' command not found${NC}"
  echo "Please install Silicon Labs SLC CLI and add it to your PATH"
  exit 1
fi
echo -e "${GREEN}✓${NC} slc found: $(which slc)"

if ! command -v arm-none-eabi-gcc &> /dev/null; then
  echo -e "${RED}Error: 'arm-none-eabi-gcc' not found${NC}"
  echo "Please install GNU Arm Embedded Toolchain and add it to your PATH"
  exit 1
fi
echo -e "${GREEN}✓${NC} arm-none-eabi-gcc found: $(which arm-none-eabi-gcc)"

GCC_VERSION=$(arm-none-eabi-gcc --version | head -n1)
echo "  Version: $GCC_VERSION"

# Configure SLC CLI
echo ""
echo -e "${GREEN}Configuring SLC CLI...${NC}"

GCC_TOOLCHAIN_DIR="$(dirname $(which arm-none-eabi-gcc))/.."

if slc configuration --sdk="$GSDK_DIR" --gcc-toolchain="$GCC_TOOLCHAIN_DIR" 2>&1; then
  echo -e "${GREEN}✓${NC} SLC configuration successful"
else
  echo -e "${YELLOW}Warning: SLC configuration failed, but continuing anyway${NC}"
fi

# Trust the SDK
echo ""
echo -e "${GREEN}Trusting GSDK...${NC}"
if slc signature trust --sdk="$GSDK_DIR" 2>&1; then
  echo -e "${GREEN}✓${NC} SDK trusted successfully"
else
  echo -e "${YELLOW}Warning: SDK trust command failed, but continuing anyway${NC}"
fi

# Clean old build directory if it exists
if [ -d "$BUILD_DIR" ]; then
  echo -e "${YELLOW}Cleaning old build directory...${NC}"
  rm -rf "$BUILD_DIR"
fi

# Generate bootloader project
echo ""
echo -e "${GREEN}Generating bootloader project with SLC...${NC}"
cd "$BOOTLOADER_DIR"

# Build the SLC generate command
SLC_CMD="slc generate \"$BOOTLOADER_DIR/tradfri-bootloader-spiflash.slcp\" \
  -np \
  -d build \
  -name \"tradfri-bootloader-spiflash\" \
  -o makefile \
  --with EFR32MG1P132F256GM32"

echo "Running: $SLC_CMD"
eval $SLC_CMD

# Find the generated Makefile
MAKEFILE=$(find "$BUILD_DIR" -maxdepth 1 -name "*.Makefile" 2>/dev/null | head -1)

if [ -z "$MAKEFILE" ] || [ ! -f "$MAKEFILE" ]; then
  echo -e "${RED}Error: Project generation failed - no Makefile found${NC}"
  exit 1
fi

MAKEFILE_NAME=$(basename "$MAKEFILE")
echo -e "${GREEN}✓${NC} Project generated successfully: $MAKEFILE_NAME"

# Build the bootloader
echo ""
echo -e "${GREEN}Building bootloader (release mode)...${NC}"
cd "$BUILD_DIR"

# Determine number of parallel jobs
if command -v nproc &> /dev/null; then
  JOBS=$(nproc)
elif command -v sysctl &> /dev/null; then
  JOBS=$(sysctl -n hw.ncpu)
else
  JOBS=4
fi

echo "Building with $JOBS parallel jobs..."
make -f "$MAKEFILE_NAME" -j"$JOBS"

# Find and report build outputs
echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Bootloader build completed!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo "Build outputs:"

# Find output files
for ext in s37 hex elf out; do
  FILES=$(find "$BUILD_DIR" -name "*.${ext}" 2>/dev/null || true)
  if [ -n "$FILES" ]; then
    echo "  ${ext^^} files:"
    echo "$FILES" | while read -r file; do
      SIZE=$(ls -lh "$file" | awk '{print $5}')
      echo "    - $file ($SIZE)"
    done
  fi
done

# Show bootloader-specific files
echo ""
echo "Important bootloader files:"
if [ -f "$BUILD_DIR/tradfri-bootloader-spiflash.s37" ]; then
  SIZE=$(ls -lh "$BUILD_DIR/tradfri-bootloader-spiflash.s37" | awk '{print $5}')
  echo -e "  ${GREEN}✓${NC} Main bootloader: tradfri-bootloader-spiflash.s37 ($SIZE)"
fi

if [ -f "$BUILD_DIR/tradfri-bootloader-spiflash-crc.s37" ]; then
  SIZE=$(ls -lh "$BUILD_DIR/tradfri-bootloader-spiflash-crc.s37" | awk '{print $5}')
  echo -e "  ${GREEN}✓${NC} With CRC: tradfri-bootloader-spiflash-crc.s37 ($SIZE)"
fi

# Check bootloader size
if [ -f "$BUILD_DIR/tradfri-bootloader-spiflash.elf" ]; then
  SIZE_INFO=$(arm-none-eabi-size "$BUILD_DIR/tradfri-bootloader-spiflash.elf")
  echo ""
  echo "Bootloader memory usage:"
  echo "$SIZE_INFO"

  # Extract text size (flash usage)
  TEXT_SIZE=$(echo "$SIZE_INFO" | tail -1 | awk '{print $1}')
  TEXT_KB=$((TEXT_SIZE / 1024))

  if [ $TEXT_KB -gt 24 ]; then
    echo -e "${RED}Warning: Bootloader is ${TEXT_KB}KB (>24KB). May not fit!${NC}"
  else
    echo -e "${GREEN}✓${NC} Bootloader size: ${TEXT_KB}KB (fits in typical 16-24KB allocation)"
  fi
fi

echo ""
echo -e "${GREEN}Next steps:${NC}"
echo "1. Flash the bootloader (one-time):"
echo "   ${YELLOW}commander flash $BUILD_DIR/tradfri-bootloader-spiflash.s37 --device EFR32MG1P132F256GM32${NC}"
echo ""
echo "2. Verify bootloader info:"
echo "   ${YELLOW}commander bootloader info --device EFR32MG1P132F256GM32${NC}"
echo ""
echo "3. Flash your application firmware:"
echo "   ${YELLOW}commander flash firmware/build/release/zigbee_bme280_sensor_tradfri.s37 --device EFR32MG1P132F256GM32${NC}"
echo ""
echo -e "${GREEN}Bootloader is ready for flashing!${NC}"
