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

// Sensor update interval in milliseconds
// Updated for 2xAAA battery operation (higher capacity than CR2032)
// Current: 1 minute (60000 ms) - good balance of responsiveness and battery life
// For testing/development: use 30000 (30 seconds)
// For maximum battery life: use 300000-900000 (5-15 minutes)
#define SENSOR_UPDATE_INTERVAL_MS   60000

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
