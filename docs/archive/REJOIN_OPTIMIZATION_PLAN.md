# Optimized Rejoin Implementation Plan (GSDK 4.5.0)

## Overview
Implement fast rejoin optimization for TRÅDFRI sensor using modern GSDK 4.5.0 APIs.
Goal: Reduce rejoin time from ~2.2s (full scan) to ~138ms (current channel only).

## Modern API Approach (No Stack Modification)

### Available APIs
1. `emberFindAndRejoinNetwork(bool haveKey, uint32_t channelMask)`
2. `emberRejoinNetwork(bool haveKey)` - convenience function (current channel only)
3. `emberGetRadioChannel()` - get current channel number
4. `BIT32(n)` - convert channel to bitmask

### Implementation Phases

## Phase 1: Add Optimized Rejoin State Machine

**File:** `app.c`

**Add State Enum:**
```c
typedef enum {
  REJOIN_STATE_IDLE,
  REJOIN_STATE_CURRENT_CHANNEL,
  REJOIN_STATE_ALL_CHANNELS,
  REJOIN_STATE_DONE
} RejoinState_t;

static RejoinState_t rejoin_state = REJOIN_STATE_IDLE;
static uint8_t saved_channel = 0;
```

**Add Rejoin Event:**
```c
static sl_zigbee_event_t rejoin_retry_event;

static void rejoin_retry_event_handler(sl_zigbee_event_t *event)
{
  switch (rejoin_state) {
    case REJOIN_STATE_CURRENT_CHANNEL:
      // Timeout waiting for rejoin on current channel
      emberAfCorePrintln("Rejoin on channel %d failed, trying all channels...", saved_channel);
      rejoin_state = REJOIN_STATE_ALL_CHANNELS;

      // Try all channels
      EmberStatus status = emberFindAndRejoinNetwork(true, EMBER_ALL_802_15_4_CHANNELS_MASK);
      if (status != EMBER_SUCCESS) {
        emberAfCorePrintln("Full channel scan failed: 0x%x", status);
        rejoin_state = REJOIN_STATE_IDLE;
      }
      break;

    case REJOIN_STATE_ALL_CHANNELS:
      // Timeout on full scan - give up
      emberAfCorePrintln("Rejoin failed on all channels");
      rejoin_state = REJOIN_STATE_IDLE;
      break;

    default:
      break;
  }
}
```

## Phase 2: Implement Optimized Rejoin Function

**Add to app.c:**
```c
/**
 * @brief Start optimized rejoin - try current channel first
 *
 * Attempts rejoin on current channel first for fast reconnection.
 * Falls back to full channel scan after timeout.
 */
static void start_optimized_rejoin(void)
{
  // Get current channel
  saved_channel = emberGetRadioChannel();
  uint32_t channel_mask = BIT32(saved_channel);

  emberAfCorePrintln("Starting optimized rejoin on channel %d", saved_channel);

  rejoin_state = REJOIN_STATE_CURRENT_CHANNEL;

  // Try current channel only (fast path)
  EmberStatus status = emberFindAndRejoinNetwork(true, channel_mask);

  if (status == EMBER_SUCCESS) {
    // Schedule timeout to try all channels if current fails
    // 500ms timeout - enough for current channel attempt
    sl_zigbee_event_set_delay_ms(&rejoin_retry_event, 500);
  } else {
    emberAfCorePrintln("Rejoin start failed: 0x%x", status);
    rejoin_state = REJOIN_STATE_IDLE;
  }
}

/**
 * @brief Alternative: Use convenience API (even simpler)
 */
static void start_optimized_rejoin_simple(void)
{
  emberAfCorePrintln("Starting optimized rejoin (current channel only)");

  rejoin_state = REJOIN_STATE_CURRENT_CHANNEL;

  // Use convenience function - automatically tries current channel only
  EmberStatus status = emberRejoinNetwork(true);

  if (status == EMBER_SUCCESS) {
    // Schedule timeout to try all channels if current fails
    sl_zigbee_event_set_delay_ms(&rejoin_retry_event, 500);
  } else {
    emberAfCorePrintln("Rejoin start failed: 0x%x", status);
    rejoin_state = REJOIN_STATE_IDLE;
  }
}
```

## Phase 3: Hook into Network Status Callback

**Modify emberAfStackStatusCallback:**
```c
void emberAfStackStatusCallback(EmberStatus status)
{
  if (status == EMBER_NETWORK_UP) {
    emberAfCorePrintln("Network joined successfully");

    // Cancel any pending rejoin retry
    rejoin_state = REJOIN_STATE_DONE;
    sl_zigbee_event_set_inactive(&rejoin_retry_event);

    // ... existing code ...

  } else if (status == EMBER_NETWORK_DOWN) {
    emberAfCorePrintln("Network down - will attempt optimized rejoin");

    // ... existing code ...

    // Start optimized rejoin after network down
    // Small delay to ensure stack is ready
    sl_zigbee_event_set_delay_ms(&rejoin_retry_event, 100);
  }
}
```

## Phase 4: Initialize Rejoin Event

**Add to emberAfInitCallback:**
```c
void emberAfInitCallback(void)
{
  // Initialize rejoin retry event
  sl_zigbee_event_init(&rejoin_retry_event, rejoin_retry_event_handler);

  // ... existing initialization ...
}
```

## Phase 5: Add CLI Commands (Optional)

**For testing:**
```c
void emberAfPluginNetworkSteeringCompleteCallback(EmberStatus status,
                                                   uint8_t totalBeacons,
                                                   uint8_t joinAttempts,
                                                   uint8_t finalState)
{
  if (status == EMBER_SUCCESS) {
    emberAfCorePrintln("Network steering complete - joined successfully");
    rejoin_state = REJOIN_STATE_DONE;
  } else {
    emberAfCorePrintln("Network steering failed: 0x%x", status);
  }
}
```

## Expected Performance

### Before Optimization (Full Scan)
- **Time:** ~2.2 seconds worst case, ~968ms typical
- **Channels scanned:** All 16 channels (11-26)
- **Power consumption:** High (radio active during entire scan)

### After Optimization (Current Channel First)
- **Time (success):** ~138ms
- **Time (fallback):** ~138ms + 500ms timeout + full scan = ~2.6s worst case
- **Channels scanned:** 1 channel (success), 16 channels (fallback)
- **Power consumption:**
  - Success: 7x less power (1 channel vs 16)
  - Failure: Slightly more (timeout overhead)

### Real-World Benefit
- **Typical case:** Network unchanged, rejoin succeeds immediately
- **Power savings:** ~85% reduction in rejoin power consumption
- **Battery life impact:** Significant for 2xAAA sensor with frequent reconnections

## Testing Strategy

1. **Test normal operation:**
   - Power cycle device
   - Verify quick rejoin on current channel
   - Check logs show "~138ms" rejoin time

2. **Test channel change scenario:**
   - Change coordinator channel
   - Power cycle device
   - Verify fallback to full scan works
   - Check logs show timeout then full scan

3. **Test network unavailable:**
   - Turn off coordinator
   - Power cycle device
   - Verify graceful timeout and retry logic

4. **Measure power consumption:**
   - Use current measurement tool
   - Compare before/after optimization
   - Verify expected power savings

## Configuration Options

**Tunable parameters:**
```c
#define REJOIN_CURRENT_CHANNEL_TIMEOUT_MS  500  // Time to wait before full scan
#define REJOIN_FULL_SCAN_TIMEOUT_MS       5000  // Time to wait for full scan
#define REJOIN_RETRY_DELAY_MS              100  // Delay before starting rejoin
```

## Advantages Over Deprecated Example

1. ✅ **No stack modification** - uses public APIs only
2. ✅ **GSDK 4.5.0 compatible** - modern API usage
3. ✅ **Maintainable** - no forked SDK code
4. ✅ **Simple** - state machine with callbacks
5. ✅ **Testable** - clear states and timeouts

## Next Steps

1. Implement Phase 1-4 in app.c
2. Test on TRÅDFRI hardware
3. Measure actual rejoin times
4. Tune timeout values if needed
5. Add metrics/logging for monitoring

## References

- [Network Formation API - Silicon Labs](https://docs.silabs.com/d/zigbee-stack-api/7.2.2/network-formation)
- [Stack Information API - Silicon Labs](https://docs.silabs.com/zigbee/7.1/zigbee-stack-api/stack-info)
- [Deprecated Example](https://github.com/SiliconLabsSoftware/zigbee_applications/tree/5a5c996021a4f6728181a37e6051f8d571999082/deprecated/zigbee_optimize_bootup_rejoin)
