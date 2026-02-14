# Binding Guide

Current project model: **coordinator-side binding**.

## What Is Required

- Device exposes standard server clusters on endpoint `1`:
  - `msTemperatureMeasurement` (`0x0402`)
  - `msRelativeHumidity` (`0x0405`) for profiles that support humidity
  - `msPressureMeasurement` (`0x0403`) for profiles that support pressure
  - `genPowerCfg` (`0x0001`)
- Coordinator binds these clusters and configures reporting.

No extra device-side binding table customization is required for normal ZHA/Z2M usage.

## ZHA

- Reconfigure device from UI after pairing.
- Verify bound clusters in ZHA diagnostics/signature.
- If values do not update, re-run reconfigure and check logs for reporting traffic.

## Zigbee2MQTT

Use standard Z2M bind/configure flow from UI or bridge requests.

Example bind request:

```bash
mosquitto_pub -t 'zigbee2mqtt/bridge/request/device/bind' \
  -m '{"from":"<device_ieee>","to":"coordinator","clusters":["msTemperatureMeasurement","msRelativeHumidity","msPressureMeasurement","genPowerCfg"]}'
```

## Troubleshooting

- Reconfigure succeeds but no updates:
  - verify device joined as expected and reports are visible on-air/SWO
  - verify coordinator reporting config (min/max/change) for each cluster
- Cluster not present:
  - check selected firmware profile (`bme280`/`bmp280`/`sht31`)
