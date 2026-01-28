/**
 * @file main.c
 * @brief Main entry point for Zigbee BME280 Sensor application
 */

#include "app/framework/include/af.h"

int main(void)
{
  // Initialize Zigbee stack
  emberAfMainInit();

  // Main event loop
  while (true) {
    emberAfMainTick();
  }
}
