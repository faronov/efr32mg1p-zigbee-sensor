/**
 * @file main.c
 * @brief Main entry point for Zigbee BME280 Sensor application
 */

#include "sl_component_catalog.h"
#include "sl_system_init.h"
#include "sl_event_handler.h"
#include <stdio.h>

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
#include "sl_power_manager.h"
#endif

#if defined(SL_CATALOG_KERNEL_PRESENT)
#include "sl_system_kernel.h"
#else
void sl_system_process_action(void);
#endif

int main(void)
{
  // Initialize Silicon Labs system
  sl_system_init();

  // Early SWO sanity print (debug builds should show this).
  printf("SWO OK: main start\n");

#if defined(SL_CATALOG_KERNEL_PRESENT)
  // Start kernel
  sl_system_kernel_start();
#else
  // Bare metal main loop
  while (1) {
    // Run event handlers
    sl_system_process_action();

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
    // Sleep until next event
    sl_power_manager_sleep();
#endif
  }
#endif

  return 0;
}
