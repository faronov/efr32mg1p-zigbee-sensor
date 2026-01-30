/**
 * @file app_config.c
 * @brief Configuration attribute handler for BME280 sensor
 *
 * Handles manufacturer-specific configuration attributes in Basic cluster (0x0000).
 * These attributes are stored in NVM and persist across reboots.
 */

#include "app_config.h"
#include "app_sensor.h"
#include "af.h"
#include "app/framework/include/af.h"
#include <string.h>

// Endpoint where configuration attributes are located
#define CONFIG_ENDPOINT  1

// Manufacturer-specific attribute IDs in Basic cluster (0xF000 range)
#define ZCL_SENSOR_READ_INTERVAL_ATTRIBUTE_ID          0xF000  // uint16, seconds
#define ZCL_TEMPERATURE_OFFSET_ATTRIBUTE_ID            0xF001  // int16s, 0.01°C units
#define ZCL_HUMIDITY_OFFSET_ATTRIBUTE_ID               0xF002  // int16s, 0.01% units
#define ZCL_PRESSURE_OFFSET_ATTRIBUTE_ID               0xF003  // int16s, 0.01 kPa units
#define ZCL_LED_ENABLE_ATTRIBUTE_ID                    0xF004  // boolean
#define ZCL_REPORT_THRESHOLD_TEMPERATURE_ATTRIBUTE_ID  0xF010  // uint16, 0.01°C units
#define ZCL_REPORT_THRESHOLD_HUMIDITY_ATTRIBUTE_ID     0xF011  // uint16, 0.01% units
#define ZCL_REPORT_THRESHOLD_PRESSURE_ATTRIBUTE_ID     0xF012  // uint16, 0.01 kPa units

// Global configuration (loaded from NVM at startup)
static app_config_t config;

void app_config_init(void)
{
  EmberAfStatus status;

  // Load sensor read interval from NVM
  uint16_t interval;
  status = emberAfReadServerAttribute(CONFIG_ENDPOINT,
                                      ZCL_BASIC_CLUSTER_ID,
                                      ZCL_SENSOR_READ_INTERVAL_ATTRIBUTE_ID,
                                      (uint8_t *)&interval,
                                      sizeof(interval));
  config.sensor_read_interval_seconds = (status == EMBER_ZCL_STATUS_SUCCESS) ? interval : 60;

  // Load temperature offset
  int16_t temp_offset;
  status = emberAfReadServerAttribute(CONFIG_ENDPOINT,
                                      ZCL_BASIC_CLUSTER_ID,
                                      ZCL_TEMPERATURE_OFFSET_ATTRIBUTE_ID,
                                      (uint8_t *)&temp_offset,
                                      sizeof(temp_offset));
  config.temperature_offset_centidegrees = (status == EMBER_ZCL_STATUS_SUCCESS) ? temp_offset : 0;

  // Load humidity offset
  int16_t humidity_offset;
  status = emberAfReadServerAttribute(CONFIG_ENDPOINT,
                                      ZCL_BASIC_CLUSTER_ID,
                                      ZCL_HUMIDITY_OFFSET_ATTRIBUTE_ID,
                                      (uint8_t *)&humidity_offset,
                                      sizeof(humidity_offset));
  config.humidity_offset_centipercent = (status == EMBER_ZCL_STATUS_SUCCESS) ? humidity_offset : 0;

  // Load pressure offset
  int16_t pressure_offset;
  status = emberAfReadServerAttribute(CONFIG_ENDPOINT,
                                      ZCL_BASIC_CLUSTER_ID,
                                      ZCL_PRESSURE_OFFSET_ATTRIBUTE_ID,
                                      (uint8_t *)&pressure_offset,
                                      sizeof(pressure_offset));
  config.pressure_offset_centikilopascals = (status == EMBER_ZCL_STATUS_SUCCESS) ? pressure_offset : 0;

  // Load LED enable
  bool led_enable;
  status = emberAfReadServerAttribute(CONFIG_ENDPOINT,
                                      ZCL_BASIC_CLUSTER_ID,
                                      ZCL_LED_ENABLE_ATTRIBUTE_ID,
                                      (uint8_t *)&led_enable,
                                      sizeof(led_enable));
  config.led_enable = (status == EMBER_ZCL_STATUS_SUCCESS) ? led_enable : true;

  // Load report thresholds
  uint16_t temp_threshold;
  status = emberAfReadServerAttribute(CONFIG_ENDPOINT,
                                      ZCL_BASIC_CLUSTER_ID,
                                      ZCL_REPORT_THRESHOLD_TEMPERATURE_ATTRIBUTE_ID,
                                      (uint8_t *)&temp_threshold,
                                      sizeof(temp_threshold));
  config.report_threshold_temperature = (status == EMBER_ZCL_STATUS_SUCCESS) ? temp_threshold : 100;

  uint16_t humidity_threshold;
  status = emberAfReadServerAttribute(CONFIG_ENDPOINT,
                                      ZCL_BASIC_CLUSTER_ID,
                                      ZCL_REPORT_THRESHOLD_HUMIDITY_ATTRIBUTE_ID,
                                      (uint8_t *)&humidity_threshold,
                                      sizeof(humidity_threshold));
  config.report_threshold_humidity = (status == EMBER_ZCL_STATUS_SUCCESS) ? humidity_threshold : 100;

  uint16_t pressure_threshold;
  status = emberAfReadServerAttribute(CONFIG_ENDPOINT,
                                      ZCL_BASIC_CLUSTER_ID,
                                      ZCL_REPORT_THRESHOLD_PRESSURE_ATTRIBUTE_ID,
                                      (uint8_t *)&pressure_threshold,
                                      sizeof(pressure_threshold));
  config.report_threshold_pressure = (status == EMBER_ZCL_STATUS_SUCCESS) ? pressure_threshold : 1;

  emberAfCorePrintln("Config loaded:");
  emberAfCorePrintln("  Read interval: %d seconds", config.sensor_read_interval_seconds);
  emberAfCorePrintln("  Temp offset: %d (0.01°C)", config.temperature_offset_centidegrees);
  emberAfCorePrintln("  Humidity offset: %d (0.01%%)", config.humidity_offset_centipercent);
  emberAfCorePrintln("  Pressure offset: %d (0.01 kPa)", config.pressure_offset_centikilopascals);
  emberAfCorePrintln("  LED enable: %d", config.led_enable);
}

const app_config_t* app_config_get(void)
{
  return &config;
}

/**
 * @brief Callback when Basic cluster attributes are written
 *
 * This callback is invoked when any attribute in Basic cluster is written
 * by a remote device (e.g., via Zigbee2MQTT or ZHA).
 */
void emberAfBasicClusterServerAttributeChangedCallback(uint8_t endpoint,
                                                        EmberAfAttributeId attributeId)
{
  EmberAfStatus status;

  switch (attributeId) {
    case ZCL_SENSOR_READ_INTERVAL_ATTRIBUTE_ID: {
      uint16_t interval;
      status = emberAfReadServerAttribute(endpoint,
                                          ZCL_BASIC_CLUSTER_ID,
                                          ZCL_SENSOR_READ_INTERVAL_ATTRIBUTE_ID,
                                          (uint8_t *)&interval,
                                          sizeof(interval));
      if (status == EMBER_ZCL_STATUS_SUCCESS && interval >= 10 && interval <= 3600) {
        config.sensor_read_interval_seconds = interval;
        emberAfCorePrintln("Sensor read interval changed to %d seconds", interval);
        // Apply new interval
        app_sensor_set_interval(interval * 1000);  // Convert to milliseconds
      }
      break;
    }

    case ZCL_TEMPERATURE_OFFSET_ATTRIBUTE_ID: {
      int16_t offset;
      status = emberAfReadServerAttribute(endpoint,
                                          ZCL_BASIC_CLUSTER_ID,
                                          ZCL_TEMPERATURE_OFFSET_ATTRIBUTE_ID,
                                          (uint8_t *)&offset,
                                          sizeof(offset));
      if (status == EMBER_ZCL_STATUS_SUCCESS) {
        config.temperature_offset_centidegrees = offset;
        emberAfCorePrintln("Temperature offset changed to %d (0.01°C)", offset);
      }
      break;
    }

    case ZCL_HUMIDITY_OFFSET_ATTRIBUTE_ID: {
      int16_t offset;
      status = emberAfReadServerAttribute(endpoint,
                                          ZCL_BASIC_CLUSTER_ID,
                                          ZCL_HUMIDITY_OFFSET_ATTRIBUTE_ID,
                                          (uint8_t *)&offset,
                                          sizeof(offset));
      if (status == EMBER_ZCL_STATUS_SUCCESS) {
        config.humidity_offset_centipercent = offset;
        emberAfCorePrintln("Humidity offset changed to %d (0.01%%)", offset);
      }
      break;
    }

    case ZCL_PRESSURE_OFFSET_ATTRIBUTE_ID: {
      int16_t offset;
      status = emberAfReadServerAttribute(endpoint,
                                          ZCL_BASIC_CLUSTER_ID,
                                          ZCL_PRESSURE_OFFSET_ATTRIBUTE_ID,
                                          (uint8_t *)&offset,
                                          sizeof(offset));
      if (status == EMBER_ZCL_STATUS_SUCCESS) {
        config.pressure_offset_centikilopascals = offset;
        emberAfCorePrintln("Pressure offset changed to %d (0.01 kPa)", offset);
      }
      break;
    }

    case ZCL_LED_ENABLE_ATTRIBUTE_ID: {
      bool enable;
      status = emberAfReadServerAttribute(endpoint,
                                          ZCL_BASIC_CLUSTER_ID,
                                          ZCL_LED_ENABLE_ATTRIBUTE_ID,
                                          (uint8_t *)&enable,
                                          sizeof(enable));
      if (status == EMBER_ZCL_STATUS_SUCCESS) {
        config.led_enable = enable;
        emberAfCorePrintln("LED enable changed to %d", enable);
      }
      break;
    }

    case ZCL_REPORT_THRESHOLD_TEMPERATURE_ATTRIBUTE_ID: {
      uint16_t threshold;
      status = emberAfReadServerAttribute(endpoint,
                                          ZCL_BASIC_CLUSTER_ID,
                                          ZCL_REPORT_THRESHOLD_TEMPERATURE_ATTRIBUTE_ID,
                                          (uint8_t *)&threshold,
                                          sizeof(threshold));
      if (status == EMBER_ZCL_STATUS_SUCCESS) {
        config.report_threshold_temperature = threshold;
        emberAfCorePrintln("Temperature report threshold changed to %d (0.01°C)", threshold);
      }
      break;
    }

    case ZCL_REPORT_THRESHOLD_HUMIDITY_ATTRIBUTE_ID: {
      uint16_t threshold;
      status = emberAfReadServerAttribute(endpoint,
                                          ZCL_BASIC_CLUSTER_ID,
                                          ZCL_REPORT_THRESHOLD_HUMIDITY_ATTRIBUTE_ID,
                                          (uint8_t *)&threshold,
                                          sizeof(threshold));
      if (status == EMBER_ZCL_STATUS_SUCCESS) {
        config.report_threshold_humidity = threshold;
        emberAfCorePrintln("Humidity report threshold changed to %d (0.01%%)", threshold);
      }
      break;
    }

    case ZCL_REPORT_THRESHOLD_PRESSURE_ATTRIBUTE_ID: {
      uint16_t threshold;
      status = emberAfReadServerAttribute(endpoint,
                                          ZCL_BASIC_CLUSTER_ID,
                                          ZCL_REPORT_THRESHOLD_PRESSURE_ATTRIBUTE_ID,
                                          (uint8_t *)&threshold,
                                          sizeof(threshold));
      if (status == EMBER_ZCL_STATUS_SUCCESS) {
        config.report_threshold_pressure = threshold;
        emberAfCorePrintln("Pressure report threshold changed to %d (0.01 kPa)", threshold);
      }
      break;
    }

    default:
      // Other Basic cluster attributes - ignore
      break;
  }
}
