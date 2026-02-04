const exposes = require('zigbee-herdsman-converters/lib/exposes');

const ea = exposes.access;

const manufacturerOptions = {manufacturerCode: 0x1002};

const ATTR = {
  sensor_read_interval: 0xF000,      // uint16, seconds
  temperature_offset: 0xF001,        // int16s, 0.01 C
  humidity_offset: 0xF002,           // int16s, 0.01 %RH
  pressure_offset: 0xF003,           // int16s, 0.01 kPa
  led_enable: 0xF004,                // boolean
  report_threshold_temperature: 0xF010, // uint16, 0.01 C
  report_threshold_humidity: 0xF011,    // uint16, 0.01 %RH
  report_threshold_pressure: 0xF012,    // uint16, 0.01 kPa
};

const scale10 = (v) => Math.round(v * 100);
const unscale10 = (v) => Math.round(v) / 100;

const fz_openbme280_config = {
  cluster: 'genBasic',
  type: ['attributeReport', 'readResponse'],
  convert: (model, msg, publish, options, meta) => {
    const data = msg.data || {};
    const result = {};

    const get = (id) => {
      if (data[id] !== undefined) return data[id];
      const key = id.toString();
      if (data[key] !== undefined) return data[key];
      return undefined;
    };

    const setScaled = (key, id) => {
      const raw = get(id);
      if (raw !== undefined) {
        result[key] = unscale10(raw);
      }
    };

    const setDirect = (key, id) => {
      const raw = get(id);
      if (raw !== undefined) {
        result[key] = raw;
      }
    };

    setDirect('sensor_read_interval', ATTR.sensor_read_interval);
    setScaled('temperature_offset', ATTR.temperature_offset);
    setScaled('humidity_offset', ATTR.humidity_offset);
    setScaled('pressure_offset', ATTR.pressure_offset);
    setDirect('led_enable', ATTR.led_enable);
    setScaled('report_threshold_temperature', ATTR.report_threshold_temperature);
    setScaled('report_threshold_humidity', ATTR.report_threshold_humidity);
    setScaled('report_threshold_pressure', ATTR.report_threshold_pressure);

    return result;
  },
};

const tz_openbme280_config = {
  key: Object.keys(ATTR),
  convertSet: async (entity, key, value, meta) => {
    const payload = {};
    switch (key) {
      case 'sensor_read_interval':
        payload[ATTR.sensor_read_interval] = Number(value);
        break;
      case 'temperature_offset':
        payload[ATTR.temperature_offset] = scale10(Number(value));
        break;
      case 'humidity_offset':
        payload[ATTR.humidity_offset] = scale10(Number(value));
        break;
      case 'pressure_offset':
        payload[ATTR.pressure_offset] = scale10(Number(value));
        break;
      case 'led_enable':
        payload[ATTR.led_enable] = value ? 1 : 0;
        break;
      case 'report_threshold_temperature':
        payload[ATTR.report_threshold_temperature] = scale10(Number(value));
        break;
      case 'report_threshold_humidity':
        payload[ATTR.report_threshold_humidity] = scale10(Number(value));
        break;
      case 'report_threshold_pressure':
        payload[ATTR.report_threshold_pressure] = scale10(Number(value));
        break;
      default:
        return;
    }

    await entity.write('genBasic', payload, manufacturerOptions);
    return {state: {[key]: value}};
  },
  convertGet: async (entity, key, meta) => {
    if (ATTR[key] === undefined) {
      return;
    }
    await entity.read('genBasic', [ATTR[key]], manufacturerOptions);
  },
};

module.exports = {
  zigbeeModel: ['TRADFRI-BME280'],
  model: 'TRADFRI-BME280',
  vendor: 'OpenBME280',
  description: 'Zigbee BME280/BMP280 sensor',
  fromZigbee: [fz_openbme280_config],
  toZigbee: [tz_openbme280_config],
  exposes: [
    exposes.numeric('sensor_read_interval', ea.STATE_SET)
      .withUnit('s')
      .withValueMin(10)
      .withValueMax(3600)
      .withValueStep(1),
    exposes.numeric('temperature_offset', ea.STATE_SET)
      .withUnit('C')
      .withValueMin(-5)
      .withValueMax(5)
      .withValueStep(0.01),
    exposes.numeric('humidity_offset', ea.STATE_SET)
      .withUnit('%')
      .withValueMin(-10)
      .withValueMax(10)
      .withValueStep(0.01),
    exposes.numeric('pressure_offset', ea.STATE_SET)
      .withUnit('kPa')
      .withValueMin(-5)
      .withValueMax(5)
      .withValueStep(0.01),
    exposes.binary('led_enable', ea.STATE_SET, 1, 0),
    exposes.numeric('report_threshold_temperature', ea.STATE_SET)
      .withUnit('C')
      .withValueMin(0)
      .withValueMax(10)
      .withValueStep(0.01),
    exposes.numeric('report_threshold_humidity', ea.STATE_SET)
      .withUnit('%')
      .withValueMin(0)
      .withValueMax(100)
      .withValueStep(0.01),
    exposes.numeric('report_threshold_pressure', ea.STATE_SET)
      .withUnit('kPa')
      .withValueMin(0)
      .withValueMax(10)
      .withValueStep(0.01),
  ],
  configure: async (device, coordinatorEndpoint, logger) => {
    const endpoint = device.getEndpoint(1);
    if (!endpoint) return;
    await endpoint.read('genBasic',
      Object.values(ATTR),
      manufacturerOptions);
  },
};
