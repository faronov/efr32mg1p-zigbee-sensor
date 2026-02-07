/**
 * @file app_sensor.c
 * @brief BME280 sensor integration implementation
 */

#include "app_sensor.h"
#include "app_config.h"
#include "bme280_min.h"
#include "battery.h"
#include "af.h"
#include "app/framework/include/af.h"
#include "sl_sleeptimer.h"
#include <stdio.h>

// Endpoint where sensor clusters are located
#define SENSOR_ENDPOINT  1

// Convenience aliases for ZAP-generated cluster/attribute IDs
// ZAP uses "RELATIVE_HUMIDITY" not "HUMIDITY", so we create shorter aliases
#define ZCL_HUMIDITY_MEASUREMENT_CLUSTER_ID      ZCL_RELATIVE_HUMIDITY_MEASUREMENT_CLUSTER_ID
#define ZCL_HUMIDITY_MEASURED_VALUE_ATTRIBUTE_ID ZCL_RELATIVE_HUMIDITY_MEASURED_VALUE_ATTRIBUTE_ID

// Event control for periodic updates
static sl_zigbee_event_t sensor_update_event;
static bool sensor_update_event_initialized = false;
static bool sensor_ready = false;
static bool battery_ready = false;

// Configurable sensor update interval
static uint32_t sensor_update_interval_ms = SENSOR_UPDATE_INTERVAL_MS;

#ifndef APP_DEBUG_FAKE_SENSOR_VALUES
#define APP_DEBUG_FAKE_SENSOR_VALUES 0
#endif
#ifndef APP_FORCE_SENSOR_INTERVAL_MS
#define APP_FORCE_SENSOR_INTERVAL_MS 10000
#endif

static uint32_t fake_last_change_ms = 0;
static bme280_data_t fake_sensor_data = {
  .temperature = 2150, // 21.50 C
  .humidity = 5000,    // 50.00 %
  .pressure = 101325,  // Pa
};
static uint32_t fake_prng_state = 0x12345678u;

static uint32_t app_get_ms(void)
{
  uint32_t ticks = sl_sleeptimer_get_tick_count();
  return (uint32_t)sl_sleeptimer_tick_to_ms(ticks);
}

static uint32_t app_fake_prng_next(uint32_t salt)
{
  fake_prng_state = (fake_prng_state * 1664525u) + 1013904223u + salt;
  return fake_prng_state;
}

static int32_t app_fake_apply_delta_percent(int32_t base, int8_t percent)
{
  return base + ((base * percent) / 100);
}

static void app_update_fake_sensor_data(uint32_t now_ms)
{
  if (fake_last_change_ms != 0 && (now_ms - fake_last_change_ms) < 60000u) {
    return;
  }
  fake_last_change_ms = now_ms;

  int8_t d_t = (int8_t)((int32_t)(app_fake_prng_next(now_ms) % 21u) - 10);
  int8_t d_h = (int8_t)((int32_t)(app_fake_prng_next(now_ms + 1u) % 21u) - 10);
  int8_t d_p = (int8_t)((int32_t)(app_fake_prng_next(now_ms + 2u) % 21u) - 10);

  fake_sensor_data.temperature = app_fake_apply_delta_percent(fake_sensor_data.temperature, d_t);
  fake_sensor_data.humidity = app_fake_apply_delta_percent(fake_sensor_data.humidity, d_h);
  fake_sensor_data.pressure = app_fake_apply_delta_percent(fake_sensor_data.pressure, d_p);

  if (fake_sensor_data.humidity < 0) {
    fake_sensor_data.humidity = 0;
  } else if (fake_sensor_data.humidity > 10000) {
    fake_sensor_data.humidity = 10000;
  }

  if (fake_sensor_data.pressure < 80000) {
    fake_sensor_data.pressure = 80000;
  } else if (fake_sensor_data.pressure > 120000) {
    fake_sensor_data.pressure = 120000;
  }
}

// Forward declaration
static void sensor_update_event_handler(sl_zigbee_event_t *event);

bool app_sensor_init(void)
{
  sensor_update_event_initialized = false;
  sensor_ready = false;
  battery_ready = false;

  // Initialize battery monitoring regardless of sensor presence.
  battery_ready = battery_init();
  if (!battery_ready) {
    emberAfCorePrintln("Error: Battery monitoring initialization failed");
  } else {
    emberAfCorePrintln("Battery monitoring initialized successfully");
  }

  // Initialize BME280 sensor
  if (!bme280_init()) {
    emberAfCorePrintln("Error: BME280 initialization failed");
    sensor_ready = false;
  } else {
    sensor_ready = true;
    emberAfCorePrintln("Detected sensor chip ID: 0x%02X (%s)",
                       bme280_get_chip_id(),
                       bme280_has_humidity() ? "BME280" : "BMP280");
    emberAfCorePrintln("BME280 sensor initialized successfully");
  }

  if (!sensor_ready && !battery_ready) {
    emberAfCorePrintln("Error: neither sensor nor battery monitor initialized");
    return false;
  }

  // Load configured interval from NVM
  const app_config_t* config = app_config_get();
  sensor_update_interval_ms = config->sensor_read_interval_seconds * 1000;
#if APP_FORCE_SENSOR_INTERVAL_MS > 0
  sensor_update_interval_ms = APP_FORCE_SENSOR_INTERVAL_MS;
#endif

  // Initialize and schedule the event for periodic updates
  sl_zigbee_event_init(&sensor_update_event, sensor_update_event_handler);
  sensor_update_event_initialized = true;
  sl_zigbee_event_set_delay_ms(&sensor_update_event, sensor_update_interval_ms);

  emberAfCorePrintln("Sensor poll interval: %d seconds", sensor_update_interval_ms / 1000);
  emberAfCorePrintln("Reporting thresholds (local attrs): dT=%d dRH=%d dP=%d",
                     config->report_threshold_temperature,
                     config->report_threshold_humidity,
                     config->report_threshold_pressure);

  return true;
}

void app_sensor_start_periodic_updates(void)
{
  if (!sensor_update_event_initialized) {
    emberAfCorePrintln("Sensor periodic updates disabled: sensor/event not initialized");
    return;
  }

  // (Re)start the periodic sensor update event
  emberAfCorePrintln("Starting periodic sensor updates (interval: %d seconds)",
                     sensor_update_interval_ms / 1000);
  sl_zigbee_event_set_delay_ms(&sensor_update_event, sensor_update_interval_ms);
}

static void sensor_update_event_handler(sl_zigbee_event_t *event)
{
  // Only read sensor if network is up (power optimization)
  if (emberAfNetworkState() == EMBER_JOINED_NETWORK) {
    // Perform sensor update
    app_sensor_update();

    // Reschedule for next update
    sl_zigbee_event_set_delay_ms(&sensor_update_event, sensor_update_interval_ms);
  } else {
    // Network down - stop periodic reads to save power
    emberAfCorePrintln("Network down: sensor reads suspended");
    // Event will be restarted by app_sensor_start_periodic_updates() when network comes back up
  }
}

void app_sensor_set_interval(uint32_t interval_ms)
{
  if (interval_ms < 10000) {
    emberAfCorePrintln("Warning: Interval too short, using minimum 10 seconds");
    interval_ms = 10000;
  }

  sensor_update_interval_ms = interval_ms;
  emberAfCorePrintln("Sensor update interval changed to %d seconds", interval_ms / 1000);

  if (!sensor_update_event_initialized) {
    emberAfCorePrintln("Sensor interval stored; periodic event will start after sensor init");
    return;
  }

  // Restart the timer with new interval
  sl_zigbee_event_set_delay_ms(&sensor_update_event, sensor_update_interval_ms);
}

void app_sensor_update(void)
{
  bme280_data_t sensor_data;
  EmberAfStatus status;
  bool has_humidity = sensor_ready ? bme280_has_humidity() : true;
  bool have_sensor_sample = false;
  uint32_t now_ms = app_get_ms();

  // Read sensor data
  if (sensor_ready) {
    if (bme280_read_data(&sensor_data)) {
      have_sensor_sample = true;
    } else {
      emberAfCorePrintln("Error: Failed to read BME280 data");
    }
  }

  if (!have_sensor_sample && APP_DEBUG_FAKE_SENSOR_VALUES) {
    app_update_fake_sensor_data(now_ms);
    sensor_data = fake_sensor_data;
    has_humidity = true;
    have_sensor_sample = true;
    emberAfCorePrintln("Sensor: using debug fallback values (minute drift)");
  }

  if (have_sensor_sample && has_humidity) {
    emberAfCorePrintln("Sensor read (raw): T=%d.%02d C, RH=%d.%02d %%, P=%d Pa",
                       sensor_data.temperature / 100,
                       sensor_data.temperature % 100,
                       sensor_data.humidity / 100,
                       sensor_data.humidity % 100,
                       sensor_data.pressure);
  } else if (have_sensor_sample) {
    emberAfCorePrintln("Sensor read (raw): T=%d.%02d C, RH=--, P=%d Pa",
                       sensor_data.temperature / 100,
                       sensor_data.temperature % 100,
                       sensor_data.pressure);
  }

  // Get configuration and apply calibration offsets
  const app_config_t* config = app_config_get();
  // Apply calibration offsets
  int32_t temp_calibrated = 0;
  int32_t humidity_calibrated = 0;
  int32_t pressure_calibrated = 0;
  if (have_sensor_sample) {
    temp_calibrated = sensor_data.temperature + config->temperature_offset_centidegrees;
    if (has_humidity) {
      humidity_calibrated = sensor_data.humidity + config->humidity_offset_centipercent;
    }
    pressure_calibrated = sensor_data.pressure + (config->pressure_offset_centikilopascals * 10); // Convert 0.01 kPa to Pa
  }

  if (have_sensor_sample && has_humidity) {
    emberAfCorePrintln("Sensor read (calibrated): T=%d.%02d C, RH=%d.%02d %%, P=%d Pa",
                       (int)(temp_calibrated / 100),
                       (int)(temp_calibrated % 100),
                       (int)(humidity_calibrated / 100),
                       (int)(humidity_calibrated % 100),
                       (int)pressure_calibrated);
  } else if (have_sensor_sample) {
    emberAfCorePrintln("Sensor read (calibrated): T=%d.%02d C, RH=--, P=%d Pa",
                       (int)(temp_calibrated / 100),
                       (int)(temp_calibrated % 100),
                       (int)pressure_calibrated);
  }

  if (have_sensor_sample) {
    // Update Temperature Measurement cluster (0x0402)
    // MeasuredValue is int16, in 0.01°C units
    int16_t temp_value = (int16_t)temp_calibrated;
    status = emberAfWriteServerAttribute(SENSOR_ENDPOINT,
                                         ZCL_TEMP_MEASUREMENT_CLUSTER_ID,
                                         ZCL_TEMP_MEASURED_VALUE_ATTRIBUTE_ID,
                                         (uint8_t *)&temp_value,
                                         ZCL_INT16S_ATTRIBUTE_TYPE);
    if (status != EMBER_ZCL_STATUS_SUCCESS) {
      emberAfCorePrintln("Error: Failed to update temperature attribute (0x%x)", status);
    }

    if (has_humidity) {
      // Update Relative Humidity Measurement cluster (0x0405)
      // MeasuredValue is uint16, in 0.01%RH units
      uint16_t humidity_value = (uint16_t)humidity_calibrated;
      status = emberAfWriteServerAttribute(SENSOR_ENDPOINT,
                                           ZCL_HUMIDITY_MEASUREMENT_CLUSTER_ID,
                                           ZCL_HUMIDITY_MEASURED_VALUE_ATTRIBUTE_ID,
                                           (uint8_t *)&humidity_value,
                                           ZCL_INT16U_ATTRIBUTE_TYPE);
      if (status != EMBER_ZCL_STATUS_SUCCESS) {
        emberAfCorePrintln("Error: Failed to update humidity attribute (0x%x)", status);
      }
    } else {
      emberAfCorePrintln("Humidity not supported by sensor (BMP280)");
    }

    // Update Pressure Measurement cluster (0x0403)
    // MeasuredValue is int16, in kPa units (divide Pa by 1000)
    // Zigbee spec: signed 16-bit integer in kPa
    int16_t pressure_value = (int16_t)(pressure_calibrated / 1000);
    status = emberAfWriteServerAttribute(SENSOR_ENDPOINT,
                                         ZCL_PRESSURE_MEASUREMENT_CLUSTER_ID,
                                         ZCL_PRESSURE_MEASURED_VALUE_ATTRIBUTE_ID,
                                         (uint8_t *)&pressure_value,
                                         ZCL_INT16S_ATTRIBUTE_TYPE);
    if (status != EMBER_ZCL_STATUS_SUCCESS) {
      emberAfCorePrintln("Error: Failed to update pressure attribute (0x%x)", status);
    }
  }

  if (battery_ready) {
    // Update Power Configuration cluster (0x0001)
    // Read battery voltage and percentage
    uint16_t battery_voltage_mv = battery_read_voltage_mv();
    uint8_t battery_voltage_100mv = (uint8_t)(battery_voltage_mv / 100);
    uint8_t battery_percentage = battery_calculate_percentage(battery_voltage_mv);
    uint16_t battery_adc_raw = battery_get_last_raw_adc();
    bool battery_sample_valid = battery_last_measurement_valid();

    emberAfCorePrintln("Battery: adc=%d %s, %d mV (%d %%), raw: %d/200",
                       battery_adc_raw,
                       battery_sample_valid ? "OK" : "FALLBACK",
                       battery_voltage_mv,
                       battery_percentage / 2, // Convert to percentage (200 = 100%)
                       battery_percentage);

    // Update BatteryVoltage attribute (0x0020)
    // uint8, in 100mV units (e.g., 30 = 3.0V)
    status = emberAfWriteServerAttribute(SENSOR_ENDPOINT,
                                         ZCL_POWER_CONFIG_CLUSTER_ID,
                                         ZCL_BATTERY_VOLTAGE_ATTRIBUTE_ID,
                                         (uint8_t *)&battery_voltage_100mv,
                                         ZCL_INT8U_ATTRIBUTE_TYPE);
    if (status != EMBER_ZCL_STATUS_SUCCESS) {
      emberAfCorePrintln("Error: Failed to update battery voltage attribute (0x%x)", status);
    }

    // Update BatteryPercentageRemaining attribute (0x0021)
    // uint8, 0-200 range (0.5% resolution, 200 = 100%)
    status = emberAfWriteServerAttribute(SENSOR_ENDPOINT,
                                         ZCL_POWER_CONFIG_CLUSTER_ID,
                                         ZCL_BATTERY_PERCENTAGE_REMAINING_ATTRIBUTE_ID,
                                         (uint8_t *)&battery_percentage,
                                         ZCL_INT8U_ATTRIBUTE_TYPE);
    if (status != EMBER_ZCL_STATUS_SUCCESS) {
      emberAfCorePrintln("Error: Failed to update battery percentage attribute (0x%x)", status);
    }
  } else {
    emberAfCorePrintln("Battery monitor not initialized");
  }

  // Trigger attribute reporting (if configured by coordinator)
  // The reporting mechanism will automatically send reports if bound
  emberAfCorePrintln("Sensor/battery attribute update complete");
}

bool app_sensor_is_ready(void)
{
  return sensor_ready;
}
