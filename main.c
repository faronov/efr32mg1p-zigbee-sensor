/**
 * @file main.c
 * @brief Main entry point for Zigbee BME280 Sensor application
 */

#include "app/framework/include/af.h"
#include "sl_system_init.h"
#include "app/framework/util/af-main.h"

int main(void)
{
  // Initialize Silicon Labs system
  sl_system_init();

  // Initialize and run Zigbee application framework
  // This function contains the main event loop and never returns
  emberAfMain();

  return 0;
}
