/**
 * @file app.c
 * @brief Application logic for Zigbee BME280 Sensor
 */

#include "sl_component_catalog.h"
#include "zigbee_app_framework_event.h"
#include "app/framework/include/af.h"
#ifdef SL_CATALOG_ZIGBEE_NETWORK_STEERING_PRESENT
#include "app/framework/plugin/network-steering/network-steering.h"
#endif
#ifdef SL_CATALOG_ZIGBEE_REPORTING_PRESENT
#include "app/framework/plugin/reporting/reporting.h"
#endif
#include "app_profile.h"
#include "app_sensor.h"
#include "app_config.h"
#include "stack/include/network-formation.h"  // For manual network join
#include "stack/include/security.h"
#include "stack/include/binding-table.h"
#include "stack/include/child.h"
#include "hal.h"
#include "sl_sleeptimer.h"
#include "em_cmu.h"
#include "em_gpio.h"
#include "sl_spidrv_instances.h"
#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
#include "sl_power_manager.h"
#endif
#include <stdio.h>
#include <string.h>

// Optional SPIDRV init symbols (generated when SPIDRV component is present).
__attribute__((weak)) void sl_spidrv_exp_init(void);
__attribute__((weak)) void sl_spidrv_init_instances(void);

#ifdef SL_CATALOG_SIMPLE_BUTTON_PRESENT
#include "sl_simple_button_instances.h"
#endif

#ifdef SL_CATALOG_SIMPLE_LED_PRESENT
#include "sl_simple_led_instances.h"
#endif

// LED blink event for network joining indication
static sl_zigbee_event_t led_blink_event;
static bool led_blink_active = false;

// LED off timer - turn off LED after network join confirmation
static sl_zigbee_event_t led_off_event;

static uint8_t join_attempt_count = 0;
#define JOIN_SCAN_DURATION 3  // Active scan duration (Zigbee scan exponent)

// Button action flags (set by ISR, cleared by main loop)
// Using volatile because these are accessed from both ISR and main context
static volatile bool button_short_press_pending = false;
static volatile bool button_long_press_pending = false;

// Button press duration tracking
static uint32_t button_press_start_tick = 0;
static bool button_pressed = false;
#define BUTTON_DEBOUNCE_MS 80
#ifndef APP_BUTTON_LONG_PRESS_MS
#define APP_BUTTON_LONG_PRESS_MS 5000  // default: 5 seconds for long press
#endif

// Optimized rejoin state machine (TEMPORARILY DISABLED - event queue issue)
// typedef enum {
//   REJOIN_STATE_IDLE,
//   REJOIN_STATE_CURRENT_CHANNEL,
//   REJOIN_STATE_ALL_CHANNELS,
//   REJOIN_STATE_DONE
// } RejoinState_t;

// static sl_zigbee_event_t rejoin_retry_event;
// static RejoinState_t rejoin_state = REJOIN_STATE_IDLE;
// static uint8_t saved_channel = 0;

// Rejoin timeout configuration
#define REJOIN_CURRENT_CHANNEL_TIMEOUT_MS  500   // Wait 500ms before trying all channels
#define REJOIN_FULL_SCAN_TIMEOUT_MS        5000  // Wait 5s for full scan to complete

// Channel mask helper macro
#ifndef BIT32
#define BIT32(n) (((uint32_t)1) << (n))
#endif

#if defined(__GNUC__)
#define APP_UNUSED __attribute__((unused))
#else
#define APP_UNUSED
#endif

#ifndef APP_DEBUG_DIAG_ALWAYS
#define APP_DEBUG_DIAG_ALWAYS 0
#endif
#ifndef APP_DEBUG_SPI_ONLY
#define APP_DEBUG_SPI_ONLY 0
#endif
#ifndef APP_DEBUG_RESET_NETWORK
#define APP_DEBUG_RESET_NETWORK 0
#endif
#ifndef APP_DEBUG_AWAKE_AFTER_JOIN_MS
#define APP_DEBUG_AWAKE_AFTER_JOIN_MS 0
#endif
#ifndef APP_DEBUG_FAST_POLL_AFTER_JOIN_MS
#define APP_DEBUG_FAST_POLL_AFTER_JOIN_MS 0
#endif
#ifndef APP_DEBUG_FAST_POLL_INTERVAL_MS
#define APP_DEBUG_FAST_POLL_INTERVAL_MS 250
#endif
#ifndef APP_DEBUG_MANUAL_POLL_BOOST_MS
#define APP_DEBUG_MANUAL_POLL_BOOST_MS 60000
#endif
#ifndef APP_DEBUG_MANUAL_POLL_INTERVAL_MS
#define APP_DEBUG_MANUAL_POLL_INTERVAL_MS 250
#endif
#ifndef APP_DEBUG_BUTTON_GUARD_AFTER_JOIN_MS
#define APP_DEBUG_BUTTON_GUARD_AFTER_JOIN_MS 30000
#endif
#ifndef APP_DEBUG_BUTTON_GUARD_AFTER_BOOT_MS
#define APP_DEBUG_BUTTON_GUARD_AFTER_BOOT_MS 1500
#endif
#ifndef APP_DEBUG_BUTTON_GUARD_AFTER_LEAVE_MS
#define APP_DEBUG_BUTTON_GUARD_AFTER_LEAVE_MS 5000
#endif
#ifndef APP_DEBUG_JOIN_RETRY_BACKOFF_MS
#define APP_DEBUG_JOIN_RETRY_BACKOFF_MS 2000
#endif
#ifndef APP_DEBUG_JOIN_RETRY_BACKOFF_AFTER_LEAVE_MS
#define APP_DEBUG_JOIN_RETRY_BACKOFF_AFTER_LEAVE_MS 800
#endif
#ifndef APP_DEBUG_BUTTON_MAX_VALID_PRESS_MS
#define APP_DEBUG_BUTTON_MAX_VALID_PRESS_MS 30000
#endif
#ifndef APP_DEBUG_POLL_SIMPLE_BUTTON_INSTANCES
#define APP_DEBUG_POLL_SIMPLE_BUTTON_INSTANCES 0
#endif
#ifndef APP_DEBUG_AUTO_JOIN_ON_BOOT
#define APP_DEBUG_AUTO_JOIN_ON_BOOT 1
#endif
#ifndef APP_DEBUG_AUTO_JOIN_ON_PIN_RESET
#define APP_DEBUG_AUTO_JOIN_ON_PIN_RESET 0
#endif
#ifndef APP_DEBUG_AUTO_JOIN_DELAY_MS
#define APP_DEBUG_AUTO_JOIN_DELAY_MS 5000
#endif
#ifndef APP_DEBUG_JOIN_AS_END_DEVICE
#define APP_DEBUG_JOIN_AS_END_DEVICE 0
#endif
#ifndef APP_DEBUG_SET_KEEPALIVE_ALL
#define APP_DEBUG_SET_KEEPALIVE_ALL 0
#endif
#ifndef APP_DEBUG_NO_SLEEP
#define APP_DEBUG_NO_SLEEP 0
#endif
#ifndef APP_DEBUG_USE_NETWORK_STEERING
#define APP_DEBUG_USE_NETWORK_STEERING 1
#endif
#define APP_DEBUG_PRINTF(...) printf(__VA_ARGS__)

#if defined(SL_CATALOG_ZIGBEE_NETWORK_STEERING_PRESENT) && (APP_DEBUG_USE_NETWORK_STEERING != 0)
#define APP_RUNTIME_NETWORK_STEERING 1
#else
#define APP_RUNTIME_NETWORK_STEERING 0
#endif

static void handle_short_press(void);
static void handle_long_press(void);

void app_debug_sanity(void)
{
  APP_DEBUG_PRINTF("app_debug_sanity\n");
#if APP_DEBUG_DIAG_ALWAYS
  uint8_t endpoint_count = emberAfEndpointCount();
  APP_DEBUG_PRINTF("Debug: endpoint count=%u primary=%u\n",
                   endpoint_count,
                   emberAfPrimaryEndpoint());
  bool ota_client_present = false;
  for (uint8_t i = 0; i < endpoint_count; i++) {
    uint8_t ep = emberAfEndpointFromIndex(i);
    bool has_ota = emberAfContainsClient(ep, ZCL_OTA_BOOTLOAD_CLUSTER_ID);
    if (has_ota) {
      ota_client_present = true;
    }
    APP_DEBUG_PRINTF("Debug: ep %u ota_client=%u\n", ep, has_ota ? 1 : 0);
  }
  if (!ota_client_present) {
    APP_DEBUG_PRINTF("Debug: OTA client cluster missing - expect ep FF writes\n");
  }
#endif
}

void app_debug_trigger_short_press(void)
{
  APP_DEBUG_PRINTF("app_debug_trigger_short_press\n");
  handle_short_press();
}

void app_debug_trigger_long_press(void)
{
  APP_DEBUG_PRINTF("app_debug_trigger_long_press\n");
  handle_long_press();
}

// Zigbee 3.0 channels (11-26)
#define ZIGBEE_CHANNELS_MASK 0x07FFF800

// Single-channel network join state (Series 1 workaround for event queue limitation)
// Try channels in order of popularity: 15, 20, 25, 11, 14, 19, 24, 26, 12, 13, 16, 17, 18, 21, 22, 23
static const uint8_t channel_scan_order[] = {15, 20, 25, 11, 14, 19, 24, 26, 12, 13, 16, 17, 18, 21, 22, 23};
static uint8_t current_channel_index = 0;  // Index into channel_scan_order array
static bool network_join_in_progress = false;
static bool join_scan_in_progress = false;
static bool join_network_found = false;
static bool app_intentional_leave_pending = false;
static EmberZigbeeNetwork join_candidate;
static bool af_init_seen = false;
static bool af_init_reported = false;
static bool basic_identity_pending = false;
static bool af_init_force_pending = false;
static uint32_t af_init_force_tick = 0;
static uint32_t basic_identity_tick = 0;
#if defined(SL_CATALOG_POWER_MANAGER_PRESENT) && (APP_DEBUG_AWAKE_AFTER_JOIN_MS > 0)
static bool app_join_awake_active = false;
static uint32_t app_join_awake_start_tick = 0;
#endif
#if (APP_DEBUG_FAST_POLL_AFTER_JOIN_MS > 0)
static bool app_fast_poll_active = false;
static uint32_t app_fast_poll_start_tick = 0;
#endif
static bool app_manual_poll_boost_active = false;
static uint32_t app_manual_poll_boost_start_tick = 0;
static uint32_t app_manual_poll_boost_last_tick = 0;
static uint32_t app_button_unlock_tick = 0;
static uint32_t app_leave_unlock_tick = 0;
static uint32_t app_join_retry_unlock_tick = 0;
static bool app_auto_join_scheduled = false;
static uint32_t app_auto_join_tick = 0;
#if APP_DEBUG_RESET_NETWORK
static bool debug_reset_network_done = false;
#endif

void app_debug_poll(void);
bool app_debug_button_ready(void);
static bool join_pending = false;
static bool join_security_configured = false;

#ifndef EMBER_ENCRYPTION_KEY_SIZE
#define EMBER_ENCRYPTION_KEY_SIZE 16
#endif

static const uint8_t zigbee_alliance_key[EMBER_ENCRYPTION_KEY_SIZE] = {
  0x5A, 0x69, 0x67, 0x42, 0x65, 0x65, 0x41, 0x6C,
  0x6C, 0x69, 0x61, 0x6E, 0x63, 0x65, 0x30, 0x39
};

// Forward declarations
static void led_blink_event_handler(sl_zigbee_event_t *event);
static void led_off_event_handler(sl_zigbee_event_t *event);
static void rejoin_retry_event_handler(sl_zigbee_event_t *event) APP_UNUSED;
static void start_optimized_rejoin(void) APP_UNUSED;
static void handle_short_press(void);
static void handle_long_press(void);
static EmberStatus start_join_scan(void);
static void try_next_channel(void);
static bool configure_join_security(void);
static bool log_basic_identity(void);
static void app_flash_probe(void);
static void app_flash_enable_init(void);
static void app_flash_send_cmd(GPIO_Port_TypeDef port,
                               unsigned int pin,
                               uint8_t cmd);
static bool app_flash_probe_with_cs(GPIO_Port_TypeDef port,
                                    unsigned int pin,
                                    const char *label);
static void app_init_once(void);
#if APP_DEBUG_RESET_NETWORK
static void app_debug_reset_network_state(void);
#endif
static void app_configure_default_reporting(void);

static bool app_join_retry_blocked(uint32_t now)
{
  if (app_join_retry_unlock_tick == 0) {
    return false;
  }
  if ((int32_t)(app_join_retry_unlock_tick - now) <= 0) {
    app_join_retry_unlock_tick = 0;
    return false;
  }
  return true;
}

static bool app_leave_guard_active(uint32_t now)
{
  if (app_leave_unlock_tick == 0) {
    return false;
  }
  if ((int32_t)(app_leave_unlock_tick - now) <= 0) {
    app_leave_unlock_tick = 0;
    return false;
  }
  return true;
}

static void app_set_join_retry_backoff(uint32_t now, uint32_t delay_ms)
{
  app_join_retry_unlock_tick = now + sl_sleeptimer_ms_to_tick(delay_ms);
}

#if APP_RUNTIME_NETWORK_STEERING
void emberAfPluginNetworkSteeringCompleteCallback(EmberStatus status,
                                                  uint8_t totalBeacons,
                                                  uint8_t joinAttempts,
                                                  uint8_t finalState)
{
  APP_DEBUG_PRINTF("Join: steering complete status=0x%02x beacons=%u attempts=%u state=%u\n",
                   status,
                   totalBeacons,
                   joinAttempts,
                   finalState);

  // If stack is still down after steering completion, allow a new button-triggered join attempt.
  if (emberAfNetworkState() != EMBER_JOINED_NETWORK) {
    network_join_in_progress = false;
    join_scan_in_progress = false;
    join_network_found = false;
  }
}
#endif

/**
 * @brief Zigbee application init callback
 *
 * Called when the Zigbee stack has completed initialization.
 * This is where we initialize application components.
 */
static void app_init_once(void)
{
  uint32_t now = sl_sleeptimer_get_tick_count();
#if APP_DEBUG_SPI_ONLY
  APP_DEBUG_PRINTF("SPI-only debug mode\n");
  app_flash_probe();
  return;
#endif
  if (af_init_seen) {
    APP_DEBUG_PRINTF("AF init callback (duplicate)\n");
    return;
  }
  APP_DEBUG_PRINTF("AF init callback\n");
  af_init_seen = true;
  af_init_force_pending = false;
  af_init_force_tick = 0;

#ifdef SL_CATALOG_SIMPLE_BUTTON_PRESENT
  // TRADFRI boards may not have an external pull-up on BTN0 (PB13).
  // Force internal pull-up + input filter to avoid floating/false presses.
  GPIO_PinModeSet(gpioPortB, 13, gpioModeInputPullFilter, 1);
  APP_DEBUG_PRINTF("BTN0: internal pull-up enabled (PB13)\n");
  // Re-arm simple_button after direct GPIO reconfiguration above.
  sl_button_disable(&sl_button_btn0);
  sl_button_enable(&sl_button_btn0);

  // Drop any stale press state captured before AF init was complete.
  // This avoids false "long press" on the first post-boot release edge.
  button_short_press_pending = false;
  button_long_press_pending = false;
  button_pressed = false;
  button_press_start_tick = 0;
  app_button_unlock_tick = now + sl_sleeptimer_ms_to_tick(APP_DEBUG_BUTTON_GUARD_AFTER_BOOT_MS);
  APP_DEBUG_PRINTF("Button guard: ignoring BTN0 for %lu ms after init\n",
                   (unsigned long)APP_DEBUG_BUTTON_GUARD_AFTER_BOOT_MS);

#endif

#if APP_DEBUG_RESET_NETWORK
  app_debug_reset_network_state();
#endif
  emberAfCorePrintln("Zigbee BME280 Sensor Application");
  emberAfCorePrintln("Silicon Labs EFR32MG1P + Bosch BME280");
  emberAfCorePrintln("Press BTN0 to join network or trigger sensor reading");

  // Initialize LED events
  sl_zigbee_event_init(&led_blink_event, led_blink_event_handler);
  sl_zigbee_event_init(&led_off_event, led_off_event_handler);

  // Button handling uses emberAfTickCallback() to check flags - no events needed

  // Initialize optimized rejoin event (TEMPORARILY DISABLED - event queue issue)
  // sl_zigbee_event_init(&rejoin_retry_event, rejoin_retry_event_handler);

  // Initialize configuration from NVM
  app_config_init();
  if (!log_basic_identity()) {
    basic_identity_pending = true;
  }

#if APP_DEBUG_DIAG_ALWAYS
  app_flash_probe();
#endif

  // Initialize BME280 sensor
  if (!app_sensor_init()) {
    emberAfCorePrintln("ERROR: Sensor initialization failed!");
#ifdef SL_CATALOG_SIMPLE_LED_PRESENT
    // Rapid blink on error
    for (int i = 0; i < 10; i++) {
      sl_led_toggle(&sl_led_led0);
      sl_sleeptimer_delay_millisecond(100);
    }
#endif
  }

  app_configure_default_reporting();

#if APP_DEBUG_AUTO_JOIN_ON_BOOT
  if (emberAfNetworkState() != EMBER_JOINED_NETWORK && !network_join_in_progress) {
    uint32_t now = sl_sleeptimer_get_tick_count();
    app_auto_join_scheduled = true;
    app_auto_join_tick = now + sl_sleeptimer_ms_to_tick(APP_DEBUG_AUTO_JOIN_DELAY_MS);
    APP_DEBUG_PRINTF("Debug: auto-join scheduled in %lu ms\n",
                     (unsigned long)APP_DEBUG_AUTO_JOIN_DELAY_MS);
  }
#endif

#if APP_DEBUG_AUTO_JOIN_ON_PIN_RESET
  // TRADFRI button can be wired to reset line on some hardware revisions.
  // If boot reason is external pin reset, treat it as an implicit join request.
  if (halGetResetInfo() == 0x03u) {
    APP_DEBUG_PRINTF("Debug: auto-join after pin reset\n");
    handle_short_press();
  }
#endif
}

static void app_configure_default_reporting(void)
{
#ifdef SL_CATALOG_ZIGBEE_REPORTING_PRESENT
  EmberAfPluginReportingEntry entry;
  EmberAfStatus st;

  memset(&entry, 0, sizeof(entry));
  entry.direction = EMBER_ZCL_REPORTING_DIRECTION_REPORTED;
  entry.endpoint = 1;
  entry.mask = CLUSTER_MASK_SERVER;
  entry.manufacturerCode = EMBER_AF_NULL_MANUFACTURER_CODE;

  entry.clusterId = ZCL_TEMP_MEASUREMENT_CLUSTER_ID;
  entry.attributeId = ZCL_TEMP_MEASURED_VALUE_ATTRIBUTE_ID;
  entry.data.reported.minInterval = 10;
  entry.data.reported.maxInterval = 300;
  entry.data.reported.reportableChange = 50;   // 0.50 C (0.01C units)
  st = emberAfPluginReportingConfigureReportedAttribute(&entry);
  APP_DEBUG_PRINTF("Reporting default: temp -> 0x%02x\n", st);

  entry.clusterId = ZCL_RELATIVE_HUMIDITY_MEASUREMENT_CLUSTER_ID;
  entry.attributeId = ZCL_RELATIVE_HUMIDITY_MEASURED_VALUE_ATTRIBUTE_ID;
  entry.data.reported.minInterval = 10;
  entry.data.reported.maxInterval = 300;
  entry.data.reported.reportableChange = 100;  // 1.00 %RH (0.01% units)
#if (APP_PROFILE_HAS_HUMIDITY != 0)
  st = emberAfPluginReportingConfigureReportedAttribute(&entry);
  APP_DEBUG_PRINTF("Reporting default: humidity -> 0x%02x\n", st);
#else
  APP_DEBUG_PRINTF("Reporting default: humidity skipped by profile\n");
#endif

  entry.clusterId = ZCL_PRESSURE_MEASUREMENT_CLUSTER_ID;
  entry.attributeId = ZCL_PRESSURE_MEASURED_VALUE_ATTRIBUTE_ID;
  entry.data.reported.minInterval = 10;
  entry.data.reported.maxInterval = 300;
  entry.data.reported.reportableChange = 1;    // 1 kPa (cluster units)
#if (APP_PROFILE_HAS_PRESSURE != 0)
  st = emberAfPluginReportingConfigureReportedAttribute(&entry);
  APP_DEBUG_PRINTF("Reporting default: pressure -> 0x%02x\n", st);
#else
  APP_DEBUG_PRINTF("Reporting default: pressure skipped by profile\n");
#endif

  entry.clusterId = ZCL_POWER_CONFIG_CLUSTER_ID;
  entry.attributeId = ZCL_BATTERY_VOLTAGE_ATTRIBUTE_ID;
  entry.data.reported.minInterval = 30;
  entry.data.reported.maxInterval = 1800;
  entry.data.reported.reportableChange = 1;    // 0.1V (100mV units)
  st = emberAfPluginReportingConfigureReportedAttribute(&entry);
  APP_DEBUG_PRINTF("Reporting default: battery voltage -> 0x%02x\n", st);

  entry.clusterId = ZCL_POWER_CONFIG_CLUSTER_ID;
  entry.attributeId = ZCL_BATTERY_PERCENTAGE_REMAINING_ATTRIBUTE_ID;
  entry.data.reported.minInterval = 30;
  entry.data.reported.maxInterval = 1800;
  entry.data.reported.reportableChange = 2;    // 1% (0.5% units)
  st = emberAfPluginReportingConfigureReportedAttribute(&entry);
  APP_DEBUG_PRINTF("Reporting default: battery pct -> 0x%02x\n", st);
#endif
}

void emberAfMainInitCallback(void)
{
  app_init_once();
}

#if APP_DEBUG_RESET_NETWORK
static void app_debug_reset_network_state(void)
{
  if (debug_reset_network_done) {
    return;
  }
  debug_reset_network_done = true;
  emberAfCorePrintln("Debug: resetting network state");
  EmberStatus leave_status = emberLeaveNetwork();
  emberAfCorePrintln("Debug: leave network -> 0x%02X", leave_status);
  EmberStatus bind_status = emberClearBindingTable();
  emberAfCorePrintln("Debug: clear binding table -> 0x%02X", bind_status);
  EmberStatus key_status = emberClearKeyTable();
  emberAfCorePrintln("Debug: clear key table -> 0x%02X", key_status);
  join_attempt_count = 0;
  current_channel_index = 0;
  join_scan_in_progress = false;
  join_network_found = false;
  network_join_in_progress = false;
}
#endif

void app_debug_force_af_init(void)
{
#if defined(APP_DEBUG_FORCE_AF_INIT) && (APP_DEBUG_FORCE_AF_INIT != 0)
  if (!af_init_seen) {
    APP_DEBUG_PRINTF("AF init requested (debug)\n");
    af_init_force_pending = true;
  }
#endif
}

void app_debug_poll(void)
{
  static uint32_t sensor_watchdog_last_tick = 0;
  uint32_t now = sl_sleeptimer_get_tick_count();
  uint32_t now_ms = sl_sleeptimer_tick_to_ms(now);
  bool button_guard_active = (app_button_unlock_tick != 0)
                             && ((int32_t)(app_button_unlock_tick - now) > 0);
  bool leave_guard_active = app_leave_guard_active(now);
  if (leave_guard_active) {
    button_guard_active = true;
  }

  // Hard gate: while joining, ignore all button activity completely.
  if (network_join_in_progress) {
    button_short_press_pending = false;
    button_long_press_pending = false;
    button_pressed = false;
    button_press_start_tick = 0;
  }
  if (leave_guard_active) {
    button_short_press_pending = false;
    button_long_press_pending = false;
    button_pressed = false;
    button_press_start_tick = 0;
  }

  if (!af_init_reported && af_init_seen) {
    af_init_reported = true;
    APP_DEBUG_PRINTF("AF init seen (tick)\n");
  }

  if (af_init_force_pending && !af_init_seen) {
    if (af_init_force_tick == 0) {
      af_init_force_tick = now;
    } else if (sl_sleeptimer_tick_to_ms(now - af_init_force_tick) >= 2000) {
      APP_DEBUG_PRINTF("AF init timeout - fallback callback\n");
      app_init_once();
      af_init_force_pending = false;
    }
  }

#if defined(SL_CATALOG_SIMPLE_BUTTON_PRESENT) && (APP_DEBUG_POLL_SIMPLE_BUTTON_INSTANCES != 0)
  // Keep simple_button state machine updated even if AF tick callback isn't
  // scheduled frequently on this target.
  sl_simple_button_poll_instances();
#endif

  // Some debug builds run without AF tick wiring, so process deferred joins here.
  if (join_pending && af_init_seen && !network_join_in_progress) {
    join_pending = false;
    APP_DEBUG_PRINTF("Join: deferred request starting (poll)\n");
    handle_short_press();
  }

  if (app_auto_join_scheduled) {
    EmberNetworkStatus state = emberAfNetworkState();
    if (state == EMBER_JOINED_NETWORK || network_join_in_progress) {
      app_auto_join_scheduled = false;
      app_auto_join_tick = 0;
    } else if (af_init_seen
               && app_auto_join_tick != 0
               && (int32_t)(now - app_auto_join_tick) >= 0) {
      app_auto_join_scheduled = false;
      app_auto_join_tick = 0;
      APP_DEBUG_PRINTF("Debug: auto-join timer fired\n");
      handle_short_press();
    }
  }

  // Self-heal: if joined and periodic sensor updates stall, re-arm them.
  if (emberAfNetworkState() == EMBER_JOINED_NETWORK) {
    if (sensor_watchdog_last_tick == 0
        || sl_sleeptimer_tick_to_ms(now - sensor_watchdog_last_tick) >= 5000) {
      sensor_watchdog_last_tick = now;
      uint32_t last_update_ms = app_sensor_get_last_update_ms();
      bool timer_running = app_sensor_is_timer_running();
      if (!timer_running
          || (last_update_ms != 0 && (now_ms - last_update_ms) > 45000)) {
        APP_DEBUG_PRINTF("Sensor watchdog: restart periodic updates (timer=%d last_age=%lu ms)\n",
                         timer_running ? 1 : 0,
                         (unsigned long)(last_update_ms == 0 ? 0 : (now_ms - last_update_ms)));
        app_sensor_start_periodic_updates();
      }
    }
  }

  // Some targets do not run emberAfTickCallback reliably in this app flow.
  // Consume button flags here as well to guarantee action dispatch.
  if (button_short_press_pending) {
    button_short_press_pending = false;
    if (button_guard_active) {
      APP_DEBUG_PRINTF("Button guard: short press ignored\n");
    } else {
      APP_DEBUG_PRINTF("Button short press\n");
      emberAfCorePrintln("Button: Short press detected (poll callback)");
      handle_short_press();
    }
  }

  if (button_long_press_pending) {
    button_long_press_pending = false;
    if (button_guard_active) {
      APP_DEBUG_PRINTF("Button guard: long press ignored\n");
    } else {
      APP_DEBUG_PRINTF("Button long press\n");
      emberAfCorePrintln("Button: Long press detected (poll callback)");
      handle_long_press();
    }
  }
  if (basic_identity_pending && af_init_seen) {
    if (basic_identity_tick == 0 ||
        sl_sleeptimer_tick_to_ms(now - basic_identity_tick) >= 2000) {
      basic_identity_tick = now;
      if (log_basic_identity()) {
        basic_identity_pending = false;
      }
    }
  }

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT) && (APP_DEBUG_AWAKE_AFTER_JOIN_MS > 0)
  if (app_join_awake_active && app_join_awake_start_tick != 0) {
    uint32_t elapsed_ms = sl_sleeptimer_tick_to_ms(now - app_join_awake_start_tick);
    if (elapsed_ms >= APP_DEBUG_AWAKE_AFTER_JOIN_MS) {
      sl_power_manager_remove_em_requirement(SL_POWER_MANAGER_EM0);
      app_join_awake_active = false;
      app_join_awake_start_tick = 0;
      APP_DEBUG_PRINTF("Debug: post-join awake window ended\n");
    }
  }
#endif

#if (APP_DEBUG_FAST_POLL_AFTER_JOIN_MS > 0)
  if (app_fast_poll_active && app_fast_poll_start_tick != 0) {
    uint32_t elapsed_ms = sl_sleeptimer_tick_to_ms(now - app_fast_poll_start_tick);
    if (elapsed_ms >= APP_DEBUG_FAST_POLL_AFTER_JOIN_MS) {
#if (APP_DEBUG_NO_SLEEP != 0)
      // In no-sleep debug mode keep short-poll/app-tasks active after the
      // "window" so SWO remains alive and SED stays responsive for diagnostics.
      APP_DEBUG_PRINTF("Debug: fast poll window ended (no-sleep mode, keeping short poll)\n");
#else
      emberAfSetDefaultPollControlCallback(EMBER_AF_LONG_POLL);
      emberAfRemoveFromCurrentAppTasksCallback(EMBER_AF_FORCE_SHORT_POLL);
      emberAfRemoveFromCurrentAppTasksCallback(EMBER_AF_FORCE_SHORT_POLL_FOR_PARENT_CONNECTIVITY);
      emberAfSetDefaultSleepControl(EMBER_AF_OK_TO_SLEEP);
      APP_DEBUG_PRINTF("Debug: fast poll window ended\n");
#endif
      app_fast_poll_active = false;
      app_fast_poll_start_tick = 0;
    }
  }
#endif

#if (APP_DEBUG_MANUAL_POLL_BOOST_MS > 0)
  if (app_manual_poll_boost_active) {
    if (emberAfNetworkState() != EMBER_JOINED_NETWORK) {
      app_manual_poll_boost_active = false;
      app_manual_poll_boost_start_tick = 0;
      app_manual_poll_boost_last_tick = 0;
    } else {
      uint32_t elapsed_ms = sl_sleeptimer_tick_to_ms(now - app_manual_poll_boost_start_tick);
      if (elapsed_ms >= APP_DEBUG_MANUAL_POLL_BOOST_MS) {
        app_manual_poll_boost_active = false;
        app_manual_poll_boost_start_tick = 0;
        app_manual_poll_boost_last_tick = 0;
        APP_DEBUG_PRINTF("Debug: manual poll boost window ended\n");
      } else if (app_manual_poll_boost_last_tick == 0
                 || sl_sleeptimer_tick_to_ms(now - app_manual_poll_boost_last_tick)
                 >= APP_DEBUG_MANUAL_POLL_INTERVAL_MS) {
        app_manual_poll_boost_last_tick = now;
        EmberStatus poll_st = emberPollForData();
        if (poll_st != EMBER_SUCCESS && poll_st != EMBER_MAC_SCANNING) {
          APP_DEBUG_PRINTF("Debug: manual poll -> 0x%02x\n", poll_st);
        }
      }
    }
  }
#endif
}

bool app_debug_button_ready(void)
{
  return af_init_seen;
}

static bool log_basic_identity(void)
{
  uint8_t buf[1 + 32];
  char str[33];
  EmberAfStatus st;
  uint8_t endpoint_count = emberAfEndpointCount();
  if (endpoint_count == 0) {
    APP_DEBUG_PRINTF("Basic: endpoints not ready (count=0)\n");
    return false;
  }
  uint8_t endpoint = emberAfEndpointFromIndex(0);
  APP_DEBUG_PRINTF("Basic: using endpoint %u (count=%u)\n",
                   endpoint,
                   endpoint_count);

#ifdef ZCL_POWER_SOURCE_ATTRIBUTE_ID
  {
    uint8_t power_source = 0x03; // Battery
    EmberAfStatus wr = emberAfWriteServerAttribute(endpoint,
                                                    ZCL_BASIC_CLUSTER_ID,
                                                    ZCL_POWER_SOURCE_ATTRIBUTE_ID,
                                                    &power_source,
                                                    ZCL_ENUM8_ATTRIBUTE_TYPE);
    APP_DEBUG_PRINTF("Basic: set power source(battery) -> 0x%02x\n", wr);
  }
#endif

#ifdef ZCL_MANUFACTURER_NAME_ATTRIBUTE_ID
  st = emberAfReadServerAttribute(endpoint,
                                  ZCL_BASIC_CLUSTER_ID,
                                  ZCL_MANUFACTURER_NAME_ATTRIBUTE_ID,
                                  buf,
                                  sizeof(buf));
  if (st == EMBER_ZCL_STATUS_SUCCESS) {
    uint8_t len = buf[0] > 32 ? 32 : buf[0];
    memcpy(str, &buf[1], len);
    str[len] = '\0';
    APP_DEBUG_PRINTF("Basic: manufacturer=\"%s\"\n", str);
  } else {
    APP_DEBUG_PRINTF("Basic: manufacturer read -> 0x%02x\n", st);
  }
#endif

#ifdef ZCL_MODEL_IDENTIFIER_ATTRIBUTE_ID
  st = emberAfReadServerAttribute(endpoint,
                                  ZCL_BASIC_CLUSTER_ID,
                                  ZCL_MODEL_IDENTIFIER_ATTRIBUTE_ID,
                                  buf,
                                  sizeof(buf));
  if (st == EMBER_ZCL_STATUS_SUCCESS) {
    uint8_t len = buf[0] > 32 ? 32 : buf[0];
    memcpy(str, &buf[1], len);
    str[len] = '\0';
    APP_DEBUG_PRINTF("Basic: model=\"%s\"\n", str);
  } else {
    APP_DEBUG_PRINTF("Basic: model read -> 0x%02x\n", st);
  }
#endif

#ifdef ZCL_SW_BUILD_ID_ATTRIBUTE_ID
  st = emberAfReadServerAttribute(endpoint,
                                  ZCL_BASIC_CLUSTER_ID,
                                  ZCL_SW_BUILD_ID_ATTRIBUTE_ID,
                                  buf,
                                  sizeof(buf));
  if (st == EMBER_ZCL_STATUS_SUCCESS) {
    uint8_t len = buf[0] > 32 ? 32 : buf[0];
    memcpy(str, &buf[1], len);
    str[len] = '\0';
    APP_DEBUG_PRINTF("Basic: sw build=\"%s\"\n", str);
  } else {
    APP_DEBUG_PRINTF("Basic: sw build read -> 0x%02x\n", st);
  }
#endif
  return true;
}

static void app_flash_probe(void)
{
  app_flash_enable_init();

  static bool spidrv_inited = false;
  if (!spidrv_inited) {
    if (sl_spidrv_exp_init != NULL) {
      sl_spidrv_exp_init();
    } else if (sl_spidrv_init_instances != NULL) {
      sl_spidrv_init_instances();
    } else {
      APP_DEBUG_PRINTF("SPI flash: SPIDRV init symbol missing\n");
    }
    spidrv_inited = true;
    APP_DEBUG_PRINTF("SPI flash: SPIDRV exp init (handle=%p)\n",
                     (void *)sl_spidrv_exp_handle);
#ifdef SL_SPIDRV_EXP_PERIPHERAL
    USART_TypeDef *periph = SL_SPIDRV_EXP_PERIPHERAL;
    const char *periph_name = "USART?";
    if (periph == USART0) {
      periph_name = "USART0";
    } else if (periph == USART1) {
      periph_name = "USART1";
    }
    APP_DEBUG_PRINTF("SPI flash: %s ROUTELOC0=0x%08lX ROUTEPEN=0x%08lX\n",
                     periph_name,
                     (unsigned long)periph->ROUTELOC0,
                     (unsigned long)periph->ROUTEPEN);
#endif
#ifdef SL_SPIDRV_EXP_TX_LOC
    APP_DEBUG_PRINTF("SPI flash: SPIDRV loc tx=%u rx=%u clk=%u\n",
                     (unsigned)SL_SPIDRV_EXP_TX_LOC,
                     (unsigned)SL_SPIDRV_EXP_RX_LOC,
                     (unsigned)SL_SPIDRV_EXP_CLK_LOC);
#endif
  }

  if (!app_flash_probe_with_cs(gpioPortB, 11, "PB11")) {
    APP_DEBUG_PRINTF("SPI flash: no response on PB11\n");
  }
}

static void app_flash_enable_init(void)
{
  static bool configured = false;
  if (configured) {
    return;
  }
  // ICC-1 exposes PF3; ICC-A-1 uses PF3 internally to enable the SPI flash.
  GPIO_PinModeSet(gpioPortF, 3, gpioModePushPull, 1);
  APP_DEBUG_PRINTF("SPI flash: enable PF3=1\n");
  configured = true;
}

static void app_flash_send_cmd(GPIO_Port_TypeDef port,
                               unsigned int pin,
                               uint8_t cmd)
{
  app_flash_enable_init();
  GPIO_PinModeSet(port, pin, gpioModePushPull, 1);
  uint8_t tx[1] = {cmd};
  GPIO_PinOutClear(port, pin);
  (void)SPIDRV_MTransmitB(sl_spidrv_exp_handle, tx, sizeof(tx));
  GPIO_PinOutSet(port, pin);
}

static bool app_flash_probe_with_cs(GPIO_Port_TypeDef port,
                                    unsigned int pin,
                                    const char *label)
{
  GPIO_PinModeSet(port, pin, gpioModePushPull, 1);

  app_flash_send_cmd(port, pin, 0xFF); // Release from continuous read (safe no-op)
  app_flash_send_cmd(port, pin, 0xAB); // Release from deep power-down
  app_flash_send_cmd(port, pin, 0x66); // Reset enable
  app_flash_send_cmd(port, pin, 0x99); // Reset memory
  sl_sleeptimer_delay_millisecond(1);

  uint8_t tx[4] = {0x9F, 0x00, 0x00, 0x00};
  uint8_t rx[4] = {0};

  GPIO_PinOutClear(port, pin);
  Ecode_t status = SPIDRV_MTransferB(sl_spidrv_exp_handle, tx, rx, sizeof(tx));
  GPIO_PinOutSet(port, pin);
  if (status != ECODE_OK) {
    APP_DEBUG_PRINTF("SPI flash: JEDEC read failed (%s, 0x%lx)\n",
                     label,
                     (unsigned long)status);
    return false;
  }

  APP_DEBUG_PRINTF("SPI flash: JEDEC ID %02X %02X %02X (%s)\n",
                   rx[1],
                   rx[2],
                   rx[3],
                   label);

  tx[0] = 0x05; // Read Status Register-1
  memset(rx, 0, sizeof(rx));
  GPIO_PinOutClear(port, pin);
  status = SPIDRV_MTransferB(sl_spidrv_exp_handle, tx, rx, 2);
  GPIO_PinOutSet(port, pin);
  if (status == ECODE_OK) {
    APP_DEBUG_PRINTF("SPI flash: SR1=0x%02X (%s)\n", rx[1], label);
  }

  tx[0] = 0x35; // Read Status Register-2
  memset(rx, 0, sizeof(rx));
  GPIO_PinOutClear(port, pin);
  status = SPIDRV_MTransferB(sl_spidrv_exp_handle, tx, rx, 2);
  GPIO_PinOutSet(port, pin);
  if (status == ECODE_OK) {
    APP_DEBUG_PRINTF("SPI flash: SR2=0x%02X (%s)\n", rx[1], label);
  }

  return !((rx[1] == 0x00 && rx[2] == 0x00 && rx[3] == 0x00)
           || (rx[1] == 0xFF && rx[2] == 0xFF && rx[3] == 0xFF));
}


/**
 * @brief Network join success callback
 *
 * Called when the device successfully joins a Zigbee network.
 */
void emberAfStackStatusCallback(EmberStatus status)
{
#if APP_DEBUG_SPI_ONLY
  (void)status;
  return;
#endif
  uint32_t now = sl_sleeptimer_get_tick_count();
  APP_DEBUG_PRINTF("Stack status: 0x%02x\n", status);
  if (status == EMBER_NETWORK_UP) {
    emberAfCorePrintln("Network joined successfully");
    EmberNodeType runtime_node_type = EMBER_UNKNOWN_DEVICE;
    if (emberGetNodeType(&runtime_node_type) == EMBER_SUCCESS) {
      APP_DEBUG_PRINTF("Join: runtime node type=%u\n", runtime_node_type);
    }

#if APP_DEBUG_SET_KEEPALIVE_ALL
    // Optional debug override: force keep-alive support mode.
    EmberStatus ka_status = emberSetKeepAliveMode(EMBER_KEEP_ALIVE_SUPPORT_ALL);
    APP_DEBUG_PRINTF("Join: set keep-alive mode(all) -> 0x%02x\n", ka_status);
#else
    APP_DEBUG_PRINTF("Join: keep-alive mode: stack default\n");
#endif
    app_button_unlock_tick = now + sl_sleeptimer_ms_to_tick(APP_DEBUG_BUTTON_GUARD_AFTER_JOIN_MS);
    APP_DEBUG_PRINTF("Button guard: ignoring BTN0 for %lu ms after join\n",
                     (unsigned long)APP_DEBUG_BUTTON_GUARD_AFTER_JOIN_MS);

#if (APP_DEBUG_MANUAL_POLL_BOOST_MS > 0)
    if (runtime_node_type == EMBER_SLEEPY_END_DEVICE) {
      app_manual_poll_boost_active = true;
      app_manual_poll_boost_start_tick = sl_sleeptimer_get_tick_count();
      app_manual_poll_boost_last_tick = 0;
      APP_DEBUG_PRINTF("Debug: manual poll boost enabled for %lu ms (interval=%lu ms)\n",
                       (unsigned long)APP_DEBUG_MANUAL_POLL_BOOST_MS,
                       (unsigned long)APP_DEBUG_MANUAL_POLL_INTERVAL_MS);
    } else {
      app_manual_poll_boost_active = false;
      app_manual_poll_boost_start_tick = 0;
      app_manual_poll_boost_last_tick = 0;
    }
#endif

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT) && (APP_DEBUG_AWAKE_AFTER_JOIN_MS > 0)
    if (!app_join_awake_active) {
      sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM0);
      app_join_awake_active = true;
      app_join_awake_start_tick = sl_sleeptimer_get_tick_count();
      APP_DEBUG_PRINTF("Debug: keeping EM0 for %lu ms after join\n",
                       (unsigned long)APP_DEBUG_AWAKE_AFTER_JOIN_MS);
    }
#endif

#if (APP_DEBUG_FAST_POLL_AFTER_JOIN_MS > 0)
    emberAfSetDefaultPollControlCallback(EMBER_AF_SHORT_POLL);
    emberAfAddToCurrentAppTasksCallback(EMBER_AF_FORCE_SHORT_POLL);
    emberAfAddToCurrentAppTasksCallback(EMBER_AF_FORCE_SHORT_POLL_FOR_PARENT_CONNECTIVITY);
    emberAfSetShortPollIntervalMsCallback((int16u)APP_DEBUG_FAST_POLL_INTERVAL_MS);
    emberAfSetWakeTimeoutMsCallback((int16u)APP_DEBUG_FAST_POLL_AFTER_JOIN_MS);
    emberAfSetDefaultSleepControl(EMBER_AF_STAY_AWAKE);
    app_fast_poll_active = true;
    app_fast_poll_start_tick = sl_sleeptimer_get_tick_count();
    APP_DEBUG_PRINTF("Debug: fast poll enabled for %lu ms (short=%lu ms)\n",
                     (unsigned long)APP_DEBUG_FAST_POLL_AFTER_JOIN_MS,
                     (unsigned long)APP_DEBUG_FAST_POLL_INTERVAL_MS);
#endif

    // Reset join attempt counter and scan state on success
    join_attempt_count = 0;
    current_channel_index = 0;
    network_join_in_progress = false;
    join_scan_in_progress = false;
    join_network_found = false;
    app_join_retry_unlock_tick = 0;

    // Cancel any pending rejoin attempts - network is up (TEMPORARILY DISABLED)
    // rejoin_state = REJOIN_STATE_DONE;
    // sl_zigbee_event_set_inactive(&rejoin_retry_event);

    // Stop LED blinking
    led_blink_active = false;
    sl_zigbee_event_set_inactive(&led_blink_event);

#ifdef SL_CATALOG_SIMPLE_LED_PRESENT
    // Turn LED on solid to indicate network is up
    sl_led_turn_on(&sl_led_led0);

    // Schedule LED to turn off after 3 seconds to save power
    sl_zigbee_event_set_delay_ms(&led_off_event, 3000);
#endif

    // Avoid heavy sensor transactions right at join/interview start.
    // Start periodic updates and let first sample happen on timer.
    app_sensor_start_periodic_updates();

    // Note: Binding is handled by coordinator (Zigbee2MQTT/ZHA/deCONZ)
    // No device-side binding code needed - see BINDING_GUIDE.md

  } else if (status == EMBER_NETWORK_DOWN) {
    if (app_intentional_leave_pending) {
      emberAfCorePrintln("Network down after manual leave");
      app_intentional_leave_pending = false;
      app_leave_unlock_tick = now + sl_sleeptimer_ms_to_tick(APP_DEBUG_BUTTON_GUARD_AFTER_LEAVE_MS);
      app_set_join_retry_backoff(now, APP_DEBUG_BUTTON_GUARD_AFTER_LEAVE_MS);
      APP_DEBUG_PRINTF("Button guard: ignoring BTN0 for %lu ms after leave\n",
                       (unsigned long)APP_DEBUG_BUTTON_GUARD_AFTER_LEAVE_MS);
    } else {
      emberAfCorePrintln("Network down - will attempt optimized rejoin");
      app_set_join_retry_backoff(now, APP_DEBUG_JOIN_RETRY_BACKOFF_AFTER_LEAVE_MS);
    }
    app_button_unlock_tick = 0;
    join_security_configured = false;

#if (APP_DEBUG_FAST_POLL_AFTER_JOIN_MS > 0)
    emberAfSetDefaultPollControlCallback(EMBER_AF_LONG_POLL);
    emberAfRemoveFromCurrentAppTasksCallback(EMBER_AF_FORCE_SHORT_POLL);
    emberAfRemoveFromCurrentAppTasksCallback(EMBER_AF_FORCE_SHORT_POLL_FOR_PARENT_CONNECTIVITY);
    emberAfSetDefaultSleepControl(EMBER_AF_OK_TO_SLEEP);
    app_fast_poll_active = false;
    app_fast_poll_start_tick = 0;
#endif

    app_manual_poll_boost_active = false;
    app_manual_poll_boost_start_tick = 0;
    app_manual_poll_boost_last_tick = 0;

#ifdef SL_CATALOG_SIMPLE_LED_PRESENT
    // Turn LED off when network is down
    sl_led_turn_off(&sl_led_led0);
    // Cancel any pending LED off event
    sl_zigbee_event_set_inactive(&led_off_event);
#endif

    // Stop periodic sensor timer while network is down to avoid 10s wakeups.
    app_sensor_stop_periodic_updates();

    // Optimized rejoin TEMPORARILY DISABLED - event queue issue
    // emberAfCorePrintln("Scheduling optimized rejoin in 100ms...");
    // rejoin_state = REJOIN_STATE_IDLE;
    // sl_zigbee_event_set_delay_ms(&rejoin_retry_event, 100);

  }
}

void emberAfPluginEndDeviceSupportPollCompletedCallback(EmberStatus status)
{
  // Avoid log spam on normal idle polls.
  if (status != EMBER_MAC_NO_DATA) {
    APP_DEBUG_PRINTF("Poll complete: status=0x%02x\n", status);
  }
}

bool emberAfPreCommandReceivedCallback(EmberAfClusterCommand *cmd)
{
  if (cmd != NULL && cmd->mfgSpecific == 0u) {
    if (cmd->commandId == ZCL_CONFIGURE_REPORTING_COMMAND_ID
        || cmd->commandId == ZCL_READ_REPORTING_CONFIGURATION_COMMAND_ID) {
      APP_DEBUG_PRINTF("ZCL cfg-report rx: ep=%u clus=0x%04x cmd=0x%02x dir=%u len=%u\n",
                       cmd->apsFrame->destinationEndpoint,
                       cmd->apsFrame->clusterId,
                       cmd->commandId,
                       (unsigned)((cmd->buffer[0] & ZCL_FRAME_CONTROL_CLIENT_TO_SERVER) ? 1 : 0),
                       cmd->bufLen);
    }
  }
  return false;
}

/**
 * @brief Button press callback
 *
 * Called when a button is pressed or released on the board.
 * Detects short press (<5s) vs long press (>=5s).
 *
 * CRITICAL: This runs in ISR context (GPIO_ODD_IRQn)!
 * CANNOT call ANY functions that interact with Zigbee stack!
 * CANNOT call emberAfCorePrintln()!
 * CANNOT call sl_zigbee_event_set_active()!
 *
 * Only allowed: read hardware, do math, set volatile flags
 * The button_poll_event running in main context will check these flags
 */
#ifdef SL_CATALOG_SIMPLE_BUTTON_PRESENT
void sl_button_on_change(const sl_button_t *handle)
{
  if (handle == &sl_button_btn0) {
    if (network_join_in_progress) {
      button_short_press_pending = false;
      button_long_press_pending = false;
      button_pressed = false;
      button_press_start_tick = 0;
      return;
    }

    if (app_button_unlock_tick != 0) {
      uint32_t now = sl_sleeptimer_get_tick_count();
      if ((int32_t)(app_button_unlock_tick - now) > 0) {
        button_pressed = false;
        button_press_start_tick = 0;
        return;
      }
      app_button_unlock_tick = 0;
    }

    // Ignore button edges before AF init to avoid stale hold-duration math.
    if (!af_init_seen) {
      button_pressed = false;
      button_press_start_tick = 0;
      return;
    }

    if (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED) {
      if (button_pressed) {
        return;
      }
      // Button pressed - record start time
      button_press_start_tick = sl_sleeptimer_get_tick_count();
      button_pressed = true;
      APP_DEBUG_PRINTF("BTN0: PRESSED\n");
    } else {
      // Button released - check duration
      if (button_pressed) {
        uint32_t duration_ticks = sl_sleeptimer_get_tick_count() - button_press_start_tick;
        uint32_t duration_ms = sl_sleeptimer_tick_to_ms(duration_ticks);
        APP_DEBUG_PRINTF("BTN0: RELEASED (%lu ms)\n", (unsigned long)duration_ms);

        if (duration_ms > APP_DEBUG_BUTTON_MAX_VALID_PRESS_MS) {
          // Treat extremely long holds as floating-line glitches.
          button_pressed = false;
          return;
        }

        // Set flags for main context to poll
        // These will be checked by button_poll_event_handler()
        if (duration_ms >= BUTTON_DEBOUNCE_MS) {
          if (duration_ms >= APP_BUTTON_LONG_PRESS_MS) {
            button_long_press_pending = true;
          } else {
            button_short_press_pending = true;
          }
        }

        button_pressed = false;
      }
    }
  }
}
#endif

/**
 * @brief LED blink event handler
 *
 * Blinks the LED while joining network.
 */
static void led_blink_event_handler(sl_zigbee_event_t *event)
{
#ifdef SL_CATALOG_SIMPLE_LED_PRESENT
  if (led_blink_active) {
    sl_led_toggle(&sl_led_led0);
    // Blink every 500ms
    sl_zigbee_event_set_delay_ms(&led_blink_event, 500);
  }
#endif
}

/**
 * @brief LED off event handler
 *
 * Turns off the LED after network join confirmation to save power.
 */
static void led_off_event_handler(sl_zigbee_event_t *event)
{
#ifdef SL_CATALOG_SIMPLE_LED_PRESENT
  sl_led_turn_off(&sl_led_led0);
  emberAfCorePrintln("LED turned off to save power");
#endif
}

/**
 * @brief Main tick callback - runs every main loop iteration
 *
 * This is called from main context and is perfect for checking button flags
 * set by the button ISR. No event scheduling needed!
 */
void emberAfTickCallback(void)
{
  // In debug profiles, app_debug_poll() is called from main loop.
  // Avoid handling button/join logic in two places to prevent edge races.
#if !(APP_DEBUG_DIAG_ALWAYS || APP_DEBUG_FORCE_AF_INIT)
  app_debug_poll();
#endif
}

/**
 * @brief Optimized rejoin retry event handler
 *
 * Manages the rejoin state machine:
 * 1. Try current channel first (fast path)
 * 2. If timeout, try all channels (fallback)
 */
static void rejoin_retry_event_handler(sl_zigbee_event_t *event)
{
  // TEMPORARILY DISABLED - event queue issue
  // Optimized rejoin functionality removed to reduce event usage
  (void)event;  // Unused parameter
}

/**
 * @brief Start optimized rejoin - try current channel first
 *
 * Optimization: Attempt rejoin on previously-used channel first for fast
 * reconnection (~138ms vs ~2.2s). Falls back to full channel scan after
 * timeout if current channel doesn't respond.
 *
 * Benefits:
 * - 7x faster rejoin when successful (typical case)
 * - ~85% power savings for successful rejoins
 * - Graceful fallback to full scan if network changed
 */
static void start_optimized_rejoin(void)
{
  // TEMPORARILY DISABLED - event queue issue
  // Optimized rejoin functionality removed to reduce event usage
}

/**
 * @brief Try next channel in the scan sequence
 *
 * Move to next channel and attempt join. If all channels exhausted,
 * report failure and stop scanning.
 */
static void try_next_channel(void)
{
  const uint8_t total_channels = sizeof(channel_scan_order) / sizeof(channel_scan_order[0]);

  if (join_scan_in_progress) {
    APP_DEBUG_PRINTF("Join: scan already in progress\n");
    return;
  }

  // Move to next channel
  current_channel_index++;

  while (current_channel_index < total_channels) {
    EmberStatus status = start_join_scan();
    if (status == EMBER_SUCCESS) {
      return;
    }
    APP_DEBUG_PRINTF("Join: scan start failed on ch %d status 0x%02x\n",
                     channel_scan_order[current_channel_index],
                     status);
    current_channel_index++;
  }

  if (current_channel_index >= total_channels) {
    // Exhausted all channels - give up
    emberAfCorePrintln("All channels scanned - no network found");
    network_join_in_progress = false;
    join_scan_in_progress = false;
    join_network_found = false;
    current_channel_index = 0;
    join_attempt_count++;

#ifdef SL_CATALOG_SIMPLE_LED_PRESENT
    // Stop LED blinking
    led_blink_active = false;
    sl_zigbee_event_set_inactive(&led_blink_event);
    sl_led_turn_off(&sl_led_led0);
#endif
  }
}

/**
 * @brief Start an active scan on the current channel
 *
 * Series 1 Workaround: Scans ONE channel at a time instead of all 16 channels
 * simultaneously. This prevents event queue overflow on Series 1 chips.
 */
static EmberStatus start_join_scan(void)
{
  const uint8_t total_channels = sizeof(channel_scan_order) / sizeof(channel_scan_order[0]);

  if (current_channel_index >= total_channels) {
    emberAfCorePrintln("ERROR: Invalid channel index %d", current_channel_index);
    return EMBER_INVALID_CALL;
  }

  uint8_t channel_to_scan = channel_scan_order[current_channel_index];
  uint32_t single_channel_mask = BIT32(channel_to_scan);

  emberAfCorePrintln("Active scan channel %d (%d of %d)...",
                     channel_to_scan,
                     current_channel_index + 1,
                     total_channels);
  APP_DEBUG_PRINTF("Join: start scan channel %d mask 0x%08lx\n",
                   channel_to_scan,
                   (unsigned long)single_channel_mask);

  join_scan_in_progress = true;
  join_network_found = false;
  memset(&join_candidate, 0, sizeof(join_candidate));

  // Scan ONLY this one channel - uses minimal events
  EmberStatus status = emberStartScan(EMBER_ACTIVE_SCAN, single_channel_mask, JOIN_SCAN_DURATION);
  APP_DEBUG_PRINTF("Join: emberStartScan -> 0x%02x\n", status);

  if (status != EMBER_SUCCESS) {
    emberAfCorePrintln("Failed to start scan on channel %d: 0x%x", channel_to_scan, status);
    join_scan_in_progress = false;
  }

  return status;
}

/**
 * @brief Scan result callback (called for each network found)
 */
void emberAfNetworkFoundCallback(EmberZigbeeNetwork *networkFound, uint8_t lqi, int8_t rssi)
{
#if APP_RUNTIME_NETWORK_STEERING
  // Network Steering plugin owns scan callbacks when present.
  (void)networkFound;
  (void)lqi;
  (void)rssi;
  return;
#else
  if (!network_join_in_progress || networkFound == NULL) {
    return;
  }

  if (!networkFound->allowingJoin) {
    APP_DEBUG_PRINTF("Join: network ch %d pan 0x%04x not open (lqi=%u rssi=%d)\n",
                     networkFound->channel,
                     networkFound->panId,
                     lqi,
                     rssi);
    return;
  }

  if (!join_network_found) {
    join_network_found = true;
    join_candidate = *networkFound;
    APP_DEBUG_PRINTF("Join: found network ch %d pan 0x%04x (lqi=%u rssi=%d)\n",
                     join_candidate.channel,
                     join_candidate.panId,
                     lqi,
                     rssi);
  }
#endif
}

/**
 * @brief Scan completion callback
 */
void emberAfScanCompleteCallback(uint8_t channel, EmberStatus status)
{
#if APP_RUNTIME_NETWORK_STEERING
  // Network Steering plugin owns scan callbacks when present.
  (void)channel;
  (void)status;
  return;
#else
  if (!network_join_in_progress) {
    return;
  }

  join_scan_in_progress = false;
  APP_DEBUG_PRINTF("Join: scan complete ch=%u status=0x%02x found=%d\n",
                   channel,
                   status,
                   join_network_found ? 1 : 0);

  if (join_network_found) {
    if (!configure_join_security()) {
      emberAfCorePrintln("Join aborted: security state setup failed");
      APP_DEBUG_PRINTF("Join: abort scan result join due to security setup failure\n");
      network_join_in_progress = false;
      join_scan_in_progress = false;
      join_network_found = false;
      current_channel_index = 0;
      return;
    }

    EmberNetworkParameters params;
    memset(&params, 0, sizeof(params));
    memcpy(params.extendedPanId, join_candidate.extendedPanId, EXTENDED_PAN_ID_SIZE);
    params.panId = join_candidate.panId;
    params.radioChannel = join_candidate.channel;
    params.radioTxPower = emberGetRadioPower();
    params.joinMethod = EMBER_USE_MAC_ASSOCIATION;
    params.nwkManagerId = 0x0000;
    params.nwkUpdateId = join_candidate.nwkUpdateId;
    params.channels = BIT32(join_candidate.channel);

    EmberNodeType node_type = APP_DEBUG_JOIN_AS_END_DEVICE ? EMBER_END_DEVICE : EMBER_SLEEPY_END_DEVICE;
    APP_DEBUG_PRINTF("Join: node type=%s\n",
                     APP_DEBUG_JOIN_AS_END_DEVICE ? "END_DEVICE" : "SLEEPY_END_DEVICE");
    EmberStatus join_status = emberJoinNetwork(node_type, &params);
    APP_DEBUG_PRINTF("Join: emberJoinNetwork -> 0x%02x\n", join_status);
    if (join_status != EMBER_SUCCESS) {
      emberAfCorePrintln("Join failed to start: 0x%x", join_status);
      // If stack is busy/not ready, channel hopping does not help and causes long loops.
      // Abort this join attempt and wait for the next user press.
      EmberNetworkStatus net_state = emberAfNetworkState();
      APP_DEBUG_PRINTF("Join: abort attempt on status 0x%02x (net=%d)\n", join_status, net_state);
      network_join_in_progress = false;
      join_scan_in_progress = false;
      join_network_found = false;
      current_channel_index = 0;
      join_security_configured = false;
      app_set_join_retry_backoff(sl_sleeptimer_get_tick_count(), APP_DEBUG_JOIN_RETRY_BACKOFF_MS);
      }
    return;
  }

  // No joinable network on this channel - try next channel
  try_next_channel();
#endif
}

/**
 * @brief Handle short button press (<5 seconds)
 *
 * Short press triggers immediate sensor read and report.
 * If not joined to network, starts network joining.
 */
static void handle_short_press(void)
{
#if APP_DEBUG_SPI_ONLY
  app_flash_probe();
  return;
#endif
  EmberNetworkStatus network_state = emberAfNetworkState();

  if (network_state == EMBER_JOINED_NETWORK) {
    // Already on network - trigger immediate sensor reading
    emberAfCorePrintln("Triggering immediate sensor read...");
    app_sensor_update();

#ifdef SL_CATALOG_SIMPLE_LED_PRESENT
    // Flash LED briefly to indicate sensor read
    sl_led_turn_on(&sl_led_led0);
    sl_sleeptimer_delay_millisecond(200);
    sl_led_turn_off(&sl_led_led0);
#endif

  } else {
    uint32_t now = sl_sleeptimer_get_tick_count();
    if (app_join_retry_blocked(now)) {
      APP_DEBUG_PRINTF("Join: retry backoff active\n");
      return;
    }
    if (!af_init_seen) {
      APP_DEBUG_PRINTF("Join: AF init not ready - deferring\n");
      join_pending = true;
      return;
    }
    if (network_join_in_progress) {
      emberAfCorePrintln("Join already in progress - ignoring button press");
      APP_DEBUG_PRINTF("Join: already in progress\n");
      return;
    }

    // Not on network - start join
    emberAfCorePrintln("Not joined - starting network join (attempt %d)...",
                       join_attempt_count + 1);
    APP_DEBUG_PRINTF("Join: attempt %d\n", join_attempt_count + 1);
#if !APP_RUNTIME_NETWORK_STEERING
    if (!configure_join_security()) {
      emberAfCorePrintln("Join aborted: security state setup failed");
      return;
    }
#endif

    // Reset to start of channel list
    current_channel_index = 0;
    join_scan_in_progress = false;
    join_network_found = false;
    network_join_in_progress = true;

#ifdef SL_CATALOG_SIMPLE_LED_PRESENT
    // Start LED blinking to indicate joining
    led_blink_active = true;
    sl_zigbee_event_set_active(&led_blink_event);
#endif

    EmberStatus join_status = EMBER_INVALID_CALL;
#if APP_RUNTIME_NETWORK_STEERING
    // When network steering is linked, use only plugin API to avoid scan callback conflicts.
    // Keep post-join behavior quiet and deterministic:
    // - skip steering-driven TC link key update workflow
    // - keep join focused on association/interview
    sli_zigbee_af_network_steering_options_mask = EMBER_AF_PLUGIN_NETWORK_STEERING_OPTIONS_NO_TCLK_UPDATE;
    join_status = emberAfPluginNetworkSteeringStart();
    APP_DEBUG_PRINTF("Join: emberAfPluginNetworkSteeringStart -> 0x%02x\n", join_status);
#else
    // Manual active scan + join path for builds without network steering plugin.
    join_status = start_join_scan();
#endif

    if (join_status != EMBER_SUCCESS) {
      emberAfCorePrintln("Join failed to start: 0x%x", join_status);
      if (join_status == EMBER_INVALID_CALL || join_status == 0xA8) {
        emberAfCorePrintln("Join aborted: stack not ready");
        network_join_in_progress = false;
        join_scan_in_progress = false;
        join_network_found = false;
        join_security_configured = false;
        app_set_join_retry_backoff(sl_sleeptimer_get_tick_count(), APP_DEBUG_JOIN_RETRY_BACKOFF_MS);
      } else {
        try_next_channel();
      }

#ifdef SL_CATALOG_SIMPLE_LED_PRESENT
      // Stop LED blinking on failure
      if (!network_join_in_progress) {
        led_blink_active = false;
        sl_zigbee_event_set_inactive(&led_blink_event);
        sl_led_turn_off(&sl_led_led0);
      }
#endif
    }
  }
}

static bool configure_join_security(void)
{
  EmberInitialSecurityState state;
  memset(&state, 0, sizeof(state));
  memcpy(state.preconfiguredKey.contents, zigbee_alliance_key, EMBER_ENCRYPTION_KEY_SIZE);
  state.bitmask = (EMBER_HAVE_PRECONFIGURED_KEY
                   | EMBER_REQUIRE_ENCRYPTED_KEY
                   | EMBER_TRUST_CENTER_GLOBAL_LINK_KEY);

  EmberStatus st = emberSetInitialSecurityState(&state);
  APP_DEBUG_PRINTF("Join: set security state -> 0x%02x\n", st);
  join_security_configured = (st == EMBER_SUCCESS);
  return join_security_configured;
}

/**
 * @brief Handle long button press (>=5 seconds)
 *
 * Long press toggles network state:
 * - joined: leave network
 * - not joined: start join
 */
static void handle_long_press(void)
{
  EmberNetworkStatus network_state = emberAfNetworkState();

  if (network_state == EMBER_JOINED_NETWORK) {
    emberAfCorePrintln("Long press: leaving network...");
    app_intentional_leave_pending = true;
    button_short_press_pending = false;
    button_long_press_pending = false;
    button_pressed = false;
    button_press_start_tick = 0;

    // Leave the network
    EmberStatus leave_status = emberLeaveNetwork();
    if (leave_status == EMBER_SUCCESS) {
      emberAfCorePrintln("Leave requested, waiting for network down");
    } else {
      app_intentional_leave_pending = false;
      emberAfCorePrintln("Failed to leave network: 0x%x", leave_status);
    }
  } else {
    emberAfCorePrintln("Long press: not joined, starting join");
    handle_short_press();
  }
}
