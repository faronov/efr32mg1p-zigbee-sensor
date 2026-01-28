/**
 * @file app.c
 * @brief Application logic for Zigbee BME280 Sensor
 */

#include "sl_component_catalog.h"
#include "zigbee_app_framework_event.h"
#include "app/framework/include/af.h"
#include "app/framework/plugin/network-steering/network-steering.h"
#include "app_sensor.h"

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

// Network join retry with exponential backoff
static sl_zigbee_event_t network_join_retry_event;
static uint8_t join_attempt_count = 0;
static const uint32_t join_retry_delays_ms[] = {
  0,        // First attempt: immediate
  30000,    // 30 seconds
  60000,    // 1 minute
  120000,   // 2 minutes
  300000,   // 5 minutes
  600000    // 10 minutes (max backoff)
};
#define MAX_JOIN_RETRY_DELAY_INDEX (sizeof(join_retry_delays_ms) / sizeof(join_retry_delays_ms[0]) - 1)

// Forward declarations
static void led_blink_event_handler(sl_zigbee_event_t *event);
static void led_off_event_handler(sl_zigbee_event_t *event);
static void network_join_retry_event_handler(sl_zigbee_event_t *event);

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

  // Initialize network join retry event
  sl_zigbee_event_init(&network_join_retry_event, network_join_retry_event_handler);

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

    // Reset join attempt counter on success
    join_attempt_count = 0;
    sl_zigbee_event_set_inactive(&network_join_retry_event);

    // Stop LED blinking
    led_blink_active = false;
    sl_zigbee_event_set_inactive(&led_blink_event);

#ifdef SL_CATALOG_SIMPLE_LED_PRESENT
    // Turn LED on solid to indicate network is up
    sl_led_turn_on(&sl_led_led0);

    // Schedule LED to turn off after 3 seconds to save power
    sl_zigbee_event_set_delay_ms(&led_off_event, 3000);
#endif

    // Perform initial sensor reading after joining
    app_sensor_update();

  } else if (status == EMBER_NETWORK_DOWN) {
    emberAfCorePrintln("Network down - entering low power mode");

#ifdef SL_CATALOG_SIMPLE_LED_PRESENT
    // Turn LED off when network is down
    sl_led_turn_off(&sl_led_led0);
    // Cancel any pending LED off event
    sl_zigbee_event_set_inactive(&led_off_event);
#endif

    // Sensor timer will automatically stop reading (see app_sensor.c)
    // Device will use minimal power while waiting for network
  }
}

/**
 * @brief Button press callback
 *
 * Called when a button is pressed on the board.
 */
#ifdef SL_CATALOG_SIMPLE_BUTTON_PRESENT
void sl_button_on_change(const sl_button_t *handle)
{
  // Only handle button press (not release)
  if (sl_button_get_state(handle) != SL_SIMPLE_BUTTON_PRESSED) {
    return;
  }

  if (handle == &sl_button_btn0) {
    EmberNetworkStatus network_state = emberAfNetworkState();

    if (network_state == EMBER_JOINED_NETWORK) {
      // Already on network - trigger sensor reading
      emberAfCorePrintln("Button pressed: Reading sensor...");
      app_sensor_update();

#ifdef SL_CATALOG_SIMPLE_LED_PRESENT
      // Flash LED briefly to indicate sensor read
      sl_led_turn_on(&sl_led_led0);
      sl_sleeptimer_delay_millisecond(200);
      sl_led_turn_off(&sl_led_led0);
#endif

    } else {
      // Not on network - start network steering with retry logic
      emberAfCorePrintln("Button pressed: Joining network (attempt %d)...",
                         join_attempt_count + 1);

#ifdef SL_CATALOG_SIMPLE_LED_PRESENT
      // Start LED blinking to indicate joining
      led_blink_active = true;
      sl_zigbee_event_set_active(&led_blink_event);
#endif

      // Start network steering
      EmberStatus join_status = emberAfPluginNetworkSteeringStart();

      if (join_status != EMBER_SUCCESS) {
        emberAfCorePrintln("Join failed to start: 0x%x", join_status);

        // Schedule retry with exponential backoff
        uint8_t delay_index = (join_attempt_count < MAX_JOIN_RETRY_DELAY_INDEX)
                              ? join_attempt_count
                              : MAX_JOIN_RETRY_DELAY_INDEX;
        uint32_t retry_delay = join_retry_delays_ms[delay_index];

        emberAfCorePrintln("Will retry in %d seconds", retry_delay / 1000);
        sl_zigbee_event_set_delay_ms(&network_join_retry_event, retry_delay);
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
 * @brief Network join retry event handler
 *
 * Automatically retries network joining with exponential backoff
 * when network is not available.
 */
static void network_join_retry_event_handler(sl_zigbee_event_t *event)
{
  emberAfCorePrintln("Auto-retry: Attempting to join network (attempt %d)...",
                     join_attempt_count + 1);

#ifdef SL_CATALOG_SIMPLE_LED_PRESENT
  // Start LED blinking to indicate joining
  led_blink_active = true;
  sl_zigbee_event_set_active(&led_blink_event);
#endif

  // Attempt to join
  EmberStatus join_status = emberAfPluginNetworkSteeringStart();

  if (join_status != EMBER_SUCCESS) {
    emberAfCorePrintln("Join attempt failed: 0x%x", join_status);

    // Schedule next retry with exponential backoff
    uint8_t delay_index = (join_attempt_count < MAX_JOIN_RETRY_DELAY_INDEX)
                          ? join_attempt_count
                          : MAX_JOIN_RETRY_DELAY_INDEX;
    uint32_t retry_delay = join_retry_delays_ms[delay_index];

    emberAfCorePrintln("Will retry in %d seconds", retry_delay / 1000);
    sl_zigbee_event_set_delay_ms(&network_join_retry_event, retry_delay);
    join_attempt_count++;

#ifdef SL_CATALOG_SIMPLE_LED_PRESENT
    // Stop LED blinking on failure
    led_blink_active = false;
    sl_zigbee_event_set_inactive(&led_blink_event);
    sl_led_turn_off(&sl_led_led0);
#endif
  }
}
