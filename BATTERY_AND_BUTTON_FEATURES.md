# Battery Monitoring and Button Features

This document describes the new battery monitoring and button press features added to the project.

## Overview

The following features have been implemented:

1. **Battery Voltage Monitoring** - Real-time voltage measurement using internal ADC
2. **Power Configuration Cluster** - Zigbee standard battery reporting (0x0001)
3. **Button Press Duration Detection** - Long press to leave/rejoin network
4. **2xAAA Battery Configuration** - Optimized for 2xAAA alkaline batteries
5. **Faster Sensor Updates** - 1-minute intervals (increased from 5 minutes)

## Battery Monitoring

### Hardware

The EFR32MG1P internal ADC is used to measure VDD (supply voltage) without external components.

### Battery Configuration

The device is configured for **2xAAA alkaline batteries**:

| Voltage | Percentage | Meaning |
|---------|------------|---------|
| 3.2V    | 100% (200) | Fresh batteries (2x 1.6V) |
| 3.0V    | 86% (172)  | Nominal voltage (2x 1.5V) |
| 2.4V    | 43% (86)   | Half depleted |
| 1.8V    | 0% (0)     | Empty (2x 0.9V) |

### Zigbee Power Configuration Cluster (0x0001)

The following attributes are exposed:

| Attribute ID | Name | Type | Value | Description |
|--------------|------|------|-------|-------------|
| 0x0020 | BatteryVoltage | uint8 | 0-255 | Voltage in 100mV units (e.g., 30 = 3.0V) |
| 0x0021 | BatteryPercentageRemaining | uint8 | 0-200 | Battery percentage (200 = 100%, 0.5% resolution) |
| 0x0031 | BatterySize | enum8 | 3 | Battery type (3 = AAA) |
| 0x0033 | BatteryQuantity | uint8 | 2 | Number of batteries |
| 0x0035 | BatteryRatedVoltage | uint8 | 30 | Nominal voltage in 100mV units (3.0V) |

### Reporting Configuration

Battery attributes are reported:
- **Periodic**: Every 1 minute with sensor readings
- **Change-based**:
  - Voltage: When voltage changes by ≥0.1V (1 unit)
  - Percentage: When percentage changes by ≥5% (10 units)

### Implementation Files

- **src/drivers/battery.h** - Battery measurement API
- **src/drivers/battery.c** - ADC-based voltage measurement for EFR32MG1P
- **src/app/app_sensor.c** - Integration with sensor update loop

## Button Press Duration Detection

### Feature

The button now detects **press duration** instead of just press events:

| Press Type | Duration | Action (Joined) | Action (Not Joined) |
|------------|----------|----------------|---------------------|
| Short press | <2 seconds | Immediate sensor read & report | Start network join |
| Long press | ≥2 seconds | Leave network and rejoin | No action |

### Use Cases

**Short press:**
- Quick sensor reading on demand
- Manual trigger for attribute reports
- Initial network joining

**Long press:**
- Switch to a different Zigbee coordinator
- Force network rejoin after coordinator reset
- Troubleshooting network issues

### Implementation

The button handler tracks press start time using `sl_sleeptimer_get_tick_count()` and calculates duration on release:

```c
#define LONG_PRESS_THRESHOLD_MS 2000

// On button press: record start time
button_press_start_tick = sl_sleeptimer_get_tick_count();

// On button release: calculate duration
uint32_t duration_ms = sl_sleeptimer_tick_to_ms(duration_ticks);

if (duration_ms >= LONG_PRESS_THRESHOLD_MS) {
  handle_long_press();  // Leave & rejoin
} else {
  handle_short_press(); // Immediate sensor read
}
```

### LED Indication

- **Short press**: Brief flash (200ms) when joined
- **Long press**: Rapid flashing (5x 100ms) during leave operation
- **Network join**: Slow blinking (500ms) during join attempt

## Sensor Update Interval

### Changed Configuration

Updated from **5 minutes** to **1 minute** for more responsive updates with 2xAAA batteries.

| Configuration | Interval | Battery Life (2xAAA ~2400mAh) |
|--------------|----------|-------------------------------|
| **Current** | 1 minute | ~6-12 months |
| Balanced | 5 minutes | ~12-18 months |
| Maximum life | 15 minutes | ~18-24 months |

### Customization

To change the interval, edit `src/app/app_sensor.h`:

```c
#define SENSOR_UPDATE_INTERVAL_MS   60000  // 1 minute (current)
#define SENSOR_UPDATE_INTERVAL_MS   300000 // 5 minutes (balanced)
#define SENSOR_UPDATE_INTERVAL_MS   900000 // 15 minutes (max life)
```

## Power Consumption Estimate

### Active Components (1-minute updates)

| Component | Current | Duration | Energy/Update |
|-----------|---------|----------|---------------|
| MCU active | ~4 mA | ~50 ms | ~0.056 µWh |
| I2C sensor read | ~3 mA | ~20 ms | ~0.017 µWh |
| Radio TX | ~10 mA | ~10 ms | ~0.028 µWh |
| ADC measurement | ~0.5 mA | ~1 ms | ~0.001 µWh |
| **Total per update** | - | ~81 ms | **~0.1 µWh** |

### Sleep Mode

| Mode | Current | Time (per minute) | Energy/minute |
|------|---------|-------------------|---------------|
| EM2 sleep | ~3 µA | ~59.919 s | ~2.5 µWh |
| Active | ~5 mA | ~0.081 s | ~0.1 µWh |
| **Total per minute** | - | - | **~2.6 µWh** |

### Battery Life Calculation

**2xAAA alkaline (2400 mAh @ 3.0V) = 7200 mWh**

- Average power: 2.6 µWh/min = 0.156 mW
- Battery life: 7200 mWh / 0.156 mW = **46,154 hours ≈ 5.3 years**

**Note**: This is a theoretical maximum. Real-world factors reduce this:
- Network activity (polling, retries, reports)
- Temperature effects on battery capacity
- Self-discharge (~2-3% per year for alkaline)
- LED usage during user interaction

**Realistic estimate: 6-12 months** with normal Zigbee network activity.

## Testing Recommendations

### Battery Monitoring Test

1. Build and flash firmware
2. Connect to Zigbee network
3. Use ZHA/Zigbee2MQTT to read Power Configuration cluster:
   ```bash
   # Read battery voltage (0x0020)
   zcl read <endpoint> 0x0001 0x0020

   # Read battery percentage (0x0021)
   zcl read <endpoint> 0x0001 0x0021
   ```
4. Verify values are reasonable (30-32 for fresh batteries)

### Button Test

1. **Short press test**:
   - Press and release button quickly (<2s)
   - LED should flash briefly
   - Check logs for "Short press detected"
   - Verify sensor reading is triggered

2. **Long press test**:
   - Press and hold button for 2+ seconds
   - LED should flash rapidly when released
   - Check logs for "Long press detected: Leave/Rejoin network"
   - Device should leave and rejoin network

### Network Leave/Rejoin Test

1. Join device to coordinator
2. Long press button (≥2s)
3. Observe in coordinator logs:
   - Device leaves network
   - Device rejoins after ~0.5s
4. Verify device is functional after rejoin

## Files Modified

### New Files
- `src/drivers/battery.h` - Battery monitoring API
- `src/drivers/battery.c` - ADC-based voltage measurement
- `tools/add_power_config_cluster.py` - Script to add Power Config cluster to ZAP

### Modified Files
- `app.c` - Button press duration detection and handlers
- `src/app/app_sensor.c` - Battery monitoring integration
- `src/app/app_sensor.h` - Updated sensor interval to 1 minute
- `zigbee_bme280_sensor.slcp` - Added emlib_adc and battery.c
- `zigbee_bme280_sensor_tradfri.slcp` - Added emlib_adc and battery.c
- `config/zcl/zcl_config.zap` - Added Power Configuration cluster
- `README.md` - Updated documentation

## Building

No changes to build process. Both variants (BRD4151A and TRÅDFRI) will be built by CI/CD:

```bash
# Local build (requires GSDK)
export GSDK_DIR=/path/to/gecko_sdk
export BOARD=brd4151a  # or custom for TRÅDFRI
./tools/build.sh
```

## Future Enhancements

Possible improvements for battery monitoring:

1. **Battery alarm thresholds** - Trigger alerts at 20% and 10%
2. **Voltage history tracking** - Estimate remaining time
3. **Temperature compensation** - Adjust voltage reading based on temperature
4. **Dynamic update intervals** - Reduce frequency as battery depletes
5. **Low battery mode** - Reduce features when battery is low

## References

- Zigbee Cluster Library Specification, Power Configuration Cluster (0x0001)
- EFR32MG1 Reference Manual, ADC chapter
- Alkaline Battery Discharge Characteristics (Energizer, Duracell datasheets)
