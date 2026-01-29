# Hardware Pinout

This document describes the hardware connections for the EFR32MG1P BME280 Zigbee Sensor project, including both standard BRD4151A and IKEA TRÅDFRI board variants.

## BRD4151A Development Board

### GPIO Configuration

| Function | Port | Pin | GPIO | Notes |
|----------|------|-----|------|-------|
| **LED0** | A | 0 | PA0 | Status LED (active high) |
| **Button0** | F | 6 | PF6 | User button (active low, pull-up enabled) |
| **I2C SDA** | C | 10 | PC10 | BME280 data line (requires 4.7kΩ pull-up) |
| **I2C SCL** | C | 11 | PC11 | BME280 clock line (requires 4.7kΩ pull-up) |

### BME280 Sensor Connections

```
BME280 Pin      EFR32MG1P Pin    Notes
─────────────────────────────────────────────────
VCC (3.3V)   →  3.3V             Power supply
GND          →  GND              Ground
SDA          →  PC10             I2C data (add 4.7kΩ to 3.3V)
SCL          →  PC11             I2C clock (add 4.7kΩ to 3.3V)
SDO          →  GND or 3.3V      I2C address select
                                 GND = 0x76 (default)
                                 3.3V = 0x77
```

### LED Connection

The LED is already present on the BRD4151A development board:
- **PA0** is connected to an onboard LED
- **Active high**: Write 1 to turn LED on, 0 to turn off
- No external components required

### Button Connection

The button is already present on the BRD4151A development board:
- **PF6** is connected to push button BTN0
- **Active low with internal pull-up**: Button reads 0 when pressed, 1 when released
- No external components required

### Battery Measurement

Uses internal ADC to measure VDD/AVDD:
- **No external connections required**
- Measures supply voltage directly via ADC0 AVDD channel
- Suitable for 2xAAA battery pack (1.8V - 3.2V range)

## IKEA TRÅDFRI Board (EFR32MG1P132F256GM32)

### GPIO Configuration

| Function | Port | Pin | Physical Pin | Notes |
|----------|------|-----|--------------|-------|
| **LED0** | A | 0 | Pin 24 | RGB LED red channel (active high) |
| **Button0** | B | 13 | Pin 42 | Reset/pair button (active low, pull-up) |
| **I2C SDA** | C | 10 | Pin 32 | BME280 data line (requires 4.7kΩ pull-up) |
| **I2C SCL** | C | 11 | Pin 34 | BME280 clock line (requires 4.7kΩ pull-up) |

### Physical Pin Mapping (32-pin QFN package)

```
                    EFR32MG1P132F256GM32
                    ┌─────────────┐
                    │             │
         PD13 (CLK) │  1      32  │ PC10 (SDA) ────┐
         PD14 (MISO)│  2      31  │ PC9            │ I2C to BME280
         PD15 (MOSI)│  3      30  │ PC8            │
         PA0 (LED)  │  4      29  │ PC7            │
         PA1        │  5      28  │ PC6            │
         PB11 (CS)  │  6      27  │ GND            │
         PB13 (BTN) │  7      26  │ AVDD           │
         3V3        │  8      25  │ PB14           │
         GND        │  9      24  │ PB15           │
                    │             │
                    │  (pins...)  │
                    │             │
         PC11 (SCL) │ 34          │ ────────────────┘
                    │             │
                    └─────────────┘
```

### TRÅDFRI LED Connection

The TRÅDFRI board has an RGB LED with common anode:
- **PA0** controls the RED channel
- LED is active high (write 1 to turn on)
- Current limiting resistor already present on board
- Green (PA1) and Blue (PB15) channels available but not used

### TRÅDFRI Button Connection

The TRÅDFRI board has a combined reset/pairing button:
- **PB13** is connected to the physical button
- Active low with external pull-up resistor
- Button reads 0 when pressed, 1 when released
- Shared with bootloader/reset functionality

### TRÅDFRI BME280 Connection

Same as BRD4151A - connect to I2C pins:

```
BME280 Pin      TRÅDFRI Pin      Physical Pin    Notes
──────────────────────────────────────────────────────────────
VCC (3.3V)   →  3V3              Pin 8           Use board 3.3V
GND          →  GND              Pin 9,27        Ground
SDA          →  PC10             Pin 32          I2C data (add 4.7kΩ)
SCL          →  PC11             Pin 34          I2C clock (add 4.7kΩ)
SDO          →  GND              Pin 27          I2C addr 0x76
```

**IMPORTANT:** TRÅDFRI board operates at 3.3V. Ensure BME280 is 3.3V compatible!

### TRÅDFRI SPI Flash (for OTA - currently disabled)

The TRÅDFRI board includes MX25R8035F 1MB SPI flash for OTA:
- **PD13**: SPI CLK (Pin 1)
- **PD14**: SPI MISO (Pin 2)
- **PD15**: SPI MOSI (Pin 3)
- **PB11**: SPI CS (Pin 6)

**Note:** OTA functionality is temporarily disabled in firmware pending pin configuration resolution.

## Detailed I2C Configuration

### I2C Timing

The project uses I2C0 peripheral with these settings:
- **Frequency**: 100 kHz (standard mode)
- **Location**: LOC15 (PC10=SDA, PC11=SCL)
- **Clock stretching**: Enabled
- **Bus timeout**: 3000 ticks

### Pull-up Resistors

**CRITICAL:** Both I2C lines require external pull-up resistors:
- **Value**: 4.7kΩ (recommended)
- **Range**: 2.2kΩ - 10kΩ acceptable
- **Connection**: Between I2C line and VCC (3.3V)

```
        3.3V
         │
        ┌┴┐
        │ │ 4.7kΩ pull-up resistor
        └┬┘
         │
    ─────┴───── PC10 (SDA) to BME280

        3.3V
         │
        ┌┴┐
        │ │ 4.7kΩ pull-up resistor
        └┬┘
         │
    ─────┴───── PC11 (SCL) to BME280
```

## Button Functionality

### Short Press (<2 seconds)

**When not joined:**
- Initiates network joining
- LED blinks at 500ms intervals during join

**When joined:**
- Triggers immediate sensor reading
- Forces attribute report
- LED flashes briefly (200ms)

### Long Press (≥2 seconds)

**When joined:**
- Leaves current network
- Immediately attempts to rejoin
- LED flashes rapidly (5x 100ms)
- Useful for switching coordinators

**When not joined:**
- No action

## LED Indication

| Pattern | Rate | Meaning |
|---------|------|---------|
| Off | - | Default state (idle, joined, or not joined) |
| Slow blink | 500ms | Network joining in progress |
| Solid on | 3 seconds | Successfully joined (auto-off after 3s) |
| Brief flash | 200ms | Sensor reading triggered |
| Rapid flash | 100ms × 5 | Long press detected (leave/rejoin) |
| Rapid flash | 100ms × 10 | Sensor initialization error |

## Power Supply

### BRD4151A
- Can be powered via USB (5V, regulated to 3.3V onboard)
- Can use coin cell battery holder (CR2032, 3V)
- **Recommended for this project**: 2xAAA battery pack (3.0V nominal)

### TRÅDFRI
- Designed for 2xAAA battery pack (included with original device)
- Voltage range: 1.8V - 3.2V
- **Do not exceed 3.6V!**

## Battery Monitoring

The firmware monitors battery voltage using the internal ADC:
- **Channel**: AVDD (supply voltage measurement)
- **Resolution**: 12-bit (0-4095)
- **Reference**: 1.25V internal
- **Scale factor**: 4 (1/4 gain on AVDD input)
- **Measurement range**: 0 - 5V (suitable for 2xAAA)

The ADC reading is converted to voltage:
```
voltage_mv = (adc_value * 1250 * 4) / 4096
```

## Wiring Example (Breadboard Setup for BRD4151A)

```
                                    ┌─────────────┐
                                    │   BME280    │
                                    │   Module    │
                                    └─────────────┘
                                         │ │ │ │
                                        VCC│ │GND
                                         │SDA│
                                         │ │SCL
                    4.7kΩ    4.7kΩ      │ │ │
                     ┌┴┐     ┌┴┐       │ │ │
    3.3V ────────────┤ ├─────┤ ├───────┴─┘ │
                     └┬┘     └┬┘            │
    PC10 (SDA) ───────┴───────┼─────────────┘
                              │
    PC11 (SCL) ───────────────┘

    GND ───────────────────────────────────────────── GND
```

## Configuration Files

### BRD4151A Pin Definitions
Defined in `zigbee_bme280_sensor.slcp` (using default board pins):
- LED and Button use standard BRD4151A configuration
- I2C pins defined in `include/bme280_board_config.h`

### TRÅDFRI Pin Definitions
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

I2C pins defined in `include/bme280_board_config_tradfri.h`:
```c
#define BME280_I2C_SDA_PORT       gpioPortC
#define BME280_I2C_SDA_PIN        10
#define BME280_I2C_SCL_PORT       gpioPortC
#define BME280_I2C_SCL_PIN        11
```

## Testing

### LED Test
1. Flash firmware
2. Observe LED behavior on boot
3. Press button to test LED response

### Button Test
1. **Short press**: LED should flash briefly
2. **Long press (2s)**: LED should flash rapidly 5 times

### I2C Test
1. Power on device
2. Check serial output for "BME280 sensor initialized successfully"
3. If initialization fails, check I2C wiring and pull-up resistors

### BME280 Address Verification
If sensor not detected:
```bash
# Check I2C address using i2cdetect (if available)
# Default is 0x76, alternate is 0x77
```

Change address in firmware by connecting BME280 SDO pin:
- **GND**: Address 0x76 (default)
- **3.3V**: Address 0x77

Edit `include/bme280_board_config.h`:
```c
#define BME280_I2C_ADDR  0x77  // Change if using alternate address
```

## Troubleshooting

### LED not working
- **BRD4151A**: Check if PA0 is configured correctly
- **TRÅDFRI**: Verify gpioPortA, pin 0 in SLCP defines

### Button not responding
- **BRD4151A**: Check if PF6 is configured correctly
- **TRÅDFRI**: Verify gpioPortB, pin 13 in SLCP defines
- Ensure button is active-low with pull-up enabled

### I2C communication failure
1. Verify VCC is 3.3V (not 5V!)
2. Check pull-up resistors are present on SDA and SCL
3. Verify correct I2C address (0x76 or 0x77)
4. Check for loose connections
5. Measure voltage on I2C lines (should be ~3.3V when idle)

### Battery voltage reading incorrect
1. Check if battery is fresh (>2.8V for 2xAAA)
2. Verify no voltage drop under load
3. ADC measures AVDD - ensure power is stable

## References

- [EFR32MG1P Reference Manual](https://www.silabs.com/documents/public/reference-manuals/efr32xg1-rm.pdf)
- [BME280 Datasheet](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bme280-ds002.pdf)
- [IKEA TRÅDFRI Teardown](https://github.com/basilfx/TRADFRI-Hacking)
