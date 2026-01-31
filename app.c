/**
 * @file app.c
 * @brief Application logic for Zigbee BME280 Sensor
 */

#include "sl_component_catalog.h"
#include "zigbee_app_framework_event.h"
#include "app/framework/include/af.h"
// #include "app/framework/plugin/network-steering/network-steering.h" // REMOVED - causes event queue crash
#include "app_sensor.h"
#include "app_config.h"
#include "stack/include/network-formation.h"  // For manual network join

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

// Network join channel scan timeout
static sl_zigbee_event_t channel_scan_timeout_event;

// Button action deferred event (to get out of ISR context)
static sl_zigbee_event_t button_action_event;

static uint8_t join_attempt_count = 0;
#define CHANNEL_SCAN_TIMEOUT_MS 8000  // Wait 8 seconds per channel before trying next

// Button action types
typedef enum {
  BUTTON_ACTION_NONE,
  BUTTON_ACTION_SHORT_PRESS,
  BUTTON_ACTION_LONG_PRESS
} ButtonAction_t;

static ButtonAction_t pending_button_action = BUTTON_ACTION_NONE;

// Button press duration tracking
static uint32_t button_press_start_tick = 0;
static bool button_pressed = false;
#define LONG_PRESS_THRESHOLD_MS 5000  // 5 seconds for long press (leave/rejoin network)

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
#define BIT32(n) (((uint32_t)1) << (n))

// Zigbee 3.0 channels (11-26)
#define ZIGBEE_CHANNELS_MASK 0x07FFF800

// Single-channel network join state (Series 1 workaround for event queue limitation)
// Try channels in order of popularity: 15, 20, 25, 11, 14, 19, 24, 26, 12, 13, 16, 17, 18, 21, 22, 23
static const uint8_t channel_scan_order[] = {15, 20, 25, 11, 14, 19, 24, 26, 12, 13, 16, 17, 18, 21, 22, 23};
static uint8_t current_channel_index = 0;  // Index into channel_scan_order array
static bool network_join_in_progress = false;

// Forward declarations
static void led_blink_event_handler(sl_zigbee_event_t *event);
static void led_off_event_handler(sl_zigbee_event_t *event);
static void channel_scan_timeout_event_handler(sl_zigbee_event_t *event);
static void button_action_event_handler(sl_zigbee_event_t *event);
static void rejoin_retry_event_handler(sl_zigbee_event_t *event);
static void start_optimized_rejoin(void);
static void handle_short_press(void);
static void handle_long_press(void);
static EmberStatus manual_network_join(void);
static void try_next_channel(void);

/**
 * @brief Zigbee application init callback
 *
 * Called when the Zigbee stack has completed initialization.
 * This is where we initialize application components.
 */
void emberAfInitCallback(void)
{
  emberAfCorePrintln("Zigbee BME280 Sensor Application");
  emberAfCorePrintln("Silicon Labs EFR32MG1P + Bosch BME280");
  emberAfCorePrintln("Press BTN0 to join network or trigger sensor reading");

  // Initialize LED events
  sl_zigbee_event_init(&led_blink_event, led_blink_event_handler);
  sl_zigbee_event_init(&led_off_event, led_off_event_handler);

  // Initialize channel scan timeout event for sequential scanning
  sl_zigbee_event_init(&channel_scan_timeout_event, channel_scan_timeout_event_handler);

  // Initialize button action event (defers work out of ISR context)
  sl_zigbee_event_init(&button_action_event, button_action_event_handler);

  // Initialize optimized rejoin event (TEMPORARILY DISABLED - event queue issue)
  // sl_zigbee_event_init(&rejoin_retry_event, rejoin_retry_event_handler);

  // Initialize configuration from NVM
  app_config_init();

  // TEMPORARILY DISABLE sensor to reduce event usage (event queue issue)
  // Initialize BME280 sensor
  // if (!app_sensor_init()) {
  //   emberAfCorePrintln("ERROR: Sensor initialization failed!");
  // #ifdef SL_CATALOG_SIMPLE_LED_PRESENT
  //   // Rapid blink on error
  //   for (int i = 0; i < 10; i++) {
  //     sl_led_toggle(&sl_led_led0);
  //     sl_sleeptimer_delay_millisecond(100);
  //   }
  // #endif
  // }
  emberAfCorePrintln("Sensor DISABLED for testing - event queue issue");
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

    // Reset join attempt counter and scan state on success
    join_attempt_count = 0;
    current_channel_index = 0;
    network_join_in_progress = false;

    // Cancel channel scan timeout - we found a network!
    sl_zigbee_event_set_inactive(&channel_scan_timeout_event);

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

    // Sensor DISABLED for testing (event queue issue)
    // Perform initial sensor reading after joining
    // app_sensor_update();

    // Restart periodic sensor updates
    // app_sensor_start_periodic_updates();

    // Note: Binding is handled by coordinator (Zigbee2MQTT/ZHA/deCONZ)
    // No device-side binding code needed - see BINDING_GUIDE.md

  } else if (status == EMBER_NETWORK_DOWN) {
    emberAfCorePrintln("Network down - will attempt optimized rejoin");

#ifdef SL_CATALOG_SIMPLE_LED_PRESENT
    // Turn LED off when network is down
    sl_led_turn_off(&sl_led_led0);
    // Cancel any pending LED off event
    sl_zigbee_event_set_inactive(&led_off_event);
#endif

    // Sensor timer will automatically stop reading when event handler checks network state
    // Device will use minimal power while waiting for network

    // Optimized rejoin TEMPORARILY DISABLED - event queue issue
    // emberAfCorePrintln("Scheduling optimized rejoin in 100ms...");
    // rejoin_state = REJOIN_STATE_IDLE;
    // sl_zigbee_event_set_delay_ms(&rejoin_retry_event, 100);
  }
}

/**
 * @brief Button press callback
 *
 * Called when a button is pressed or released on the board.
 * Detects short press (<5s) vs long press (>=5s).
 *
 * CRITICAL: This runs in ISR context (GPIO_ODD_IRQn)!
 * CANNOT call ANY Zigbee functions including emberAfCorePrintln()!
 * Only allowed: set variables, schedule events with sl_zigbee_event_set_active()
 */
#ifdef SL_CATALOG_SIMPLE_BUTTON_PRESENT
void sl_button_on_change(const sl_button_t *handle)
{
  if (handle == &sl_button_btn0) {
    if (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED) {
      // Button pressed - record start time
      button_press_start_tick = sl_sleeptimer_get_tick_count();
      button_pressed = true;
      // NO emberAfCorePrintln() here - it schedules events!
    } else {
      // Button released - check duration
      if (button_pressed) {
        uint32_t duration_ticks = sl_sleeptimer_get_tick_count() - button_press_start_tick;
        uint32_t duration_ms = sl_sleeptimer_tick_to_ms(duration_ticks);

        // NO emberAfCorePrintln() here - runs in ISR context!

        if (duration_ms >= LONG_PRESS_THRESHOLD_MS) {
          // Long press: Leave and rejoin network
          pending_button_action = BUTTON_ACTION_LONG_PRESS;
        } else {
          // Short press: Immediate sensor read and report
          pending_button_action = BUTTON_ACTION_SHORT_PRESS;
        }

        // Schedule event to handle action in main context (not ISR context)
        // This is safe - sl_zigbee_event_set_active() can be called from ISR
        sl_zigbee_event_set_active(&button_action_event);

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
 * @brief Button action event handler
 *
 * Handles button actions deferred from ISR context. This runs in main
 * context where it's safe to call Zigbee stack functions.
 */
static void button_action_event_handler(sl_zigbee_event_t *event)
{
  (void)event;  // Unused parameter

  ButtonAction_t action = pending_button_action;
  pending_button_action = BUTTON_ACTION_NONE;

  switch (action) {
    case BUTTON_ACTION_SHORT_PRESS:
      emberAfCorePrintln("Button: Short press - processing in main context");
      handle_short_press();
      break;

    case BUTTON_ACTION_LONG_PRESS:
      emberAfCorePrintln("Button: Long press - processing in main context");
      handle_long_press();
      break;

    case BUTTON_ACTION_NONE:
    default:
      emberAfCorePrintln("Button: No action pending");
      break;
  }
}

/**
 * @brief Channel scan timeout handler
 *
 * If we haven't joined after scanning current channel, try the next channel.
 * This implements sequential channel scanning for Series 1 chips.
 */
static void channel_scan_timeout_event_handler(sl_zigbee_event_t *event)
{
  (void)event;  // Unused parameter

  // Check if we joined while waiting
  EmberNetworkStatus network_state = emberAfNetworkState();
  if (network_state == EMBER_JOINED_NETWORK) {
    emberAfCorePrintln("Network joined during scan - stopping");
    network_join_in_progress = false;
    current_channel_index = 0;
    return;
  }

  // Not joined yet - try next channel
  emberAfCorePrintln("Channel scan timeout - trying next channel");
  try_next_channel();
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

  // Move to next channel
  current_channel_index++;

  if (current_channel_index >= total_channels) {
    // Exhausted all channels - give up
    emberAfCorePrintln("All channels scanned - no network found");
    network_join_in_progress = false;
    current_channel_index = 0;
    join_attempt_count++;

#ifdef SL_CATALOG_SIMPLE_LED_PRESENT
    // Stop LED blinking
    led_blink_active = false;
    sl_zigbee_event_set_inactive(&led_blink_event);
    sl_led_turn_off(&sl_led_led0);
#endif

    return;
  }

  // Try next channel
  manual_network_join();
}

/**
 * @brief Manual network join using low-level Zigbee APIs
 *
 * Replacement for network steering plugin that caused event queue overflow.
 *
 * Series 1 Workaround: Scans ONE channel at a time instead of all 16 channels
 * simultaneously. This prevents event queue overflow on Series 1 chips with
 * limited RAM (32KB).
 *
 * Strategy: Try channels in order of popularity (15, 20, 25, 11, ...) with
 * 8-second timeout between channels. If network found, scanning stops.
 * If all 16 channels scanned with no network, gives up.
 */
static EmberStatus manual_network_join(void)
{
  const uint8_t total_channels = sizeof(channel_scan_order) / sizeof(channel_scan_order[0]);

  if (current_channel_index >= total_channels) {
    emberAfCorePrintln("ERROR: Invalid channel index %d", current_channel_index);
    return EMBER_INVALID_CALL;
  }

  uint8_t channel_to_scan = channel_scan_order[current_channel_index];
  uint32_t single_channel_mask = BIT32(channel_to_scan);

  emberAfCorePrintln("Scanning channel %d (%d of %d)...",
                     channel_to_scan,
                     current_channel_index + 1,
                     total_channels);

  // Scan ONLY this one channel - uses minimal events (~1-2 instead of 16+)
  EmberStatus status = emberFindAndRejoinNetwork(true, single_channel_mask);

  if (status == EMBER_SUCCESS) {
    emberAfCorePrintln("Channel %d scan started", channel_to_scan);
    network_join_in_progress = true;

    // Set timeout to try next channel if this one doesn't work
    sl_zigbee_event_set_delay_ms(&channel_scan_timeout_event, CHANNEL_SCAN_TIMEOUT_MS);
  } else {
    emberAfCorePrintln("Failed to start scan on channel %d: 0x%x", channel_to_scan, status);
    // Try next channel immediately
    try_next_channel();
  }

  return status;
}

/**
 * @brief Handle short button press (<5 seconds)
 *
 * Short press triggers immediate sensor read and report.
 * If not joined to network, starts network joining.
 */
static void handle_short_press(void)
{
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
    // Not on network - start manual network join
    emberAfCorePrintln("Not joined - starting network join (attempt %d)...",
                       join_attempt_count + 1);

    // Reset to start of channel list
    current_channel_index = 0;

#ifdef SL_CATALOG_SIMPLE_LED_PRESENT
    // Start LED blinking to indicate joining
    led_blink_active = true;
    sl_zigbee_event_set_active(&led_blink_event);
#endif

    // Start manual network join (no longer using network steering plugin)
    EmberStatus join_status = manual_network_join();

    if (join_status != EMBER_SUCCESS) {
      emberAfCorePrintln("Join failed to start: 0x%x", join_status);
      join_attempt_count++;

#ifdef SL_CATALOG_SIMPLE_LED_PRESENT
      // Stop LED blinking on failure
      led_blink_active = false;
      sl_zigbee_event_set_inactive(&led_blink_event);
      sl_led_turn_off(&sl_led_led0);
#endif
    }
  }
}

/**
 * @brief Handle long button press (>=5 seconds)
 *
 * Long press causes the device to leave the current network
 * and immediately attempt to rejoin (useful for moving to a new coordinator).
 */
static void handle_long_press(void)
{
  EmberNetworkStatus network_state = emberAfNetworkState();

  if (network_state == EMBER_JOINED_NETWORK) {
    emberAfCorePrintln("Leaving network and rejoining...");

#ifdef SL_CATALOG_SIMPLE_LED_PRESENT
    // Flash LED rapidly to indicate leave operation
    for (int i = 0; i < 5; i++) {
      sl_led_toggle(&sl_led_led0);
      sl_sleeptimer_delay_millisecond(100);
    }
#endif

    // Leave the network
    EmberStatus leave_status = emberLeaveNetwork();
    if (leave_status == EMBER_SUCCESS) {
      emberAfCorePrintln("Left network successfully");

      // Reset join attempt counter and channel index for new join
      join_attempt_count = 0;
      current_channel_index = 0;

      // Small delay to ensure leave completes
      sl_sleeptimer_delay_millisecond(500);

#ifdef SL_CATALOG_SIMPLE_LED_PRESENT
      // Start LED blinking to indicate joining
      led_blink_active = true;
      sl_zigbee_event_set_active(&led_blink_event);
#endif

      // Start manual network join to rejoin (no longer using network steering plugin)
      EmberStatus join_status = manual_network_join();
      if (join_status != EMBER_SUCCESS) {
        emberAfCorePrintln("Failed to start rejoin: 0x%x", join_status);
#ifdef SL_CATALOG_SIMPLE_LED_PRESENT
        led_blink_active = false;
        sl_zigbee_event_set_inactive(&led_blink_event);
        sl_led_turn_off(&sl_led_led0);
#endif
      }
    } else {
      emberAfCorePrintln("Failed to leave network: 0x%x", leave_status);
    }
  } else {
    emberAfCorePrintln("Not joined to network - long press ignored");
  }
}
