/**
 * @file main.c
 * @brief Main entry point for Zigbee BME280 Sensor application
 */

#include "sl_component_catalog.h"
#include "sl_system_init.h"
#include "sl_event_handler.h"
#include "sl_sleeptimer.h"
#include <stdio.h>

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
#include "sl_power_manager.h"
#endif

#if defined(SL_CATALOG_KERNEL_PRESENT)
#include "sl_system_kernel.h"
#else
void sl_system_process_action(void);
#endif

#ifndef APP_DEBUG_DIAG_ALWAYS
#define APP_DEBUG_DIAG_ALWAYS 0
#endif
#ifndef APP_DEBUG_NO_SLEEP
#define APP_DEBUG_NO_SLEEP 0
#endif
#ifndef APP_DEBUG_MAIN_HEARTBEAT
#define APP_DEBUG_MAIN_HEARTBEAT 0
#endif
#ifndef APP_DEBUG_POLL_BUTTON
#define APP_DEBUG_POLL_BUTTON 0
#endif

#if defined(SL_CATALOG_SIMPLE_BUTTON_PRESENT) && defined(APP_DEBUG_POLL_BUTTON) && (APP_DEBUG_POLL_BUTTON != 0)
#include "sl_simple_button_instances.h"
#endif

int main(void)
{
  // Initialize Silicon Labs system
  sl_system_init();

  // Early SWO sanity print (debug builds should show this).
  printf("SWO OK: main start\n");

#if APP_DEBUG_DIAG_ALWAYS
  printf("Debug flags: NO_SLEEP=%d HEARTBEAT=%d POLL_BUTTON=%d\n",
         APP_DEBUG_NO_SLEEP,
         APP_DEBUG_MAIN_HEARTBEAT,
         APP_DEBUG_POLL_BUTTON);
#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
  printf("Power manager: present\n");
#else
  printf("Power manager: absent\n");
#endif
#endif

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT) && defined(APP_DEBUG_NO_SLEEP) && (APP_DEBUG_NO_SLEEP != 0)
  // Keep the CPU in EM0 for debug so SWO and logs are reliable.
  sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM0);
  printf("Debug: sleep disabled (EM0 requirement)\n");
#endif

#if defined(SL_CATALOG_KERNEL_PRESENT)
#if APP_DEBUG_DIAG_ALWAYS
  printf("Kernel present; starting kernel\n");
#endif
  // Start kernel
  sl_system_kernel_start();
#else
#if APP_DEBUG_DIAG_ALWAYS
  printf("Bare metal loop start\n");
#endif
  // Bare metal main loop
  while (1) {
    // Run event handlers
    sl_system_process_action();

#if defined(APP_DEBUG_MAIN_HEARTBEAT) && (APP_DEBUG_MAIN_HEARTBEAT != 0)
    // Throttled main-loop heartbeat for SWO debugging.
    static uint32_t last_heartbeat_tick = 0;
    uint32_t now = sl_sleeptimer_get_tick_count();
    if (last_heartbeat_tick == 0 ||
        sl_sleeptimer_tick_to_ms(now - last_heartbeat_tick) >= 2000) {
      last_heartbeat_tick = now;
      printf("Main loop heartbeat\n");
    }
#endif

#if defined(SL_CATALOG_SIMPLE_BUTTON_PRESENT) && defined(APP_DEBUG_POLL_BUTTON) && (APP_DEBUG_POLL_BUTTON != 0)
    // Simple polling to confirm BTN0 is wired and readable in debug.
    static sl_button_state_t last_state = SL_SIMPLE_BUTTON_RELEASED;
    sl_button_state_t state = sl_button_get_state(&sl_button_btn0);
    if (state != last_state) {
      last_state = state;
      printf("BTN0: %s\n",
             (state == SL_SIMPLE_BUTTON_PRESSED) ? "PRESSED" : "RELEASED");
    }
#endif

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
    // Sleep until next event
    sl_power_manager_sleep();
#endif
  }
#endif

  return 0;
}
