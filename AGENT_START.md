# Agent Start

When resuming work after a long pause, read files in this order:

1. `PROJECT_STATE.md`
2. `DECISIONS.md`
3. `RUNBOOK.md`
4. `app.c`
5. `src/app/app_sensor.c`
6. `.github/workflows/build-docker.yml`

## Current Priorities
- Finish multi-profile firmware flow (`bme280/bmp280/sht31`) in CI artifacts.
- Validate interview/reporting behavior per profile in ZHA.
- Keep button/join flow stable on TRADFRI PB13 hardware.

## Non-goals
- Do not add runtime dynamic cluster toggling.
- Do not keep unsupported clusters in profile signatures.
