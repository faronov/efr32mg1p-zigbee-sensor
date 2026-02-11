# Decisions

## D-001: Device-side reporting defaults + coordinator override
- Status: accepted
- Decision:
  - Configure default reporting entries in firmware.
  - Keep device fully configurable by coordinator (`Configure Reporting`).
- Rationale:
  - Device works out of the box after join.
  - ZHA/Z2M can still tune min/max/change.

## D-002: Profile-driven cluster sets
- Status: in progress
- Decision:
  - Maintain separate profile builds (`bme280`, `bmp280`, `sht31`) with matching cluster sets.
- Rationale:
  - Avoid mismatched signatures and empty clusters.
  - Cleaner interview/reconfigure behavior.

## D-003: Join-time power behavior for SED
- Status: accepted
- Decision:
  - Use temporary short-poll window after join for interview speed.
  - Return to normal sleep after window ends.
- Rationale:
  - Faster commissioning while preserving battery behavior.

## D-004: Periodic sensor timer policy
- Status: accepted
- Decision:
  - Arm periodic sensor timer on `NETWORK_UP`.
  - Stop timer on `NETWORK_DOWN`.
- Rationale:
  - Avoid unnecessary wake-ups while offline.

## D-005: Button event source
- Status: accepted
- Decision:
  - Use debounced `simple_button` path.
  - Avoid raw direct GPIO edge fallback for action generation.
- Rationale:
  - Raw PB13 reads are susceptible to noise/touch-trigger on this hardware.

## D-006: Simplify manufacturer-specific configuration surface
- Status: accepted
- Decision:
  - Keep only one custom Basic attribute:
    - `0xF000` (`sensor_read_interval`, seconds)
  - Remove custom offsets/LED/report-threshold attributes from firmware API.
- Rationale:
  - Reduce interview/reconfigure friction and mismatch risk with coordinators/quirks.
  - Keep one stable runtime knob while preserving standard Zigbee reporting control.
