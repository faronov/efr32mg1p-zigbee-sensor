const exposes = require('zigbee-herdsman-converters/lib/exposes');

const ea = exposes.access;
const manufacturerOptions = {manufacturerCode: 0x1002};
const ATTR_SENSOR_READ_INTERVAL = 0xF000; // uint16, seconds

const fz_openbme280_config = {
  cluster: 'genBasic',
  type: ['attributeReport', 'readResponse'],
  convert: (model, msg, publish, options, meta) => {
    const data = msg.data || {};
    const raw = data[ATTR_SENSOR_READ_INTERVAL] ?? data[ATTR_SENSOR_READ_INTERVAL.toString()];
    if (raw === undefined) {
      return {};
    }
    return {sensor_read_interval: raw};
  },
};

const tz_openbme280_config = {
  key: ['sensor_read_interval'],
  convertSet: async (entity, key, value, meta) => {
    const interval = Number(value);
    await entity.write('genBasic', {[ATTR_SENSOR_READ_INTERVAL]: interval}, manufacturerOptions);
    return {state: {sensor_read_interval: interval}};
  },
  convertGet: async (entity, key, meta) => {
    await entity.read('genBasic', [ATTR_SENSOR_READ_INTERVAL], manufacturerOptions);
  },
};

module.exports = {
  zigbeeModel: ['TRADFRI-BME280'],
  model: 'TRADFRI-BME280',
  vendor: 'OpenBME280',
  description: 'Zigbee BME280/BMP280/SHT31 sensor',
  fromZigbee: [fz_openbme280_config],
  toZigbee: [tz_openbme280_config],
  exposes: [
    exposes.numeric('sensor_read_interval', ea.STATE_SET)
      .withUnit('s')
      .withValueMin(10)
      .withValueMax(3600)
      .withValueStep(1),
  ],
  configure: async (device, coordinatorEndpoint, logger) => {
    const endpoint = device.getEndpoint(1);
    if (!endpoint) return;
    await endpoint.read('genBasic', [ATTR_SENSOR_READ_INTERVAL], manufacturerOptions);
  },
};
