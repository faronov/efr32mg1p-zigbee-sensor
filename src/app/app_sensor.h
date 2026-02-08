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

/**
 * @brief Start (or restart) periodic sensor updates
 *
 * Schedules the periodic sensor update event. This should be called
 * when the network comes back up to resume periodic updates after
 * they were suspended due to network unavailability.
 */
void app_sensor_start_periodic_updates(void);

/**
 * @brief Set sensor reading interval
 *
 * Updates the periodic sensor update interval. The new interval takes
 * effect on the next scheduled update.
 *
 * @param interval_ms New interval in milliseconds (must be >= 10000)
 */
void app_sensor_set_interval(uint32_t interval_ms);

/**
 * @brief Process deferred sensor timer work in main context.
 *
 * Called from the main loop. Executes pending periodic sensor updates
 * scheduled by sleeptimer callback (which runs in interrupt context).
 */
void app_sensor_process(void);

/**
 * @brief Check whether sensor stack is initialized and ready.
 *
 * @return true if sensor init succeeded and reads can be performed.
 */
bool app_sensor_is_ready(void);

#endif // APP_SENSOR_H
