/**
 * @file app_sensor.c
 * @brief BME280 sensor integration implementation
 */

#include "app_sensor.h"
#include "bme280_min.h"
#include "battery.h"
#include "af.h"
#include "app/framework/include/af.h"
#include <stdio.h>

// Endpoint where sensor clusters are located
#define SENSOR_ENDPOINT  1

// Convenience aliases for ZAP-generated cluster/attribute IDs
// ZAP uses "RELATIVE_HUMIDITY" not "HUMIDITY", so we create shorter aliases
#define ZCL_HUMIDITY_MEASUREMENT_CLUSTER_ID      ZCL_RELATIVE_HUMIDITY_MEASUREMENT_CLUSTER_ID
#define ZCL_HUMIDITY_MEASURED_VALUE_ATTRIBUTE_ID ZCL_RELATIVE_HUMIDITY_MEASURED_VALUE_ATTRIBUTE_ID

// Event control for periodic updates
static sl_zigbee_event_t sensor_update_event;

// Forward declaration
static void sensor_update_event_handler(sl_zigbee_event_t *event);

bool app_sensor_init(void)
{
  // Initialize BME280 sensor
  if (!bme280_init()) {
    emberAfCorePrintln("Error: BME280 initialization failed");
    return false;
  }

  emberAfCorePrintln("BME280 sensor initialized successfully");

  // Initialize battery monitoring
  if (!battery_init()) {
    emberAfCorePrintln("Error: Battery monitoring initialization failed");
    return false;
  }

  emberAfCorePrintln("Battery monitoring initialized successfully");

  // Initialize and schedule the event for periodic updates
  sl_zigbee_event_init(&sensor_update_event, sensor_update_event_handler);
  sl_zigbee_event_set_delay_ms(&sensor_update_event, SENSOR_UPDATE_INTERVAL_MS);

  return true;
}

void app_sensor_start_periodic_updates(void)
{
  // (Re)start the periodic sensor update event
  emberAfCorePrintln("Starting periodic sensor updates (interval: %d seconds)",
                     SENSOR_UPDATE_INTERVAL_MS / 1000);
  sl_zigbee_event_set_delay_ms(&sensor_update_event, SENSOR_UPDATE_INTERVAL_MS);
}

static void sensor_update_event_handler(sl_zigbee_event_t *event)
{
  // Only read sensor if network is up (power optimization)
  if (emberAfNetworkState() == EMBER_JOINED_NETWORK) {
    // Perform sensor update
    app_sensor_update();

    // Reschedule for next update
    sl_zigbee_event_set_delay_ms(&sensor_update_event, SENSOR_UPDATE_INTERVAL_MS);
  } else {
    // Network down - stop periodic reads to save power
    emberAfCorePrintln("Network down: sensor reads suspended");
    // Event will be restarted by app_sensor_start_periodic_updates() when network comes back up
  }
}

void app_sensor_update(void)
{
  bme280_data_t sensor_data;
  EmberAfStatus status;

  // Read sensor data
  if (!bme280_read_data(&sensor_data)) {
    emberAfCorePrintln("Error: Failed to read BME280 data");
    return;
  }

  emberAfCorePrintln("Sensor read: T=%d.%02d C, RH=%d.%02d %%, P=%d Pa",
                     sensor_data.temperature / 100,
                     sensor_data.temperature % 100,
                     sensor_data.humidity / 100,
                     sensor_data.humidity % 100,
                     sensor_data.pressure);

  // Update Temperature Measurement cluster (0x0402)
  // MeasuredValue is int16, in 0.01°C units
  int16_t temp_value = (int16_t)sensor_data.temperature;
  status = emberAfWriteServerAttribute(SENSOR_ENDPOINT,
                                       ZCL_TEMP_MEASUREMENT_CLUSTER_ID,
                                       ZCL_TEMP_MEASURED_VALUE_ATTRIBUTE_ID,
                                       (uint8_t *)&temp_value,
                                       ZCL_INT16S_ATTRIBUTE_TYPE);
  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    emberAfCorePrintln("Error: Failed to update temperature attribute (0x%x)", status);
  }

  // Update Relative Humidity Measurement cluster (0x0405)
  // MeasuredValue is uint16, in 0.01%RH units
  uint16_t humidity_value = (uint16_t)sensor_data.humidity;
  status = emberAfWriteServerAttribute(SENSOR_ENDPOINT,
                                       ZCL_HUMIDITY_MEASUREMENT_CLUSTER_ID,
                                       ZCL_HUMIDITY_MEASURED_VALUE_ATTRIBUTE_ID,
                                       (uint8_t *)&humidity_value,
                                       ZCL_INT16U_ATTRIBUTE_TYPE);
  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    emberAfCorePrintln("Error: Failed to update humidity attribute (0x%x)", status);
  }

  // Update Pressure Measurement cluster (0x0403)
  // MeasuredValue is int16, in kPa units (divide Pa by 1000)
  // Zigbee spec: signed 16-bit integer in kPa
  int16_t pressure_value = (int16_t)(sensor_data.pressure / 1000);
  status = emberAfWriteServerAttribute(SENSOR_ENDPOINT,
                                       ZCL_PRESSURE_MEASUREMENT_CLUSTER_ID,
                                       ZCL_PRESSURE_MEASURED_VALUE_ATTRIBUTE_ID,
                                       (uint8_t *)&pressure_value,
                                       ZCL_INT16S_ATTRIBUTE_TYPE);
  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    emberAfCorePrintln("Error: Failed to update pressure attribute (0x%x)", status);
  }

  // Update Power Configuration cluster (0x0001)
  // Read battery voltage and percentage
  uint16_t battery_voltage_mv = battery_read_voltage_mv();
  uint8_t battery_voltage_100mv = battery_read_voltage_100mv();
  uint8_t battery_percentage = battery_calculate_percentage(battery_voltage_mv);

  emberAfCorePrintln("Battery: %d mV (%d %%), raw: %d/200",
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

  // Trigger attribute reporting (if configured by coordinator)
  // The reporting mechanism will automatically send reports if bound
  emberAfCorePrintln("Sensor and battery attributes updated successfully");
}
