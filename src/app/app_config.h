/**
 * @file app_config.h
 * @brief Configuration attribute handler for BME280 sensor
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "af.h"

// Manufacturer code for custom configuration attributes
#define APP_MANUFACTURER_CODE 0x1002u

// Manufacturer-specific Basic cluster attributes (0xF000 range)
#define ZCL_SENSOR_READ_INTERVAL_ATTRIBUTE_ID 0xF000  // uint16, seconds

/**
 * @brief Configuration structure holding all customizable parameters
 */
typedef struct {
  // Sensor reading interval (10-3600 seconds)
  uint16_t sensor_read_interval_seconds;
} app_config_t;

/**
 * @brief Initialize configuration system and load values from NVM
 *
 * Call this once during application initialization, after Zigbee stack is ready.
 */
void app_config_init(void);

/**
 * @brief Get pointer to current configuration
 *
 * @return Pointer to configuration structure (read-only)
 */
const app_config_t* app_config_get(void);

/**
 * @brief Read manufacturer-specific Basic attribute from runtime config.
 *
 * @param attribute_id Basic cluster attribute id (0xF0xx)
 * @param attribute_type Output Zigbee type id
 * @param value_out Output value bytes (little-endian)
 * @param value_len_io Input: max buffer len, Output: actual len
 * @return EMBER_ZCL_STATUS_SUCCESS on success or ZCL error status
 */
EmberAfStatus app_config_read_mfg_attribute(EmberAfAttributeId attribute_id,
                                            uint8_t *attribute_type,
                                            uint8_t *value_out,
                                            uint8_t *value_len_io);

/**
 * @brief Write manufacturer-specific Basic attribute into runtime config.
 *
 * Applies runtime side-effects (sensor interval, etc.) and best-effort mirrors
 * to ZCL/NVM if attribute metadata is present.
 */
EmberAfStatus app_config_write_mfg_attribute(EmberAfAttributeId attribute_id,
                                             uint8_t attribute_type,
                                             const uint8_t *value,
                                             uint8_t value_len);

#endif // APP_CONFIG_H
