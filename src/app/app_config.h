/**
 * @file app_config.h
 * @brief Configuration attribute handler for BME280 sensor
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Configuration structure holding all customizable parameters
 */
typedef struct {
  // Sensor reading interval (10-3600 seconds)
  uint16_t sensor_read_interval_seconds;

  // Calibration offsets in 0.01 units
  int16_t temperature_offset_centidegrees;    // Range: -500 to +500 (± 5.0°C)
  int16_t humidity_offset_centipercent;       // Range: -1000 to +1000 (± 10%)
  int16_t pressure_offset_centikilopascals;   // Range: -500 to +500 (± 5.0 kPa)

  // LED control
  bool led_enable;

  // Report thresholds in 0.01 units
  uint16_t report_threshold_temperature;      // Default: 100 (1.0°C)
  uint16_t report_threshold_humidity;         // Default: 100 (1.0%)
  uint16_t report_threshold_pressure;         // Default: 1 (0.01 kPa)
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

#endif // APP_CONFIG_H
