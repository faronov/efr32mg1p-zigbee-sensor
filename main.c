/**
 * @file main.c
 * @brief Main entry point for Zigbee BME280 Sensor application
 */

#include "sl_component_catalog.h"
#include "sl_system_init.h"
#include "sl_event_handler.h"
#include "sl_sleeptimer.h"
#include "em_device.h"
#include <stdio.h>
#include <stdint.h>
#include "app/framework/include/af.h"

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
#include "sl_power_manager.h"
#endif

#if defined(SL_CATALOG_KERNEL_PRESENT)
#include "sl_system_kernel.h"
#else
void sl_system_process_action(void);
#endif

void app_debug_sanity(void);
void app_debug_trigger_short_press(void);
void app_debug_force_af_init(void);
void app_debug_poll(void);

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
#ifndef APP_DEBUG_FORCE_AF_INIT
#define APP_DEBUG_FORCE_AF_INIT 0
#endif
#ifndef APP_DEBUG_FAULT_DUMP
#define APP_DEBUG_FAULT_DUMP 0
#endif
#ifndef APP_DEBUG_BOOT_DELAY_MS
#define APP_DEBUG_BOOT_DELAY_MS 0
#endif
#ifndef APP_BUILD_TAG
#define APP_BUILD_TAG "unknown"
#endif

#if defined(SL_CATALOG_SIMPLE_BUTTON_PRESENT) && defined(APP_DEBUG_POLL_BUTTON) && (APP_DEBUG_POLL_BUTTON != 0)
#include "sl_simple_button_instances.h"
#endif

#if APP_DEBUG_FAULT_DUMP
static void app_fault_dump(const char *name, uint32_t *stacked)
{
  uint32_t r0 = stacked[0];
  uint32_t r1 = stacked[1];
  uint32_t r2 = stacked[2];
  uint32_t r3 = stacked[3];
  uint32_t r12 = stacked[4];
  uint32_t lr = stacked[5];
  uint32_t pc = stacked[6];
  uint32_t psr = stacked[7];

  printf("FAULT: %s\n", name);
  printf("R0=0x%08lx R1=0x%08lx R2=0x%08lx R3=0x%08lx\n",
         (unsigned long)r0, (unsigned long)r1,
         (unsigned long)r2, (unsigned long)r3);
  printf("R12=0x%08lx LR=0x%08lx PC=0x%08lx PSR=0x%08lx\n",
         (unsigned long)r12, (unsigned long)lr,
         (unsigned long)pc, (unsigned long)psr);
  printf("CFSR=0x%08lx HFSR=0x%08lx BFAR=0x%08lx MMFAR=0x%08lx\n",
         (unsigned long)SCB->CFSR,
         (unsigned long)SCB->HFSR,
         (unsigned long)SCB->BFAR,
         (unsigned long)SCB->MMFAR);
  while (1) {
    // Halt so the fault stays visible in SWO.
  }
}

static void app_hardfault_c(uint32_t *stacked)
{
  app_fault_dump("HardFault", stacked);
}

static void app_busfault_c(uint32_t *stacked)
{
  app_fault_dump("BusFault", stacked);
}

__attribute__((naked)) void HardFault_Handler(void)
{
  __asm volatile(
    "tst lr, #4\n"
    "ite eq\n"
    "mrseq r0, msp\n"
    "mrsne r0, psp\n"
    "b app_hardfault_c\n"
  );
}

__attribute__((naked)) void BusFault_Handler(void)
{
  __asm volatile(
    "tst lr, #4\n"
    "ite eq\n"
    "mrseq r0, msp\n"
    "mrsne r0, psp\n"
    "b app_busfault_c\n"
  );
}
#endif

int main(void)
{
  // Initialize Silicon Labs system
  sl_system_init();

#if APP_DEBUG_BOOT_DELAY_MS
  // Give time to attach SWO before any logs.
  sl_sleeptimer_delay_millisecond(APP_DEBUG_BOOT_DELAY_MS);
#endif

#if APP_DEBUG_FORCE_AF_INIT
  // Force AF init in debug builds in case the framework init isn't wired.
  // Some SDK variants don't expose init level enums/macros; default to "DONE".
#ifndef EMBER_AF_INIT_LEVEL_DONE
#define EMBER_AF_INIT_LEVEL_DONE 0xFFu
#endif
  uint8_t init_level = (uint8_t)EMBER_AF_INIT_LEVEL_DONE;
  emberAfInit(init_level);
  printf("Debug: forced emberAfInit level=%u\n", (unsigned)init_level);
  app_debug_force_af_init();
#endif

  // Early SWO sanity print (debug builds should show this).
#if APP_DEBUG_BOOT_DELAY_MS
  printf("SWO OK: main start (delay=%lu ms, tag=%s)\n",
         (unsigned long)APP_DEBUG_BOOT_DELAY_MS,
         APP_BUILD_TAG);
#else
  printf("SWO OK: main start (tag=%s)\n", APP_BUILD_TAG);
#endif

#if APP_DEBUG_DIAG_ALWAYS
  printf("Debug flags: NO_SLEEP=%d HEARTBEAT=%d POLL_BUTTON=%d\n",
         APP_DEBUG_NO_SLEEP,
         APP_DEBUG_MAIN_HEARTBEAT,
         APP_DEBUG_POLL_BUTTON);
#ifdef EMBER_AF_PLUGIN_OTA_CLIENT_AUTO_START
  printf("Debug: OTA auto-start=%d\n", EMBER_AF_PLUGIN_OTA_CLIENT_AUTO_START);
#endif
#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
  printf("Power manager: present\n");
#else
  printf("Power manager: absent\n");
#endif

#if APP_DEBUG_DIAG_ALWAYS
  app_debug_sanity();
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
      if (state == SL_SIMPLE_BUTTON_PRESSED) {
        app_debug_trigger_short_press();
      }
    }
#endif

#if APP_DEBUG_DIAG_ALWAYS || APP_DEBUG_FORCE_AF_INIT
    // Ensure debug AF init and identity checks run even if AF tick isn't wired.
    app_debug_poll();
#endif

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
    // Sleep until next event
    sl_power_manager_sleep();
#endif
  }
#endif

  return 0;
}
