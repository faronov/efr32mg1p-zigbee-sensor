/**
 * Zigbee2MQTT External Converter for OpenBME280 sensor profiles.
 * Supports one manufacturer-specific config attribute:
 *   - sensor_read_interval (Basic/0x0000, attr 0xF000, mfgCode 0x1002)
 */

const fz = require('zigbee-herdsman-converters/converters/fromZigbee');
const tz = require('zigbee-herdsman-converters/converters/toZigbee');
const exposes = require('zigbee-herdsman-converters/lib/exposes');
const reporting = require('zigbee-herdsman-converters/lib/reporting');

const e = exposes.presets;
const ea = exposes.access;

const MANUFACTURER_CODE = 0x1002;
const SENSOR_READ_INTERVAL_ATTR = 0xF000;

const fzLocal = {
  openbme280_config: {
    cluster: 'genBasic',
    type: ['attributeReport', 'readResponse'],
    convert: (model, msg, publish, options, meta) => {
      const data = msg.data || {};
      const raw = data[SENSOR_READ_INTERVAL_ATTR] ?? data[SENSOR_READ_INTERVAL_ATTR.toString()];
      if (raw === undefined) return {};
      return {sensor_read_interval: raw};
    },
  },
};

const tzLocal = {
  openbme280_config: {
    key: ['sensor_read_interval'],
    convertSet: async (entity, key, value, meta) => {
      const interval = Number(value);
      await entity.write('genBasic', {[SENSOR_READ_INTERVAL_ATTR]: interval}, {manufacturerCode: MANUFACTURER_CODE});
      return {state: {sensor_read_interval: interval}};
    },
    convertGet: async (entity, key, meta) => {
      await entity.read('genBasic', [SENSOR_READ_INTERVAL_ATTR], {manufacturerCode: MANUFACTURER_CODE});
    },
  },
};

module.exports = {
  zigbeeModel: ['TRADFRI-BME280'],
  model: 'TRADFRI-BME280',
  vendor: 'OpenBME280',
  description: 'BME280/BMP280/SHT31 sensor',
  fromZigbee: [
    fz.temperature,
    fz.humidity,
    fz.pressure,
    fz.battery,
    fz.identify,
    fzLocal.openbme280_config,
  ],
  toZigbee: [
    tz.factory_reset,
    tzLocal.openbme280_config,
  ],
  exposes: [
    e.temperature(),
    e.humidity(),
    e.pressure(),
    e.battery(),
    e.battery_voltage(),
    exposes.numeric('sensor_read_interval', ea.ALL)
      .withValueMin(10)
      .withValueMax(3600)
      .withValueStep(1)
      .withUnit('s')
      .withDescription('Sensor reading interval in seconds'),
  ],
  configure: async (device, coordinatorEndpoint, logger) => {
    const endpoint = device.getEndpoint(1);
    await reporting.bind(endpoint, coordinatorEndpoint, [
      'msTemperatureMeasurement',
      'msRelativeHumidity',
      'msPressureMeasurement',
      'genPowerCfg',
    ]);
    await reporting.temperature(endpoint);
    await reporting.humidity(endpoint);
    await reporting.pressure(endpoint);
    await reporting.batteryVoltage(endpoint);
    await reporting.batteryPercentageRemaining(endpoint);
    await endpoint.read('genBasic', [SENSOR_READ_INTERVAL_ATTR], {manufacturerCode: MANUFACTURER_CODE});
  },
};
