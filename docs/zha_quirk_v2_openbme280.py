from zigpy.zcl import types as t
from zigpy.zcl.clusters.general import Basic, PowerConfiguration, Identify
from zigpy.zcl.clusters.measurement import (
    TemperatureMeasurement,
    PressureMeasurement,
    RelativeHumidity,
)
from zhaquirks import CustomCluster, QuirkBuilder


MANUFACTURER = "OpenBME280"
MODEL = "TRADFRI-BME280"
MANUFACTURER_CODE = 0x1002


class OpenBme280Basic(CustomCluster, Basic):
    manufacturer_id = MANUFACTURER_CODE
    attributes = Basic.attributes.copy()
    attributes.update(
        {
            0xF000: ("sensor_read_interval", t.uint16_t),
            0xF001: ("temperature_offset", t.int16s),
            0xF002: ("humidity_offset", t.int16s),
            0xF003: ("pressure_offset", t.int16s),
            0xF004: ("led_enable", t.Bool),
            0xF010: ("report_threshold_temperature", t.uint16_t),
            0xF011: ("report_threshold_humidity", t.uint16_t),
            0xF012: ("report_threshold_pressure", t.uint16_t),
        }
    )


quirk = (
    QuirkBuilder(MANUFACTURER, MODEL)
    .adds(OpenBme280Basic)
    .adds(PowerConfiguration)
    .adds(Identify)
    .adds(TemperatureMeasurement)
    .adds(PressureMeasurement)
    .adds(RelativeHumidity)
    .build()
)
