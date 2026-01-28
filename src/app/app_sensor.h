/**
 * @file app_sensor.h
 * @brief BME280 sensor integration for Zigbee application
 *
 * Manages periodic sensor readings and updates Zigbee cluster attributes.
 */

#ifndef APP_SENSOR_H
#define APP_SENSOR_H

#include <stdint.h>
#include <stdbool.h>

// Sensor update interval in milliseconds (30 seconds)
#define SENSOR_UPDATE_INTERVAL_MS   30000

/**
 * @brief Initialize sensor integration
 *
 * Initializes the BME280 sensor and sets up periodic timer
 * for sensor readings.
 *
 * @return true if successful, false otherwise
 */
bool app_sensor_init(void);

/**
 * @brief Periodic sensor update callback
 *
 * Called by the Zigbee stack's event mechanism to read sensor
 * data and update ZCL attributes.
 *
 * This function should be called from the main event loop or
 * registered with the Zigbee event system.
 */
void app_sensor_update(void);

#endif // APP_SENSOR_H
