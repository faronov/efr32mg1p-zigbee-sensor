# Documentation Index

This directory is the single source of project documentation.

## Core
- `PROJECT_STATE.md` - Current profiles, active files, behavior.
- `RUNBOOK.md` - Daily operations: CI, artifacts, flashing, SWO.
- `DECISIONS.md` - Accepted technical decisions and rationale.

## Hardware And Board
- `PINOUT.md` - TRADFRI module pin mapping and wiring.
- `TRADFRI_SETUP.md` - End-to-end setup for board, build, flash.

## OTA
- `OTA_SETUP_GUIDE.md` - Bootloader + storage setup for OTA.
- `OTA_FILE_CREATION.md` - Creating `.gbl` / `.ota` / `.zigbee` files.

## Integration
- `BINDING_GUIDE.md` - Coordinator-side binding/reporting basics.
- `zha_quirk_v2.py` - ZHA quirk v2 (sensor read interval only).
- `zigbee2mqtt-converter.js` - Zigbee2MQTT external converter.

## Power
- `POWER_OPTIMIZATION.md` - Sleep/poll behavior and power notes.

## Notes
- Removed overlapping docs:
  - `AGENT_START.md` (duplicated by `PROJECT_STATE.md` + `RUNBOOK.md` + `DECISIONS.md`)
  - `BATTERY_AND_BUTTON_FEATURES.md` (covered by `PINOUT.md`, `POWER_OPTIMIZATION.md`, `DECISIONS.md`)
  - `BOOTLOADER_FLASHING.md` (covered by `TRADFRI_SETUP.md` + `OTA_SETUP_GUIDE.md`)
