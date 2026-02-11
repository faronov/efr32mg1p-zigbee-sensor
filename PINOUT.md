# TRÅDFRI Module Hardware Pinout

This document describes the hardware connections for the IKEA TRÅDFRI module (EFR32MG1P132F256GM32) used in this project.

## TRÅDFRI ICC-1 Module Pinout

![TRÅDFRI Pinout](docs/images/tradfri-pinout.png)

*Pinout diagram credit: [basilfx/TRADFRI-Hacking](https://github.com/basilfx/TRADFRI-Hacking)*

### Pin Layout

**Left Side (Top to Bottom):**
- PA0 - LED (Red channel)
- PA1 - LED (Green channel, unused)
- PB12 - GPIO
- PB13 - Button
- GND - Ground
- VCC - Power (3.3V from batteries)

**Right Side (Top to Bottom):**
- RESET - Hardware reset
- PF3 - SPI flash enable (not exposed on ICC-A-1)
- PF2 - SWO (Serial Wire Output for debugging)
- PF1 - SWDIO (Serial Wire Debug I/O)
- PF0 - SWCLK (Serial Wire Clock)
- PC11 - I2C SCL (BME280)
- PC10 - I2C SDA (BME280)
- PB14 - GPIO
- PB15 - LED (Blue channel, unused)
- GND - Ground

**Center (SPI Flash - IS25LQ020B 256KB):**
- PB11 - CS (Chip Select)
- PD15 - MOSI (Master Out Slave In)
- PD14 - MISO (Master In Slave Out)
- PD13 - CLK (Clock)

## GPIO Configuration

| Function | Port | Pin | Physical Pin | Notes |
|----------|------|-----|--------------|-------|
| **LED0** | A | 0 | Left side, top | RGB LED red channel (active high) |
| **Button0** | B | 13 | Left side, 4th | Reset/pair button (active low, pull-up) |
| **I2C SDA** | C | 10 | Right side, 7th | BME280 data line (requires 4.7kΩ pull-up) |
| **I2C SCL** | C | 11 | Right side, 6th | BME280 clock line (requires 4.7kΩ pull-up) |

## BME280 Sensor Connections

```
BME280 Pin      TRÅDFRI Pin      Location         Notes
──────────────────────────────────────────────────────────────
VCC (3.3V)   →  VCC              Left side, 6th   Use board 3.3V (2xAAA)
GND          →  GND              Left/Right side  Ground (multiple pins)
SDA          →  PC10             Right side, 7th  I2C data (add 4.7kΩ pull-up)
SCL          →  PC11             Right side, 6th  I2C clock (add 4.7kΩ pull-up)
SDO          →  GND              Any GND pin      I2C address 0x76 (default)
```

**IMPORTANT:**
- TRÅDFRI operates at 3.3V from 2xAAA batteries. Ensure BME280 is 3.3V compatible!
- Both I2C lines require external 4.7kΩ pull-up resistors to VCC

### Pull-up Resistors

**CRITICAL:** Both I2C lines require external pull-up resistors:
- **Value**: 4.7kΩ (recommended)
- **Range**: 2.2kΩ - 10kΩ acceptable
- **Connection**: Between I2C line and VCC (3.3V)

```
        VCC (3.3V)
         │
        ┌┴┐
        │ │ 4.7kΩ pull-up resistor
        └┬┘
         │
    ─────┴───── PC10 (SDA) to BME280

        VCC (3.3V)
         │
        ┌┴┐
        │ │ 4.7kΩ pull-up resistor
        └┬┘
         │
    ─────┴───── PC11 (SCL) to BME280
```

## LED Functionality

The TRÅDFRI board has an RGB LED with common anode:
- **PA0** controls the RED channel (used in this project)
- **Active high**: Write 1 to turn LED on, 0 to turn off
- Current limiting resistor already present on board
- Green (PA1) and Blue (PB15) channels available but not used

### LED Indication Patterns

| Pattern | Rate | Meaning |
|---------|------|---------|
| Off | - | Default state (idle, power saving) |
| Slow blink | 500ms | Network joining in progress |
| Solid on | 30 seconds | Successfully joined (auto-off after 30s) |
| Brief flash | 200ms | Sensor reading triggered |
| Rapid flash | 100ms × 5 | Long press detected (leave/rejoin) |
| Rapid flash | 100ms × 10 | Sensor initialization error |

## Button Functionality

The TRÅDFRI board has a combined reset/pairing button:
- **PB13** is connected to the physical button (left side, 4th pin)
- **Active low** with external pull-up resistor
- Button reads 0 when pressed, 1 when released
- Shared with bootloader/reset functionality

### Short Press (<2 seconds)

**When not joined to network:**
- Initiates network joining
- LED blinks at 500ms intervals during join

**When joined to network:**
- Triggers immediate sensor reading
- Forces attribute report to coordinator
- LED flashes briefly (200ms)

### Long Press (≥2 seconds)

**When joined to network:**
- Leaves current network
- Immediately attempts to rejoin
- LED flashes rapidly (5x 100ms)
- Useful for switching coordinators

**When not joined:**
- No action

## SPI Flash (for OTA Updates)

The TRÅDFRI board includes **ISSI IS25LQ020B** 256KB (2Mbit) SPI flash for OTA:
- **PD13**: SPI CLK
- **PD14**: SPI MISO
- **PD15**: SPI MOSI
- **PB11**: SPI CS (Chip Select)
- **PF3**: Flash enable (drive high to activate; on ICC-A-1 this pin is not exposed but still connected internally)

OTA functionality is fully supported. See [OTA_FILE_CREATION.md](OTA_FILE_CREATION.md) for creating update files.

## Programming Connections (SWD)

For initial firmware flashing via J-Link or similar:
- **PF0 (SWCLK)**: Serial Wire Clock (right side, 5th)
- **PF1 (SWDIO)**: Serial Wire Data I/O (right side, 4th)
- **PF2 (SWO)**: Serial Wire Output for debug logging (right side, 3rd)
- **RESET**: Hardware reset (right side, top)
- **GND**: Ground reference

Typical SWD connection:
```
Programmer    TRÅDFRI
─────────────────────
SWDCLK    →   PF0
SWDIO     →   PF1
SWO       →   PF2 (optional, for debug output)
RESET     →   RESET
GND       →   GND
VCC       →   VCC (3.3V - power target, or leave disconnected if battery powered)
```

## I2C Configuration Details

The project uses I2C0 peripheral with these settings:
- **Frequency**: 100 kHz (standard mode)
- **Location**: LOC15 (PC10=SDA, PC11=SCL)
- **Clock stretching**: Enabled
- **Bus timeout**: 3000 ticks

Configuration defined in `include/bme280_board_config_tradfri.h`:
```c
#define BME280_I2C_SDA_PORT       gpioPortC
#define BME280_I2C_SDA_PIN        10
#define BME280_I2C_SCL_PORT       gpioPortC
#define BME280_I2C_SCL_PIN        11
#define BME280_I2C_ADDR           0x76
```

## Power Supply

### Battery Configuration
- Designed for 2xAAA battery pack (included with original IKEA device)
- **Nominal voltage**: 3.0V (2 × 1.5V alkaline)
- **Operating range**: 1.8V - 3.2V
- **Low battery cutoff**: ~2.0V
- **Do not exceed 3.6V!** Risk of chip damage

### Battery Life Estimate
- Depends on `sensor_read_interval` (default 10s, configurable 10..3600s)
- Depends on coordinator reporting settings and link quality
- Debug no-sleep builds are not representative for battery estimates

See [POWER_OPTIMIZATION.md](POWER_OPTIMIZATION.md) for detailed power analysis.

## Battery Monitoring

The firmware monitors battery voltage using the internal ADC:
- **Channel**: AVDD (supply voltage measurement)
- **Resolution**: 12-bit (0-4095)
- **Reference**: 1.25V internal
- **Scale factor**: 4 (1/4 gain on AVDD input)
- **Measurement range**: 0 - 5V (suitable for 2xAAA)

The ADC reading is converted to voltage:
```c
voltage_mv = (adc_value * 1250 * 4) / 4096
```

Battery percentage reported to Zigbee Power Configuration cluster:
- 0-200 range (200 = 100%)
- Based on 3.0V nominal, 2.0V cutoff
- Updated on sensor cycle and reported per Zigbee reporting configuration

## Wiring Example

```
                            ┌─────────────┐
                            │   BME280    │
                            │   Module    │
                            └─────────────┘
                                 │ │ │ │
                                VCC│ │GND
                                 │SDA│
                                 │ │SCL
                4.7kΩ    4.7kΩ  │ │ │
                 ┌┴┐     ┌┴┐   │ │ │
VCC (3.3V) ──────┤ ├─────┤ ├───┴─┘ │
                 └┬┘     └┬┘        │
PC10 (SDA) ───────┴───────┼─────────┘
                          │
PC11 (SCL) ───────────────┘

GND ───────────────────────────────────── GND
```

## Testing

### LED Test
1. Flash firmware via SWD
2. Observe LED behavior on boot
3. Press button to test LED response

### Button Test
1. **Short press**: LED should flash briefly
2. **Long press (2s)**: LED should flash rapidly 5 times

### I2C Test
1. Power on device (2xAAA batteries)
2. Check serial output (SWO) for "BME280 sensor initialized successfully"
3. If initialization fails, check:
   - I2C wiring
   - Pull-up resistors (4.7kΩ on both SDA and SCL)
   - BME280 power (3.3V)

### BME280 Address Configuration

Default I2C address is **0x76** (SDO pin connected to GND).

To use alternate address **0x77**:
1. Connect BME280 SDO pin to 3.3V instead of GND
2. Update firmware in `include/bme280_board_config_tradfri.h`:
```c
#define BME280_I2C_ADDR  0x77  // Change to 0x77
```

## Troubleshooting

### LED not working
- Verify PA0 is configured correctly in SLCP
- Check if LED is functional (test with multimeter in diode mode)
- Ensure firmware flashed successfully

### Button not responding
- Verify PB13 is configured correctly in SLCP
- Check button with multimeter (should read 0Ω to GND when pressed)
- Ensure pull-up is enabled in firmware

### I2C communication failure
1. **Verify VCC is 3.3V** (not 5V!) - measure at BME280 VCC pin
2. **Check pull-up resistors** - should measure ~4.7kΩ between I2C line and VCC
3. **Verify correct I2C address** - 0x76 (default) or 0x77
4. **Check for loose connections** - resolder if necessary
5. **Measure voltage on I2C lines** - should be ~3.3V when idle (pulled up)
6. **Test with oscilloscope** - should see clock pulses on SCL during init

### Battery voltage reading incorrect
1. Check if batteries are fresh (>2.8V for 2xAAA)
2. Verify no voltage drop under load
3. Measure VCC at module - should match battery voltage
4. ADC measures AVDD directly - no calibration needed

### OTA update fails
1. Verify bootloader is flashed first
2. Check SPI flash connections (PD13, PD14, PD15, PB11)
3. Ensure .ota or .zigbee file format is correct
4. Version number must be higher than current firmware

## Configuration Files

### Pin Definitions
Defined in `zigbee_bme280_sensor_tradfri.slcp`:
```yaml
define:
  - name: SL_SIMPLE_LED_LED0_PORT
    value: gpioPortA
  - name: SL_SIMPLE_LED_LED0_PIN
    value: 0
  - name: SL_SIMPLE_BUTTON_BTN0_PORT
    value: gpioPortB
  - name: SL_SIMPLE_BUTTON_BTN0_PIN
    value: 13
```

### I2C Configuration
Defined in `include/bme280_board_config_tradfri.h`:
```c
#define BME280_I2C_SDA_PORT       gpioPortC
#define BME280_I2C_SDA_PIN        10
#define BME280_I2C_SCL_PORT       gpioPortC
#define BME280_I2C_SCL_PIN        11
#define BME280_I2C_ADDR           0x76
```

## References

- [EFR32MG1P Reference Manual](https://www.silabs.com/documents/public/reference-manuals/efr32xg1-rm.pdf)
- [EFR32MG1P132F256GM32 Datasheet](https://www.silabs.com/documents/public/data-sheets/efr32mg1-datasheet.pdf)
- [BME280 Datasheet](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bme280-ds002.pdf)
- [IKEA TRÅDFRI Teardown & Hacking](https://github.com/basilfx/TRADFRI-Hacking)
- [IS25LQ020B Flash Datasheet](https://www.issi.com/WW/pdf/25LQ025B-512B-010B-020B-040B.pdf)
