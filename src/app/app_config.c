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

// Global configuration (loaded from NVM at startup)
static app_config_t config;

static EmberAfStatus read_config_attribute(EmberAfAttributeId attribute_id,
                                           uint8_t *data,
                                           uint8_t data_size)
{
  return emberAfReadManufacturerSpecificServerAttribute(CONFIG_ENDPOINT,
                                                        ZCL_BASIC_CLUSTER_ID,
                                                        attribute_id,
                                                        APP_MANUFACTURER_CODE,
                                                        data,
                                                        data_size);
}

void app_config_init(void)
{
  EmberAfStatus status;

  // Load sensor read interval from NVM
  uint16_t interval;
  status = read_config_attribute(ZCL_SENSOR_READ_INTERVAL_ATTRIBUTE_ID,
                                 (uint8_t *)&interval,
                                 sizeof(interval));
  config.sensor_read_interval_seconds = (status == EMBER_ZCL_STATUS_SUCCESS) ? interval : 60;
  if (config.sensor_read_interval_seconds < 10 || config.sensor_read_interval_seconds > 3600) {
    config.sensor_read_interval_seconds = 60;
  }

  // Load temperature offset
  int16_t temp_offset;
  status = read_config_attribute(ZCL_TEMPERATURE_OFFSET_ATTRIBUTE_ID,
                                 (uint8_t *)&temp_offset,
                                 sizeof(temp_offset));
  config.temperature_offset_centidegrees = (status == EMBER_ZCL_STATUS_SUCCESS) ? temp_offset : 0;

  // Load humidity offset
  int16_t humidity_offset;
  status = read_config_attribute(ZCL_HUMIDITY_OFFSET_ATTRIBUTE_ID,
                                 (uint8_t *)&humidity_offset,
                                 sizeof(humidity_offset));
  config.humidity_offset_centipercent = (status == EMBER_ZCL_STATUS_SUCCESS) ? humidity_offset : 0;

  // Load pressure offset
  int16_t pressure_offset;
  status = read_config_attribute(ZCL_PRESSURE_OFFSET_ATTRIBUTE_ID,
                                 (uint8_t *)&pressure_offset,
                                 sizeof(pressure_offset));
  config.pressure_offset_centikilopascals = (status == EMBER_ZCL_STATUS_SUCCESS) ? pressure_offset : 0;

  // Load LED enable
  bool led_enable;
  status = read_config_attribute(ZCL_LED_ENABLE_ATTRIBUTE_ID,
                                 (uint8_t *)&led_enable,
                                 sizeof(led_enable));
  config.led_enable = (status == EMBER_ZCL_STATUS_SUCCESS) ? led_enable : true;

  // Load report thresholds
  uint16_t temp_threshold;
  status = read_config_attribute(ZCL_REPORT_THRESHOLD_TEMPERATURE_ATTRIBUTE_ID,
                                 (uint8_t *)&temp_threshold,
                                 sizeof(temp_threshold));
  config.report_threshold_temperature = (status == EMBER_ZCL_STATUS_SUCCESS) ? temp_threshold : 100;

  uint16_t humidity_threshold;
  status = read_config_attribute(ZCL_REPORT_THRESHOLD_HUMIDITY_ATTRIBUTE_ID,
                                 (uint8_t *)&humidity_threshold,
                                 sizeof(humidity_threshold));
  config.report_threshold_humidity = (status == EMBER_ZCL_STATUS_SUCCESS) ? humidity_threshold : 100;

  uint16_t pressure_threshold;
  status = read_config_attribute(ZCL_REPORT_THRESHOLD_PRESSURE_ATTRIBUTE_ID,
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

static EmberAfStatus write_config_attribute(EmberAfAttributeId attribute_id,
                                            const uint8_t *data,
                                            EmberAfAttributeType data_type,
                                            uint8_t data_size)
{
  return emberAfWriteManufacturerSpecificServerAttribute(CONFIG_ENDPOINT,
                                                         ZCL_BASIC_CLUSTER_ID,
                                                         attribute_id,
                                                         APP_MANUFACTURER_CODE,
                                                         data,
                                                         data_type);
}

EmberAfStatus app_config_read_mfg_attribute(EmberAfAttributeId attribute_id,
                                            uint8_t *attribute_type,
                                            uint8_t *value_out,
                                            uint8_t *value_len_io)
{
  if (attribute_type == NULL || value_out == NULL || value_len_io == NULL) {
    return EMBER_ZCL_STATUS_INVALID_FIELD;
  }

  switch (attribute_id) {
    case ZCL_SENSOR_READ_INTERVAL_ATTRIBUTE_ID:
      if (*value_len_io < sizeof(uint16_t)) { return EMBER_ZCL_STATUS_INSUFFICIENT_SPACE; }
      *attribute_type = ZCL_INT16U_ATTRIBUTE_TYPE;
      value_out[0] = (uint8_t)(config.sensor_read_interval_seconds & 0xFFu);
      value_out[1] = (uint8_t)(config.sensor_read_interval_seconds >> 8);
      *value_len_io = 2;
      return EMBER_ZCL_STATUS_SUCCESS;
    case ZCL_TEMPERATURE_OFFSET_ATTRIBUTE_ID:
      if (*value_len_io < sizeof(int16_t)) { return EMBER_ZCL_STATUS_INSUFFICIENT_SPACE; }
      *attribute_type = ZCL_INT16S_ATTRIBUTE_TYPE;
      value_out[0] = (uint8_t)(config.temperature_offset_centidegrees & 0xFF);
      value_out[1] = (uint8_t)(((uint16_t)config.temperature_offset_centidegrees) >> 8);
      *value_len_io = 2;
      return EMBER_ZCL_STATUS_SUCCESS;
    case ZCL_HUMIDITY_OFFSET_ATTRIBUTE_ID:
      if (*value_len_io < sizeof(int16_t)) { return EMBER_ZCL_STATUS_INSUFFICIENT_SPACE; }
      *attribute_type = ZCL_INT16S_ATTRIBUTE_TYPE;
      value_out[0] = (uint8_t)(config.humidity_offset_centipercent & 0xFF);
      value_out[1] = (uint8_t)(((uint16_t)config.humidity_offset_centipercent) >> 8);
      *value_len_io = 2;
      return EMBER_ZCL_STATUS_SUCCESS;
    case ZCL_PRESSURE_OFFSET_ATTRIBUTE_ID:
      if (*value_len_io < sizeof(int16_t)) { return EMBER_ZCL_STATUS_INSUFFICIENT_SPACE; }
      *attribute_type = ZCL_INT16S_ATTRIBUTE_TYPE;
      value_out[0] = (uint8_t)(config.pressure_offset_centikilopascals & 0xFF);
      value_out[1] = (uint8_t)(((uint16_t)config.pressure_offset_centikilopascals) >> 8);
      *value_len_io = 2;
      return EMBER_ZCL_STATUS_SUCCESS;
    case ZCL_LED_ENABLE_ATTRIBUTE_ID:
      if (*value_len_io < sizeof(uint8_t)) { return EMBER_ZCL_STATUS_INSUFFICIENT_SPACE; }
      *attribute_type = ZCL_BOOLEAN_ATTRIBUTE_TYPE;
      value_out[0] = config.led_enable ? 1 : 0;
      *value_len_io = 1;
      return EMBER_ZCL_STATUS_SUCCESS;
    case ZCL_REPORT_THRESHOLD_TEMPERATURE_ATTRIBUTE_ID:
      if (*value_len_io < sizeof(uint16_t)) { return EMBER_ZCL_STATUS_INSUFFICIENT_SPACE; }
      *attribute_type = ZCL_INT16U_ATTRIBUTE_TYPE;
      value_out[0] = (uint8_t)(config.report_threshold_temperature & 0xFFu);
      value_out[1] = (uint8_t)(config.report_threshold_temperature >> 8);
      *value_len_io = 2;
      return EMBER_ZCL_STATUS_SUCCESS;
    case ZCL_REPORT_THRESHOLD_HUMIDITY_ATTRIBUTE_ID:
      if (*value_len_io < sizeof(uint16_t)) { return EMBER_ZCL_STATUS_INSUFFICIENT_SPACE; }
      *attribute_type = ZCL_INT16U_ATTRIBUTE_TYPE;
      value_out[0] = (uint8_t)(config.report_threshold_humidity & 0xFFu);
      value_out[1] = (uint8_t)(config.report_threshold_humidity >> 8);
      *value_len_io = 2;
      return EMBER_ZCL_STATUS_SUCCESS;
    case ZCL_REPORT_THRESHOLD_PRESSURE_ATTRIBUTE_ID:
      if (*value_len_io < sizeof(uint16_t)) { return EMBER_ZCL_STATUS_INSUFFICIENT_SPACE; }
      *attribute_type = ZCL_INT16U_ATTRIBUTE_TYPE;
      value_out[0] = (uint8_t)(config.report_threshold_pressure & 0xFFu);
      value_out[1] = (uint8_t)(config.report_threshold_pressure >> 8);
      *value_len_io = 2;
      return EMBER_ZCL_STATUS_SUCCESS;
    default:
      return EMBER_ZCL_STATUS_UNSUPPORTED_ATTRIBUTE;
  }
}

EmberAfStatus app_config_write_mfg_attribute(EmberAfAttributeId attribute_id,
                                             uint8_t attribute_type,
                                             const uint8_t *value,
                                             uint8_t value_len)
{
  EmberAfStatus status = EMBER_ZCL_STATUS_SUCCESS;
  if (value == NULL) {
    return EMBER_ZCL_STATUS_INVALID_FIELD;
  }

  switch (attribute_id) {
    case ZCL_SENSOR_READ_INTERVAL_ATTRIBUTE_ID: {
      if (attribute_type != ZCL_INT16U_ATTRIBUTE_TYPE || value_len != 2) {
        return EMBER_ZCL_STATUS_INVALID_DATA_TYPE;
      }
      uint16_t interval = (uint16_t)(value[0] | ((uint16_t)value[1] << 8));
      if (interval < 10 || interval > 3600) {
        return EMBER_ZCL_STATUS_INVALID_VALUE;
      }
      config.sensor_read_interval_seconds = interval;
      app_sensor_set_interval((uint32_t)interval * 1000u);
      (void)write_config_attribute(attribute_id, value, ZCL_INT16U_ATTRIBUTE_TYPE, 2);
      break;
    }
    case ZCL_TEMPERATURE_OFFSET_ATTRIBUTE_ID: {
      if (attribute_type != ZCL_INT16S_ATTRIBUTE_TYPE || value_len != 2) {
        return EMBER_ZCL_STATUS_INVALID_DATA_TYPE;
      }
      config.temperature_offset_centidegrees = (int16_t)(value[0] | ((uint16_t)value[1] << 8));
      (void)write_config_attribute(attribute_id, value, ZCL_INT16S_ATTRIBUTE_TYPE, 2);
      break;
    }
    case ZCL_HUMIDITY_OFFSET_ATTRIBUTE_ID: {
      if (attribute_type != ZCL_INT16S_ATTRIBUTE_TYPE || value_len != 2) {
        return EMBER_ZCL_STATUS_INVALID_DATA_TYPE;
      }
      config.humidity_offset_centipercent = (int16_t)(value[0] | ((uint16_t)value[1] << 8));
      (void)write_config_attribute(attribute_id, value, ZCL_INT16S_ATTRIBUTE_TYPE, 2);
      break;
    }
    case ZCL_PRESSURE_OFFSET_ATTRIBUTE_ID: {
      if (attribute_type != ZCL_INT16S_ATTRIBUTE_TYPE || value_len != 2) {
        return EMBER_ZCL_STATUS_INVALID_DATA_TYPE;
      }
      config.pressure_offset_centikilopascals = (int16_t)(value[0] | ((uint16_t)value[1] << 8));
      (void)write_config_attribute(attribute_id, value, ZCL_INT16S_ATTRIBUTE_TYPE, 2);
      break;
    }
    case ZCL_LED_ENABLE_ATTRIBUTE_ID: {
      if (attribute_type != ZCL_BOOLEAN_ATTRIBUTE_TYPE || value_len != 1) {
        return EMBER_ZCL_STATUS_INVALID_DATA_TYPE;
      }
      config.led_enable = (value[0] != 0);
      (void)write_config_attribute(attribute_id, value, ZCL_BOOLEAN_ATTRIBUTE_TYPE, 1);
      break;
    }
    case ZCL_REPORT_THRESHOLD_TEMPERATURE_ATTRIBUTE_ID: {
      if (attribute_type != ZCL_INT16U_ATTRIBUTE_TYPE || value_len != 2) {
        return EMBER_ZCL_STATUS_INVALID_DATA_TYPE;
      }
      config.report_threshold_temperature = (uint16_t)(value[0] | ((uint16_t)value[1] << 8));
      (void)write_config_attribute(attribute_id, value, ZCL_INT16U_ATTRIBUTE_TYPE, 2);
      break;
    }
    case ZCL_REPORT_THRESHOLD_HUMIDITY_ATTRIBUTE_ID: {
      if (attribute_type != ZCL_INT16U_ATTRIBUTE_TYPE || value_len != 2) {
        return EMBER_ZCL_STATUS_INVALID_DATA_TYPE;
      }
      config.report_threshold_humidity = (uint16_t)(value[0] | ((uint16_t)value[1] << 8));
      (void)write_config_attribute(attribute_id, value, ZCL_INT16U_ATTRIBUTE_TYPE, 2);
      break;
    }
    case ZCL_REPORT_THRESHOLD_PRESSURE_ATTRIBUTE_ID: {
      if (attribute_type != ZCL_INT16U_ATTRIBUTE_TYPE || value_len != 2) {
        return EMBER_ZCL_STATUS_INVALID_DATA_TYPE;
      }
      config.report_threshold_pressure = (uint16_t)(value[0] | ((uint16_t)value[1] << 8));
      (void)write_config_attribute(attribute_id, value, ZCL_INT16U_ATTRIBUTE_TYPE, 2);
      break;
    }
    default:
      status = EMBER_ZCL_STATUS_UNSUPPORTED_ATTRIBUTE;
      break;
  }

  return status;
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
      status = emberAfReadManufacturerSpecificServerAttribute(endpoint,
                                                              ZCL_BASIC_CLUSTER_ID,
                                                              ZCL_SENSOR_READ_INTERVAL_ATTRIBUTE_ID,
                                                              APP_MANUFACTURER_CODE,
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
      status = emberAfReadManufacturerSpecificServerAttribute(endpoint,
                                                              ZCL_BASIC_CLUSTER_ID,
                                                              ZCL_TEMPERATURE_OFFSET_ATTRIBUTE_ID,
                                                              APP_MANUFACTURER_CODE,
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
      status = emberAfReadManufacturerSpecificServerAttribute(endpoint,
                                                              ZCL_BASIC_CLUSTER_ID,
                                                              ZCL_HUMIDITY_OFFSET_ATTRIBUTE_ID,
                                                              APP_MANUFACTURER_CODE,
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
      status = emberAfReadManufacturerSpecificServerAttribute(endpoint,
                                                              ZCL_BASIC_CLUSTER_ID,
                                                              ZCL_PRESSURE_OFFSET_ATTRIBUTE_ID,
                                                              APP_MANUFACTURER_CODE,
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
      status = emberAfReadManufacturerSpecificServerAttribute(endpoint,
                                                              ZCL_BASIC_CLUSTER_ID,
                                                              ZCL_LED_ENABLE_ATTRIBUTE_ID,
                                                              APP_MANUFACTURER_CODE,
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
      status = emberAfReadManufacturerSpecificServerAttribute(endpoint,
                                                              ZCL_BASIC_CLUSTER_ID,
                                                              ZCL_REPORT_THRESHOLD_TEMPERATURE_ATTRIBUTE_ID,
                                                              APP_MANUFACTURER_CODE,
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
      status = emberAfReadManufacturerSpecificServerAttribute(endpoint,
                                                              ZCL_BASIC_CLUSTER_ID,
                                                              ZCL_REPORT_THRESHOLD_HUMIDITY_ATTRIBUTE_ID,
                                                              APP_MANUFACTURER_CODE,
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
      status = emberAfReadManufacturerSpecificServerAttribute(endpoint,
                                                              ZCL_BASIC_CLUSTER_ID,
                                                              ZCL_REPORT_THRESHOLD_PRESSURE_ATTRIBUTE_ID,
                                                              APP_MANUFACTURER_CODE,
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
