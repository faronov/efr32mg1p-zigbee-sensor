/**
 * @file app_config.c
 * @brief Configuration attribute handler for OpenBME280 sensor profiles
 *
 * Keeps only one manufacturer-specific Basic attribute:
 * - 0xF000 Sensor Read Interval (seconds)
 */

#include "app_config.h"
#include "app_sensor.h"
#include "af.h"
#include "app/framework/include/af.h"

// Endpoint where configuration attributes are located
#define CONFIG_ENDPOINT 1
#define APP_SENSOR_INTERVAL_MIN_SECONDS 10u
#define APP_SENSOR_INTERVAL_MAX_SECONDS 3600u
#define APP_SENSOR_INTERVAL_DEFAULT_SECONDS 60u

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

static EmberAfStatus write_config_attribute(EmberAfAttributeId attribute_id,
                                            const uint8_t *data,
                                            EmberAfAttributeType data_type)
{
  return emberAfWriteManufacturerSpecificServerAttribute(CONFIG_ENDPOINT,
                                                         ZCL_BASIC_CLUSTER_ID,
                                                         attribute_id,
                                                         APP_MANUFACTURER_CODE,
                                                         data,
                                                         data_type);
}

void app_config_init(void)
{
  EmberAfStatus status;
  uint16_t interval = APP_SENSOR_INTERVAL_DEFAULT_SECONDS;

  status = read_config_attribute(ZCL_SENSOR_READ_INTERVAL_ATTRIBUTE_ID,
                                 (uint8_t *)&interval,
                                 sizeof(interval));
  if (status != EMBER_ZCL_STATUS_SUCCESS
      || interval < APP_SENSOR_INTERVAL_MIN_SECONDS
      || interval > APP_SENSOR_INTERVAL_MAX_SECONDS) {
    interval = APP_SENSOR_INTERVAL_DEFAULT_SECONDS;
  }

  config.sensor_read_interval_seconds = interval;

  emberAfCorePrintln("Config loaded:");
  emberAfCorePrintln("  Read interval: %d seconds", config.sensor_read_interval_seconds);
}

const app_config_t *app_config_get(void)
{
  return &config;
}

EmberAfStatus app_config_read_mfg_attribute(EmberAfAttributeId attribute_id,
                                            uint8_t *attribute_type,
                                            uint8_t *value_out,
                                            uint8_t *value_len_io)
{
  if (attribute_type == NULL || value_out == NULL || value_len_io == NULL) {
    return EMBER_ZCL_STATUS_INVALID_FIELD;
  }

  if (attribute_id != ZCL_SENSOR_READ_INTERVAL_ATTRIBUTE_ID) {
    return EMBER_ZCL_STATUS_UNSUPPORTED_ATTRIBUTE;
  }

  if (*value_len_io < sizeof(uint16_t)) {
    return EMBER_ZCL_STATUS_INSUFFICIENT_SPACE;
  }

  *attribute_type = ZCL_INT16U_ATTRIBUTE_TYPE;
  value_out[0] = (uint8_t)(config.sensor_read_interval_seconds & 0xFFu);
  value_out[1] = (uint8_t)(config.sensor_read_interval_seconds >> 8);
  *value_len_io = 2;
  return EMBER_ZCL_STATUS_SUCCESS;
}

EmberAfStatus app_config_write_mfg_attribute(EmberAfAttributeId attribute_id,
                                             uint8_t attribute_type,
                                             const uint8_t *value,
                                             uint8_t value_len)
{
  if (value == NULL) {
    return EMBER_ZCL_STATUS_INVALID_FIELD;
  }

  if (attribute_id != ZCL_SENSOR_READ_INTERVAL_ATTRIBUTE_ID) {
    return EMBER_ZCL_STATUS_UNSUPPORTED_ATTRIBUTE;
  }

  if (attribute_type != ZCL_INT16U_ATTRIBUTE_TYPE || value_len != 2) {
    return EMBER_ZCL_STATUS_INVALID_DATA_TYPE;
  }

  uint16_t interval = (uint16_t)(value[0] | ((uint16_t)value[1] << 8));
  if (interval < APP_SENSOR_INTERVAL_MIN_SECONDS
      || interval > APP_SENSOR_INTERVAL_MAX_SECONDS) {
    return EMBER_ZCL_STATUS_INVALID_VALUE;
  }

  config.sensor_read_interval_seconds = interval;
  app_sensor_set_interval((uint32_t)interval * 1000u);
  (void)write_config_attribute(attribute_id, value, ZCL_INT16U_ATTRIBUTE_TYPE);
  return EMBER_ZCL_STATUS_SUCCESS;
}

/**
 * @brief Callback when Basic cluster attributes are written
 */
void emberAfBasicClusterServerAttributeChangedCallback(uint8_t endpoint,
                                                       EmberAfAttributeId attributeId)
{
  if (attributeId != ZCL_SENSOR_READ_INTERVAL_ATTRIBUTE_ID) {
    return;
  }

  uint16_t interval = APP_SENSOR_INTERVAL_DEFAULT_SECONDS;
  EmberAfStatus status = emberAfReadManufacturerSpecificServerAttribute(endpoint,
                                                                        ZCL_BASIC_CLUSTER_ID,
                                                                        ZCL_SENSOR_READ_INTERVAL_ATTRIBUTE_ID,
                                                                        APP_MANUFACTURER_CODE,
                                                                        (uint8_t *)&interval,
                                                                        sizeof(interval));
  if (status == EMBER_ZCL_STATUS_SUCCESS
      && interval >= APP_SENSOR_INTERVAL_MIN_SECONDS
      && interval <= APP_SENSOR_INTERVAL_MAX_SECONDS) {
    config.sensor_read_interval_seconds = interval;
    emberAfCorePrintln("Sensor read interval changed to %d seconds", interval);
    app_sensor_set_interval((uint32_t)interval * 1000u);
  }
}
