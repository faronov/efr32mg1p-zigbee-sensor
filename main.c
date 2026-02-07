/**
 * @file main.c
 * @brief Main entry point for Zigbee BME280 Sensor application
 */

#include "sl_component_catalog.h"
#include "sl_system_init.h"
#include "sl_event_handler.h"
#include "sl_sleeptimer.h"
#include <stdio.h>
#include <stdint.h>
#include "app/framework/include/af.h"
#include "hal.h"
#include "micro.h"
#include "cortexm3/diagnostic.h"

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
void app_debug_trigger_long_press(void);
void app_debug_force_af_init(void);
void app_debug_poll(void);
bool app_debug_button_ready(void);

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
#ifndef APP_DEBUG_CRASH_PRINT
#define APP_DEBUG_CRASH_PRINT 0
#endif
#ifndef APP_DEBUG_BOOT_DELAY_MS
#define APP_DEBUG_BOOT_DELAY_MS 0
#endif
#ifndef APP_DEBUG_BOOT_SPAM_MS
#define APP_DEBUG_BOOT_SPAM_MS 0
#endif
#ifndef APP_DEBUG_BOOT_SPAM_PERIOD_MS
#define APP_DEBUG_BOOT_SPAM_PERIOD_MS 1000
#endif
#ifndef APP_DEBUG_LONG_PRESS_MS
#define APP_DEBUG_LONG_PRESS_MS 5000
#endif
#ifndef APP_DEBUG_SHORT_FALLBACK_MS
#define APP_DEBUG_SHORT_FALLBACK_MS 1200
#endif
#ifndef APP_BUILD_TAG
#define APP_BUILD_TAG "unknown"
#endif

#if defined(SL_CATALOG_SIMPLE_BUTTON_PRESENT) && defined(APP_DEBUG_POLL_BUTTON) && (APP_DEBUG_POLL_BUTTON != 0)
#include "sl_simple_button_instances.h"
#endif

static uint8_t app_reset_info = 0;
static bool app_reset_info_valid = false;
static bool app_crash_print_pending = false;
static uint32_t app_boot_tick_start = 0;
static uint32_t app_boot_tick_last = 0;

static void app_debug_record_reset_info(void)
{
  uint8_t reset = halGetResetInfo();
  app_reset_info = reset;
  app_reset_info_valid = true;
  app_crash_print_pending =
    ((RESET_CRASH_REASON_MASK & (1u << reset)) != 0u);
}

static void app_debug_boot_tick(void)
{
#if APP_DEBUG_BOOT_SPAM_MS
  uint32_t now = sl_sleeptimer_get_tick_count();
  if (app_boot_tick_start == 0) {
    app_boot_tick_start = now;
  }
  if (app_crash_print_pending && app_reset_info_valid) {
    printf("Debug: crash reset info=0x%02x (%s)\n",
           app_reset_info,
           halGetResetString());
    halPrintCrashSummary(0);
    halPrintCrashDetails(0);
    app_crash_print_pending = false;
  }
  if (sl_sleeptimer_tick_to_ms(now - app_boot_tick_start) < APP_DEBUG_BOOT_SPAM_MS) {
    if (app_boot_tick_last == 0
        || sl_sleeptimer_tick_to_ms(now - app_boot_tick_last) >= APP_DEBUG_BOOT_SPAM_PERIOD_MS) {
      app_boot_tick_last = now;
      printf("BOOT: tag=%s uptime=%lu ms\n",
             APP_BUILD_TAG,
             (unsigned long)sl_sleeptimer_tick_to_ms(now - app_boot_tick_start));
    }
  }
#endif
}

int main(void)
{
  // Initialize Silicon Labs system
  sl_system_init();

#if APP_DEBUG_CRASH_PRINT || APP_DEBUG_BOOT_SPAM_MS
  app_debug_record_reset_info();
#endif

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
#if APP_DEBUG_CRASH_PRINT || APP_DEBUG_BOOT_SPAM_MS
    app_debug_boot_tick();
#endif
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
    // Poll raw BTN0 pin (PB13, active low) to avoid simple-button glitches on TRADFRI.
    static bool btn_pressed = false;
    static uint32_t press_tick = 0;
    static uint32_t debounce_tick = 0;
    static bool press_action_done = false;
    if (!app_debug_button_ready()) {
      btn_pressed = false;
      press_tick = 0;
      debounce_tick = 0;
      press_action_done = false;
    } else {
      bool raw_pressed = (GPIO_PinInGet(gpioPortB, 13) == 0);

      if (raw_pressed != btn_pressed) {
        uint32_t now_tick = sl_sleeptimer_get_tick_count();
        if (debounce_tick == 0) {
          debounce_tick = now_tick;
        }
        if (sl_sleeptimer_tick_to_ms(now_tick - debounce_tick) >= 30) {
          btn_pressed = raw_pressed;
          debounce_tick = 0;
          printf("BTN0: %s\n", btn_pressed ? "PRESSED" : "RELEASED");
          if (btn_pressed) {
            press_tick = now_tick;
            press_action_done = false;
          } else if (press_tick != 0) {
            uint32_t held_ms = sl_sleeptimer_tick_to_ms(now_tick - press_tick);
            press_tick = 0;
            if (!press_action_done) {
              if (held_ms >= APP_DEBUG_LONG_PRESS_MS) {
                app_debug_trigger_long_press();
              } else {
                app_debug_trigger_short_press();
              }
            }
            press_action_done = false;
          }
        }
      } else {
        debounce_tick = 0;
      }

      // Fallback: if release edge is missed, synthesize action by hold time.
      if (btn_pressed && press_tick != 0 && !press_action_done) {
        uint32_t held_ms = sl_sleeptimer_tick_to_ms(sl_sleeptimer_get_tick_count() - press_tick);
        if (held_ms >= APP_DEBUG_LONG_PRESS_MS) {
          printf("BTN0: fallback LONG (no release edge)\n");
          app_debug_trigger_long_press();
          press_action_done = true;
        } else if (held_ms >= APP_DEBUG_SHORT_FALLBACK_MS) {
          printf("BTN0: fallback SHORT (no release edge)\n");
          app_debug_trigger_short_press();
          press_action_done = true;
        }
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
