# Power Optimization Notes

This document reflects current behavior (not historical experiments).

## Runtime Model

- Device is a Zigbee **Sleepy End Device (SED)** in release behavior.
- After join, firmware enables a temporary fast-poll window for interview/configuration.
- After that window, device returns to normal sleepy polling.

## Main Power Levers

1. `sensor_read_interval` (`Basic 0x0000`, mfg attr `0xF000`)
   - range `10..3600` seconds
   - default `10` seconds
2. Coordinator reporting configuration
   - min/max/reportable-change per cluster
3. Network quality
   - retries/rejoin directly affect battery life

## Debug Caveat

- `APP_DEBUG_NO_SLEEP=1` keeps the device awake and is useful for diagnostics only.
- Do not use debug no-sleep measurements to estimate battery life.

## Practical Recommendations

- For battery use, increase `sensor_read_interval` (e.g. 30..300s based on requirements).
- Keep reporting thresholds/min-max reasonable on coordinator side.
- Validate behavior in release-like sleep mode before battery benchmarking.
