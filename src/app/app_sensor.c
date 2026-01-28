/**
 * @file app_sensor.c
 * @brief BME280 sensor integration implementation
 */

#include "app_sensor.h"
#include "bme280_min.h"
#include "af.h"
#include "app/framework/include/af.h"
#include <stdio.h>

// Zigbee cluster IDs
#define ZCL_TEMP_MEASUREMENT_CLUSTER_ID      0x0402
#define ZCL_PRESSURE_MEASUREMENT_CLUSTER_ID  0x0403
#define ZCL_HUMIDITY_MEASUREMENT_CLUSTER_ID  0x0405

// Zigbee attribute IDs
#define ZCL_TEMP_MEASURED_VALUE_ATTRIBUTE_ID      0x0000
#define ZCL_PRESSURE_MEASURED_VALUE_ATTRIBUTE_ID  0x0000
#define ZCL_HUMIDITY_MEASURED_VALUE_ATTRIBUTE_ID  0x0000

// Endpoint where sensor clusters are located
#define SENSOR_ENDPOINT  1

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

  // Initialize and schedule the event for periodic updates
  sl_zigbee_event_init(&sensor_update_event, sensor_update_event_handler);
  sl_zigbee_event_set_delay_ms(&sensor_update_event, SENSOR_UPDATE_INTERVAL_MS);

  return true;
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
    // Event will be restarted when network comes back up
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

  // Trigger attribute reporting (if configured by coordinator)
  // The reporting mechanism will automatically send reports if bound
  emberAfCorePrintln("Sensor attributes updated successfully");
}
