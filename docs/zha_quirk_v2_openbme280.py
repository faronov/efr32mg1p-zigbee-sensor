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
