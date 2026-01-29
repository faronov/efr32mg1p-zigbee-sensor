# Zigbee Cluster Binding Guide

> **Status**: ✅ Implemented in v1.0.0 using **coordinator-side binding** (Option 1). The sensor is ready for binding - no firmware changes needed. Coordinators like Zigbee2MQTT, ZHA, and deCONZ can bind clusters directly.

## Overview
Enable your TRÅDFRI BME280 sensor to bind its measurement clusters to other Zigbee devices (coordinators, smart home hubs, or other devices) for direct reporting.

## What is Binding?

**Binding** creates a direct relationship between two Zigbee devices:
- **Source device** (your sensor) → **Destination device** (coordinator/hub)
- Allows sensor to send reports directly to bound device
- Reports bypass coordinator routing (more efficient)
- Enables device-to-device communication

## Current Configuration Status

✅ **Already Have:**
- Identify cluster (code 3) - required for binding discovery
- Temperature Measurement cluster (0x0402)
- Relative Humidity cluster (0x0405)
- Pressure Measurement cluster (0x0403)
- Power Configuration cluster (0x0001)

⚠️ **Need to Add:**
- Binding table support
- Bind/Unbind command handlers (optional - coordinators can bind without device support)

## Implementation Options

### Option 1: Coordinator-Side Binding (Simplest)

**What:** Let the coordinator/hub handle all binding
**When:** Use this if you control the coordinator
**Advantage:** No firmware changes needed!

Your sensor already has:
- ✅ Identify cluster for discovery
- ✅ Measurement clusters with reportable attributes
- ✅ Reporting configured in ZAP

**How to bind (from coordinator/hub):**

```bash
# Using Zigbee2MQTT
mosquitto_pub -t 'zigbee2mqtt/bridge/request/device/bind' -m '{"from":"sensor_ieee_address","to":"coordinator","clusters":["msTemperatureMeasurement","msRelativeHumidity","msPressureMeasurement"]}'

# Using ZHA (Home Assistant)
# GUI: Device → Bindings → Add Binding → Select clusters

# Using zigbee-herdsman
await endpoint.bind('msTemperatureMeasurement', coordinatorEndpoint);
await endpoint.bind('msRelativeHumidity', coordinatorEndpoint);
await endpoint.bind('msPressureMeasurement', coordinatorEndpoint);
```

### Option 2: Device-Side Binding Support (Full Feature)

**What:** Add binding commands to sensor firmware
**When:** For advanced use cases or direct device-to-device binding
**Advantage:** Device can manage its own bindings

## Option 2 Implementation Steps

### Step 1: Add Binding Components

**Edit:** `zigbee_bme280_sensor_tradfri.slcp`

Add after network steering component:

```yaml
  # Network joining
  - id: zigbee_network_steering
  - id: zigbee_scan_dispatch
  - id: zigbee_update_tc_link_key

  # Binding support
  - id: zigbee_binding_table
  - id: zigbee_end_device_bind
  - id: zigbee_simple_metering_server  # For binding reports
```

### Step 2: Configure Binding Table Size

**Add to configuration section:**

```yaml
configuration:
  # Binding table configuration
  - name: EMBER_BINDING_TABLE_SIZE
    value: 10  # Number of bindings device can store
```

### Step 3: Enable Binding in ZAP

**Edit:** `config/zcl/zcl_config.zap`

For each measurement cluster, ensure client commands are enabled:

**Temperature Measurement Cluster:**
```json
{
  "name": "Temperature Measurement",
  "code": 1026,
  "side": "server",
  "enabled": 1,
  "commands": [
    {
      "name": "Bind",
      "code": 33,  // ZCL_BIND_REQUEST_COMMAND_ID
      "source": "client",
      "incoming": true,
      "outgoing": false
    },
    {
      "name": "Unbind",
      "code": 34,  // ZCL_UNBIND_REQUEST_COMMAND_ID
      "source": "client",
      "incoming": true,
      "outgoing": false
    }
  ]
}
```

### Step 4: Implement Binding Callbacks (Optional)

**File:** `app.c`

**Add binding notification callback:**

```c
/**
 * @brief Callback when a binding table entry is set
 *
 * Called when a binding is created on this device.
 */
void emberAfSetBindingCallback(EmberBindingTableEntry *entry)
{
  emberAfCorePrintln("Binding created:");
  emberAfCorePrintln("  Type: %d", entry->type);
  emberAfCorePrintln("  Local EP: %d", entry->local);
  emberAfCorePrintln("  Remote EP: %d", entry->remote);
  emberAfCorePrintln("  Cluster: 0x%2X", entry->clusterId);

  // Print remote EUI64
  emberAfCorePrint("  Remote EUI64: ");
  emberAfPrintBigEndianEui64(entry->identifier);
  emberAfCorePrintln("");
}

/**
 * @brief Callback when a binding table entry is deleted
 */
void emberAfDeleteBindingCallback(uint8_t index)
{
  emberAfCorePrintln("Binding %d deleted", index);
}
```

### Step 5: Add CLI Commands for Manual Binding (Testing)

**Add binding management:**

```c
#include "app/framework/plugin/binding-table-library/binding-table-library.h"

/**
 * @brief Create a binding manually (for testing)
 */
void app_create_binding(uint8_t endpoint,
                        uint16_t clusterId,
                        EmberEUI64 destEui64,
                        uint8_t destEndpoint)
{
  EmberBindingTableEntry binding;

  binding.type = EMBER_UNICAST_BINDING;
  binding.local = endpoint;
  binding.clusterId = clusterId;
  binding.remote = destEndpoint;
  MEMCOPY(binding.identifier, destEui64, EUI64_SIZE);

  EmberStatus status = emberSetBinding(emberFindUnusedBindingIndex(), &binding);

  if (status == EMBER_SUCCESS) {
    emberAfCorePrintln("Binding created successfully");
  } else {
    emberAfCorePrintln("Binding failed: 0x%x", status);
  }
}

/**
 * @brief List all bindings
 */
void app_print_bindings(void)
{
  uint8_t i;
  EmberBindingTableEntry entry;

  emberAfCorePrintln("Binding Table (%d entries):", EMBER_BINDING_TABLE_SIZE);

  for (i = 0; i < EMBER_BINDING_TABLE_SIZE; i++) {
    EmberStatus status = emberGetBinding(i, &entry);

    if (status == EMBER_SUCCESS && entry.type != EMBER_UNUSED_BINDING) {
      emberAfCorePrintln("  [%d] EP %d → Cluster 0x%2X → EP %d",
                         i, entry.local, entry.clusterId, entry.remote);
      emberAfCorePrint("      Remote: ");
      emberAfPrintBigEndianEui64(entry.identifier);
      emberAfCorePrintln("");
    }
  }
}
```

## Automatic Binding on Join (Advanced)

**Add to `emberAfStackStatusCallback`:**

```c
void emberAfStackStatusCallback(EmberStatus status)
{
  if (status == EMBER_NETWORK_UP) {
    emberAfCorePrintln("Network joined successfully");

    // ... existing code ...

    // Auto-bind to coordinator after joining
    EmberEUI64 coordinatorEui64;
    emberAfGetEui64(coordinatorEui64);

    // Bind temperature cluster
    app_create_binding(SENSOR_ENDPOINT,
                       ZCL_TEMP_MEASUREMENT_CLUSTER_ID,
                       coordinatorEui64,
                       1);  // Coordinator typically on endpoint 1

    // Bind humidity cluster
    app_create_binding(SENSOR_ENDPOINT,
                       ZCL_HUMIDITY_MEASUREMENT_CLUSTER_ID,
                       coordinatorEui64,
                       1);

    // Bind pressure cluster
    app_create_binding(SENSOR_ENDPOINT,
                       ZCL_PRESSURE_MEASUREMENT_CLUSTER_ID,
                       coordinatorEui64,
                       1);

    emberAfCorePrintln("Auto-binding to coordinator complete");
  }
}
```

## Testing Binding

### 1. Verify Bindings Are Created

**Using CLI (if available):**
```bash
# On sensor device
plugin binding-table print

# Expected output:
# idx | src ep | clusterId | dest ep | destEUI64
# 0   | 1      | 0x0402    | 1       | coordinator_eui64
# 1   | 1      | 0x0405    | 1       | coordinator_eui64
# 2   | 1      | 0x0403    | 1       | coordinator_eui64
```

### 2. Verify Reports Are Sent

**Check coordinator/hub logs:**
```
[Sensor] Temperature report: 23.45°C (via binding)
[Sensor] Humidity report: 45.2% (via binding)
[Sensor] Pressure report: 101.3 kPa (via binding)
```

### 3. Sniffer Verification

**Use Zigbee sniffer to confirm:**
- Reports are sent directly to bound device
- Not broadcast to network
- Uses unicast addressing

## Reporting Configuration

**Already configured in ZAP** (reportable attributes):

```json
"reportable": 1,
"minInterval": 60,      // Min 60 seconds between reports
"maxInterval": 3600,    // Max 1 hour between reports
"reportableChange": 50  // Report if value changes by 0.5°C (for temp)
```

**Bind + Report = Efficient Updates:**
- Sensor only sends when value changes significantly
- Reports go directly to bound device
- No coordinator routing overhead

## Binding Table Memory Usage

**Size calculation:**
```c
// Each binding entry: ~16 bytes
// 10 bindings = 160 bytes RAM
// 3 bindings (our use case) = 48 bytes RAM
```

**Recommendation for TRÅDFRI (limited RAM):**
```yaml
- name: EMBER_BINDING_TABLE_SIZE
  value: 5  # Enough for 3 clusters + 2 spare
```

## Benefits of Binding

### Power Savings
- ✅ Reports sent only when needed (reportable change threshold)
- ✅ No coordinator relay (direct unicast)
- ✅ Fewer network hops

### Reliability
- ✅ Direct communication path
- ✅ Coordinator failure doesn't break sensor→device link
- ✅ Lower packet loss

### Network Efficiency
- ✅ Less broadcast traffic
- ✅ Reduced coordinator load
- ✅ Better for large networks

## Common Use Cases

### 1. Bind to Coordinator
```c
// Temperature → Coordinator endpoint 1
// Most common: Home Assistant, Zigbee2MQTT, deCONZ
```

### 2. Bind to Smart Plug
```c
// Temperature → Smart plug endpoint 1
// Use case: Turn on heater when temp drops
// Requires plug to handle temp cluster commands
```

### 3. Bind to Display Device
```c
// Temperature → E-ink display endpoint 1
// Direct sensor→display updates
```

## Troubleshooting

### Binding Not Working

**Check:**
1. Binding table has free entries: `plugin binding-table print`
2. Destination device supports cluster: Check device capabilities
3. Reporting is configured: Check ZAP reportable attributes
4. Network key is current: Bindings fail if keys don't match

### Reports Not Received

**Check:**
1. Destination endpoint is correct (usually 1)
2. Destination device has cluster enabled
3. Reportable change threshold is reasonable
4. Report intervals are configured

### Memory Issues

**If running out of RAM:**
```yaml
# Reduce binding table size
- name: EMBER_BINDING_TABLE_SIZE
  value: 3  # Minimum for 3 clusters
```

## References

- [Zigbee Binding Specification - ZCL 2.5](https://zigbeealliance.org/wp-content/uploads/2019/11/docs-07-5123-05-zigbee-cluster-library-specification.pdf)
- [Silicon Labs AN1233: Zigbee Binding](https://www.silabs.com/documents/public/application-notes/an1233-zigbee-binding.pdf)
- [Zigbee Application Framework API Reference - Binding](https://docs.silabs.com/zigbee/latest/zigbee-af-api/binding)

## Summary

**For quick setup:** Use Option 1 (Coordinator-side binding)
- No firmware changes
- Works with Zigbee2MQTT, ZHA, deCONZ
- Bindings managed by hub

**For full control:** Use Option 2 (Device-side binding)
- Add binding components to SLCP
- Implement callbacks
- Device can manage own bindings
- Better for device-to-device communication

Your sensor already has everything needed for coordinator-side binding! 🎯
