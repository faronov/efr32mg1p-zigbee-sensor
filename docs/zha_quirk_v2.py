"""
ZHA Quirk v2 for EFR32MG1P BME280 Sensor

Installation:
1. Save this file to your Home Assistant custom_zha_quirks directory:
   - Docker: /config/custom_zha_quirks/
   - Core: <config_dir>/custom_zha_quirks/
2. Restart Home Assistant
3. Re-pair the device or reload the ZHA integration

More info: https://www.home-assistant.io/integrations/zha#custom-quirks
"""

from zigpy.quirks.v2 import QuirkBuilder
from zigpy.zcl.clusters.general import Basic
from zigpy import types as t

# Manufacturer-specific attribute IDs (0xF000 range)
SENSOR_READ_INTERVAL_ATTR = 0xF000


(
    QuirkBuilder("Custom", "EFR32MG1P_BME280")
    .replaces(Basic.cluster_id)
    .number(
        "sensor_read_interval",
        t.uint16_t,
        Basic.cluster_id,
        SENSOR_READ_INTERVAL_ATTR,
        min_value=10,
        max_value=3600,
        step=1,
        unit="s",
        multiplier=1,
        translation_key="sensor_read_interval",
        fallback_name="Sensor Read Interval",
    )
    .add_to_registry()
)
