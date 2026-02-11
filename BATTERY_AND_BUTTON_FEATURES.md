# Battery And Button Behavior

This file documents the current (authoritative) behavior.

## Battery

- Battery data is exposed through Zigbee Power Configuration cluster (`0x0001`):
  - `0x0020` BatteryVoltage (100mV units)
  - `0x0021` BatteryPercentageRemaining (0..200)
- Measurement is done with internal ADC and written to Zigbee attributes by firmware.
- Delivery to coordinator depends on binding + reporting configuration.

## Button (PB13)

- Input is debounced via `simple_button` path.
- Internal pull-up is enabled; external pull-up is still recommended on noisy hardware.
- Press classification:
  - `short press` => immediate sensor read/report trigger (or join when not joined)
  - `long press` => leave/rejoin flow (threshold defined in firmware)
- Firmware has a short guard window after boot/join/leave to ignore spurious presses.

## Sleep / Debug

- Release behavior: sleepy end device with temporary post-join fast poll window.
- Debug behavior may disable sleep (`APP_DEBUG_NO_SLEEP`) and is not suitable for battery estimates.

## Runtime Config (manufacturer-specific)

- Only one custom Basic attribute is supported:
  - `0xF000` (`sensor_read_interval`, seconds, `10..3600`, default `10`)
- Old custom attributes for offsets/LED/report thresholds are removed from firmware API.
