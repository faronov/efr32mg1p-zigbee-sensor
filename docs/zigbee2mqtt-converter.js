/**
 * Zigbee2MQTT External Converter for EFR32MG1P BME280 Sensor
 *
 * Installation:
 * 1. Save this file to your Zigbee2MQTT data directory (e.g., /app/data/)
 * 2. Add to configuration.yaml:
 *    external_converters:
 *      - zigbee2mqtt-converter.js
 * 3. Restart Zigbee2MQTT
 */

const fz = require('zigbee-herdsman-converters/converters/fromZigbee');
const tz = require('zigbee-herdsman-converters/converters/toZigbee');
const exposes = require('zigbee-herdsman-converters/lib/exposes');
const reporting = require('zigbee-herdsman-converters/lib/reporting');
const e = exposes.presets;
const ea = exposes.access;

// Manufacturer-specific attribute IDs
const MANUFACTURER_ATTRIBUTES = {
    sensorReadInterval: 0xF000,
    temperatureOffset: 0xF001,
    humidityOffset: 0xF002,
    pressureOffset: 0xF003,
    ledEnable: 0xF004,
    reportThresholdTemperature: 0xF010,
    reportThresholdHumidity: 0xF011,
    reportThresholdPressure: 0xF012,
};

// Custom fromZigbee converter for configuration attributes
const fzLocal = {
    bme280_config: {
        cluster: 'genBasic',
        type: ['attributeReport', 'readResponse'],
        convert: (model, msg, publish, options, meta) => {
            const result = {};

            if (msg.data.hasOwnProperty(MANUFACTURER_ATTRIBUTES.sensorReadInterval)) {
                result.sensor_read_interval = msg.data[MANUFACTURER_ATTRIBUTES.sensorReadInterval];
            }
            if (msg.data.hasOwnProperty(MANUFACTURER_ATTRIBUTES.temperatureOffset)) {
                result.temperature_offset = msg.data[MANUFACTURER_ATTRIBUTES.temperatureOffset] / 100.0;
            }
            if (msg.data.hasOwnProperty(MANUFACTURER_ATTRIBUTES.humidityOffset)) {
                result.humidity_offset = msg.data[MANUFACTURER_ATTRIBUTES.humidityOffset] / 100.0;
            }
            if (msg.data.hasOwnProperty(MANUFACTURER_ATTRIBUTES.pressureOffset)) {
                result.pressure_offset = msg.data[MANUFACTURER_ATTRIBUTES.pressureOffset] / 100.0;
            }
            if (msg.data.hasOwnProperty(MANUFACTURER_ATTRIBUTES.ledEnable)) {
                result.led_enable = msg.data[MANUFACTURER_ATTRIBUTES.ledEnable] === 1;
            }
            if (msg.data.hasOwnProperty(MANUFACTURER_ATTRIBUTES.reportThresholdTemperature)) {
                result.report_threshold_temperature = msg.data[MANUFACTURER_ATTRIBUTES.reportThresholdTemperature] / 100.0;
            }
            if (msg.data.hasOwnProperty(MANUFACTURER_ATTRIBUTES.reportThresholdHumidity)) {
                result.report_threshold_humidity = msg.data[MANUFACTURER_ATTRIBUTES.reportThresholdHumidity] / 100.0;
            }
            if (msg.data.hasOwnProperty(MANUFACTURER_ATTRIBUTES.reportThresholdPressure)) {
                result.report_threshold_pressure = msg.data[MANUFACTURER_ATTRIBUTES.reportThresholdPressure] / 100.0;
            }

            return result;
        },
    },
};

// Custom toZigbee converter for configuration attributes
const tzLocal = {
    bme280_config: {
        key: [
            'sensor_read_interval',
            'temperature_offset',
            'humidity_offset',
            'pressure_offset',
            'led_enable',
            'report_threshold_temperature',
            'report_threshold_humidity',
            'report_threshold_pressure',
        ],
        convertSet: async (entity, key, value, meta) => {
            const payloads = {
                sensor_read_interval: () => ({[MANUFACTURER_ATTRIBUTES.sensorReadInterval]: value}),
                temperature_offset: () => ({[MANUFACTURER_ATTRIBUTES.temperatureOffset]: Math.round(value * 100)}),
                humidity_offset: () => ({[MANUFACTURER_ATTRIBUTES.humidityOffset]: Math.round(value * 100)}),
                pressure_offset: () => ({[MANUFACTURER_ATTRIBUTES.pressureOffset]: Math.round(value * 100)}),
                led_enable: () => ({[MANUFACTURER_ATTRIBUTES.ledEnable]: value ? 1 : 0}),
                report_threshold_temperature: () => ({[MANUFACTURER_ATTRIBUTES.reportThresholdTemperature]: Math.round(value * 100)}),
                report_threshold_humidity: () => ({[MANUFACTURER_ATTRIBUTES.reportThresholdHumidity]: Math.round(value * 100)}),
                report_threshold_pressure: () => ({[MANUFACTURER_ATTRIBUTES.reportThresholdPressure]: Math.round(value * 100)}),
            };

            await entity.write('genBasic', payloads[key]());
            return {state: {[key]: value}};
        },
        convertGet: async (entity, key, meta) => {
            const attributes = {
                sensor_read_interval: MANUFACTURER_ATTRIBUTES.sensorReadInterval,
                temperature_offset: MANUFACTURER_ATTRIBUTES.temperatureOffset,
                humidity_offset: MANUFACTURER_ATTRIBUTES.humidityOffset,
                pressure_offset: MANUFACTURER_ATTRIBUTES.pressureOffset,
                led_enable: MANUFACTURER_ATTRIBUTES.ledEnable,
                report_threshold_temperature: MANUFACTURER_ATTRIBUTES.reportThresholdTemperature,
                report_threshold_humidity: MANUFACTURER_ATTRIBUTES.reportThresholdHumidity,
                report_threshold_pressure: MANUFACTURER_ATTRIBUTES.reportThresholdPressure,
            };

            await entity.read('genBasic', [attributes[key]]);
        },
    },
};

const definition = {
    zigbeeModel: ['EFR32MG1P_BME280'],
    model: 'EFR32MG1P_BME280',
    vendor: 'Custom',
    description: 'BME280 Temperature, Humidity, and Pressure Sensor',

    fromZigbee: [
        fz.temperature,
        fz.humidity,
        fz.pressure,
        fz.battery,
        fz.identify,
        fzLocal.bme280_config,
    ],

    toZigbee: [
        tz.factory_reset,
        tzLocal.bme280_config,
    ],

    exposes: [
        e.temperature(),
        e.humidity(),
        e.pressure(),
        e.battery(),
        e.battery_voltage(),

        // Configuration parameters
        exposes.numeric('sensor_read_interval', ea.ALL)
            .withValueMin(10)
            .withValueMax(3600)
            .withUnit('s')
            .withDescription('Sensor reading interval (10-3600 seconds)'),

        exposes.numeric('temperature_offset', ea.ALL)
            .withValueMin(-5.0)
            .withValueMax(5.0)
            .withValueStep(0.01)
            .withUnit('°C')
            .withDescription('Temperature calibration offset (-5.0 to +5.0°C)'),

        exposes.numeric('humidity_offset', ea.ALL)
            .withValueMin(-10.0)
            .withValueMax(10.0)
            .withValueStep(0.01)
            .withUnit('%')
            .withDescription('Humidity calibration offset (-10.0 to +10.0%)'),

        exposes.numeric('pressure_offset', ea.ALL)
            .withValueMin(-5.0)
            .withValueMax(5.0)
            .withValueStep(0.01)
            .withUnit('kPa')
            .withDescription('Pressure calibration offset (-5.0 to +5.0 kPa)'),

        exposes.binary('led_enable', ea.ALL, true, false)
            .withDescription('Enable or disable LED indicator'),

        exposes.numeric('report_threshold_temperature', ea.ALL)
            .withValueMin(0.01)
            .withValueMax(10.0)
            .withValueStep(0.01)
            .withUnit('°C')
            .withDescription('Temperature change threshold for reporting (default: 1.0°C)'),

        exposes.numeric('report_threshold_humidity', ea.ALL)
            .withValueMin(0.01)
            .withValueMax(10.0)
            .withValueStep(0.01)
            .withUnit('%')
            .withDescription('Humidity change threshold for reporting (default: 1.0%)'),

        exposes.numeric('report_threshold_pressure', ea.ALL)
            .withValueMin(0.01)
            .withValueMax(10.0)
            .withValueStep(0.01)
            .withUnit('kPa')
            .withDescription('Pressure change threshold for reporting (default: 0.01 kPa)'),
    ],

    configure: async (device, coordinatorEndpoint, logger) => {
        const endpoint = device.getEndpoint(1);

        // Bind standard clusters for sensor data
        await reporting.bind(endpoint, coordinatorEndpoint, [
            'msTemperatureMeasurement',
            'msRelativeHumidity',
            'msPressureMeasurement',
            'genPowerCfg',
        ]);

        // Configure reporting for sensor measurements
        await reporting.temperature(endpoint);
        await reporting.humidity(endpoint);
        await reporting.pressure(endpoint);
        await reporting.batteryVoltage(endpoint);
        await reporting.batteryPercentageRemaining(endpoint);

        // Read current configuration values
        await endpoint.read('genBasic', [
            MANUFACTURER_ATTRIBUTES.sensorReadInterval,
            MANUFACTURER_ATTRIBUTES.temperatureOffset,
            MANUFACTURER_ATTRIBUTES.humidityOffset,
            MANUFACTURER_ATTRIBUTES.pressureOffset,
            MANUFACTURER_ATTRIBUTES.ledEnable,
            MANUFACTURER_ATTRIBUTES.reportThresholdTemperature,
            MANUFACTURER_ATTRIBUTES.reportThresholdHumidity,
            MANUFACTURER_ATTRIBUTES.reportThresholdPressure,
        ]);
    },
};

module.exports = definition;
