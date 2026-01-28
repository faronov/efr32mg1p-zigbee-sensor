/**
 * @file app.c
 * @brief Application logic for Zigbee BME280 Sensor
 */

#include "app/framework/include/af.h"
#include "app_sensor.h"

/**
 * @brief Zigbee stack initialization callback
 *
 * Called when the Zigbee stack has completed initialization.
 * This is the ideal place to initialize application components.
 */
void emberAfMainInitCallback(void)
{
  emberAfCorePrintln("Zigbee BME280 Sensor Application");
  emberAfCorePrintln("Silicon Labs EFR32MG1P + Bosch BME280");

  // Initialize BME280 sensor
  if (!app_sensor_init()) {
    emberAfCorePrintln("ERROR: Sensor initialization failed!");
  }
}

/**
 * @brief Network join success callback
 *
 * Called when the device successfully joins a Zigbee network.
 */
void emberAfStackStatusCallback(EmberStatus status)
{
  if (status == EMBER_NETWORK_UP) {
    emberAfCorePrintln("Network joined successfully");

    // Perform initial sensor reading after joining
    app_sensor_update();
  } else if (status == EMBER_NETWORK_DOWN) {
    emberAfCorePrintln("Network down - attempting to rejoin");
  }
}
