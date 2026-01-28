/**
 * @file main.c
 * @brief Main entry point for Zigbee BME280 Sensor application
 */

#include "sl_component_catalog.h"
#include "sl_system_init.h"
#include "app.h"
#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
#include "sl_power_manager.h"
#endif
#include "sl_event_handler.h"

int main(void)
{
  // Initialize Silicon Labs system
  sl_system_init();

  // Initialize application
  app_init();

  // Main event loop
  while (1) {
#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
    // Power manager service
    sl_power_manager_sleep();
#else
    // No power manager - call event handler
    sl_zigbee_common_rtos_idle_handler();
#endif
  }
}
