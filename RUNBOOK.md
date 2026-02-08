# Runbook

## Build (local)
```bash
export GSDK_DIR=/path/to/gecko_sdk
export SLCP_FILE=zigbee_sensor_tradfri_bme280.slcp   # or bmp280/sht31
bash tools/build.sh
```

## CI Build
Push branch and watch workflow:
```bash
gh run list -R faronov/efr32mg1p-bme280-zigbee-sensor -L 1
gh run watch <run_id> -R faronov/efr32mg1p-bme280-zigbee-sensor --exit-status
```

## Download Artifacts
```bash
tmpdir=/tmp/ci-<run_id>
rm -rf "$tmpdir" && mkdir -p "$tmpdir"
gh run download <run_id> -R faronov/efr32mg1p-bme280-zigbee-sensor -D "$tmpdir"
```

## Flash Full Stack
```bash
C=/Applications/Commander-cli.app/Contents/MacOS/commander-cli
D=EFR32MG1P132F256
T=/tmp/ci-<run_id>
FS=$(find "$T" -type f -name first_stage.s37 | head -n1)
BL=$(find "$T" -type f -name tradfri-bootloader-spiflash.s37 | head -n1)
APP=$(find "$T" -type f -name '*.s37' | grep -E 'zigbee_.*tradfri.*(debug)?\\.s37' | head -n1)

"$C" device masserase --device "$D"
"$C" flash "$FS" --device "$D"
"$C" flash "$BL" --device "$D"
"$C" flash "$APP" --device "$D"
"$C" device reset --device "$D"
```

## SWO Read
```bash
/Applications/Commander-cli.app/Contents/MacOS/commander-cli swo read --device EFR32MG1P132F256
```

## Expected SWO Milestones
- Boot banners and debug flags
- AF init callback
- Join flow (`NWK Steering`, `EMBER_NETWORK_UP`)
- Device announce
- Read/report traffic (`FC 00` reads, `FC 10 cmd 0B` reports)

## Quick Troubleshooting
- No button response:
  - verify PB13 wiring and external pull-up (4.7k-10k to VCC)
  - check `BTN0:` lines in SWO
- Interview stalls:
  - ensure fast poll window after join is active
  - confirm inbound reads appear in SWO
- No report updates:
  - verify `FC 10 ... cmd 0B` traffic in SWO
  - check report thresholds vs actual value change
