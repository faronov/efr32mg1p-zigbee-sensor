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
TEMPERATURE_OFFSET_ATTR = 0xF001
HUMIDITY_OFFSET_ATTR = 0xF002
PRESSURE_OFFSET_ATTR = 0xF003
LED_ENABLE_ATTR = 0xF004
REPORT_THRESHOLD_TEMPERATURE_ATTR = 0xF010
REPORT_THRESHOLD_HUMIDITY_ATTR = 0xF011
REPORT_THRESHOLD_PRESSURE_ATTR = 0xF012


(
    QuirkBuilder("Custom", "EFR32MG1P_BME280")
    .replaces(Basic.cluster_id)
    .enum(
        "led_enable",
        t.Bool,
        Basic.cluster_id,
        LED_ENABLE_ATTR,
        translation_key="led_enable",
        fallback_name="LED Enable",
    )
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
    .number(
        "temperature_offset",
        t.int16s,
        Basic.cluster_id,
        TEMPERATURE_OFFSET_ATTR,
        min_value=-500,
        max_value=500,
        step=1,
        unit="°C",
        multiplier=0.01,
        translation_key="temperature_offset",
        fallback_name="Temperature Offset",
    )
    .number(
        "humidity_offset",
        t.int16s,
        Basic.cluster_id,
        HUMIDITY_OFFSET_ATTR,
        min_value=-1000,
        max_value=1000,
        step=1,
        unit="%",
        multiplier=0.01,
        translation_key="humidity_offset",
        fallback_name="Humidity Offset",
    )
    .number(
        "pressure_offset",
        t.int16s,
        Basic.cluster_id,
        PRESSURE_OFFSET_ATTR,
        min_value=-500,
        max_value=500,
        step=1,
        unit="kPa",
        multiplier=0.01,
        translation_key="pressure_offset",
        fallback_name="Pressure Offset",
    )
    .number(
        "report_threshold_temperature",
        t.uint16_t,
        Basic.cluster_id,
        REPORT_THRESHOLD_TEMPERATURE_ATTR,
        min_value=1,
        max_value=1000,
        step=1,
        unit="°C",
        multiplier=0.01,
        translation_key="report_threshold_temperature",
        fallback_name="Temperature Report Threshold",
    )
    .number(
        "report_threshold_humidity",
        t.uint16_t,
        Basic.cluster_id,
        REPORT_THRESHOLD_HUMIDITY_ATTR,
        min_value=1,
        max_value=1000,
        step=1,
        unit="%",
        multiplier=0.01,
        translation_key="report_threshold_humidity",
        fallback_name="Humidity Report Threshold",
    )
    .number(
        "report_threshold_pressure",
        t.uint16_t,
        Basic.cluster_id,
        REPORT_THRESHOLD_PRESSURE_ATTR,
        min_value=1,
        max_value=1000,
        step=1,
        unit="kPa",
        multiplier=0.01,
        translation_key="report_threshold_pressure",
        fallback_name="Pressure Report Threshold",
    )
    .add_to_registry()
)
