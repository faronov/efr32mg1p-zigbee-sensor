# Project State

## Purpose
TRADFRI EFR32MG1P Zigbee sleepy end-device firmware with sensor profiles and battery reporting.

## Active Profiles
- `bme280`: temperature + humidity + pressure + battery
- `bmp280`: temperature + pressure + battery
- `sht31`: temperature + humidity + battery

## Profile Files
- SLC:
  - `zigbee_sensor_tradfri_bme280.slcp`
  - `zigbee_sensor_tradfri_bmp280.slcp`
  - `zigbee_sensor_tradfri_sht31.slcp`
- ZAP:
  - `config/zcl/zcl_bme280.zap`
  - `config/zcl/zcl_bmp280.zap`
  - `config/zcl/zcl_sht31.zap`

## Core Implementation
- Profile selection macro: `APP_SENSOR_PROFILE` (see `include/app_profile.h`)
- Sensor logic:
  - `src/app/app_sensor.c`
  - Uses BME/BMP driver for profiles 1/2
  - Uses SHT31 driver for profile 3
- Runtime config:
  - Manufacturer-specific Basic attribute `0xF000` (`sensor_read_interval`, seconds)
  - Default: `10`, range: `10..3600`
- Reporting defaults:
  - `app.c` (`app_configure_default_reporting`)
  - Values are defined in ZAP and can be overridden by coordinator

## Sleep/Join/Button Notes
- Sleep timer for periodic sensor updates is armed on `NETWORK_UP` and stopped on `NETWORK_DOWN`.
- Button is handled through debounced `simple_button` path.
- Internal pull-up enabled on `PB13`; external pull-up resistor is still recommended on noisy hardware.

## Known Hardware Caveats
- TRADFRI boards can show PB13 noise when touched; internal pull-up may be insufficient alone.
- SPI flash is physically present on the module (IS25LQ020B); probe/enable behavior may still vary by board wiring and PF3 enable state.
- If BME280 init fails, debug fallback values can be used (when debug flag is enabled).
