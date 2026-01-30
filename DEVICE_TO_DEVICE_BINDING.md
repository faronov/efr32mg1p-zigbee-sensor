# Device-to-Device Binding Guide

## Overview

Device-to-device binding allows your TRÅDFRI BME280 sensor to send measurement reports **directly** to other Zigbee devices (like smart plugs, displays, or controllers) without going through the coordinator.

**Use Cases:**
- **Sensor → Smart Plug**: Turn on heater when temperature drops below threshold
- **Sensor → Display**: Show temperature/humidity on e-ink display
- **Sensor → Controller**: Direct reporting to custom Zigbee device

## What's Different from Coordinator Binding?

| Feature | Coordinator Binding | Device-to-Device Binding |
|---------|-------------------|-------------------------|
| **Setup** | Coordinator manages all bindings | Devices maintain own binding table |
| **Reports** | Sensor → Coordinator only | Sensor → Multiple devices directly |
| **Network load** | All traffic through coordinator | Direct unicast (more efficient) |
| **Use case** | Standard home automation | Advanced automation, custom devices |
| **Firmware support** | ✅ Enabled by default | ✅ **Now enabled in v1.0.1** |

## Prerequisites

✅ **Firmware**: v1.0.1 or later (device-side binding support)
✅ **Coordinator**: ZHA, Zigbee2MQTT, deCONZ, or any Zigbee 3.0 coordinator
✅ **Target device**: Must support receiving the measurement clusters

## How It Works

1. **Binding table** stored on sensor (5 entries, ~80 bytes RAM)
2. **Coordinator creates binding** using ZDO Bind Request
3. **Sensor stores binding** and receives confirmation callback
4. **Reports sent directly** to bound device(s) when measurements change

### Binding Flow

```
Coordinator                    Sensor                    Smart Plug
    |                            |                            |
    |--- ZDO Bind Request ------>|                            |
    |    (Sensor → Plug)         |                            |
    |                            |--- Binding created ------->|
    |                            |                            |
    |<-- Bind Response ----------|                            |
    |                            |                            |
    |                            |-- Temperature Report ----->|
    |                            |    (direct unicast)        |
    |                            |                            |
```

## Setup with ZHA (Home Assistant)

### Method 1: ZHA GUI (Easiest)

1. **Navigate to ZHA Integration**
   - Home Assistant → Settings → Devices & Services → Zigbee Home Automation

2. **Select Your Sensor**
   - Click on "TRÅDFRI BME280 Sensor" device

3. **Create Binding**
   - Go to "Clusters" tab
   - Find measurement cluster (Temperature 0x0402, Humidity 0x0405, or Pressure 0x0403)
   - Click "Bind to device"
   - Select target device (e.g., smart plug)
   - Click "Bind"

4. **Verify Binding**
   - Check sensor debug logs for "Binding created" message
   - Verify reports are received by target device

### Method 2: ZHA Service Call

Use Home Assistant Developer Tools → Services:

```yaml
service: zha.issue_zigbee_cluster_command
data:
  ieee: "00:12:4b:00:12:34:ab:cd"  # Sensor IEEE address
  endpoint_id: 1
  cluster_id: 0x0402  # Temperature Measurement
  cluster_type: "out"
  command: 0  # Bind command
  command_type: "client"
  args:
    - target_ieee: "00:12:4b:00:56:78:ef:gh"  # Smart plug IEEE address
    - target_endpoint: 1
    - cluster_id: 0x0402
```

Repeat for other clusters:
- **Humidity**: `cluster_id: 0x0405`
- **Pressure**: `cluster_id: 0x0403`

### Method 3: Python Script (Advanced)

```python
from zigpy.types import EUI64
from homeassistant.components.zha.core.const import DOMAIN

# Get ZHA gateway
gateway = hass.data[DOMAIN].gateway

# Get sensor device
sensor_ieee = EUI64.convert("00:12:4b:00:12:34:ab:cd")
sensor_device = gateway.get_device(sensor_ieee)
sensor_endpoint = sensor_device.endpoints[1]

# Get target device (smart plug)
target_ieee = EUI64.convert("00:12:4b:00:56:78:ef:gh")
target_device = gateway.get_device(target_ieee)
target_endpoint = target_device.endpoints[1]

# Bind temperature cluster
await sensor_endpoint.temperature.bind(target_endpoint)

# Bind humidity cluster
await sensor_endpoint.relative_humidity.bind(target_endpoint)

# Bind pressure cluster
await sensor_endpoint.pressure.bind(target_endpoint)

print("Bindings created successfully!")
```

## Setup with Zigbee2MQTT

### Using MQTT Command

```bash
# Bind temperature cluster
mosquitto_pub -t 'zigbee2mqtt/bridge/request/device/bind' -m '{
  "from": "BME280_Sensor",
  "to": "Smart_Plug",
  "clusters": ["msTemperatureMeasurement"]
}'

# Bind humidity cluster
mosquitto_pub -t 'zigbee2mqtt/bridge/request/device/bind' -m '{
  "from": "BME280_Sensor",
  "to": "Smart_Plug",
  "clusters": ["msRelativeHumidity"]
}'

# Bind pressure cluster
mosquitto_pub -t 'zigbee2mqtt/bridge/request/device/bind' -m '{
  "from": "BME280_Sensor",
  "to": "Smart_Plug",
  "clusters": ["msPressureMeasurement"]
}'
```

### Using Zigbee2MQTT Web UI

1. **Open Zigbee2MQTT web interface**
2. **Select BME280 sensor** from device list
3. **Go to "Bindings" tab**
4. **Click "Add Binding"**
5. **Select:**
   - Target device: Smart Plug
   - Cluster: msTemperatureMeasurement (or other clusters)
6. **Click "Bind"**

## Verifying Bindings Work

### Check Sensor Logs

Connect via SWO/RTT and look for:
```
Binding created:
  Type: 1
  Local EP: 1
  Remote EP: 1
  Cluster: 0x0402
  Remote EUI64: 00:12:4B:00:56:78:EF:GH
```

### Check Target Device

Monitor the target device (smart plug) logs for incoming attribute reports:
```
Received attribute report from 00:12:4B:00:12:34:AB:CD
  Cluster: 0x0402 (Temperature)
  Attribute: 0x0000 (MeasuredValue)
  Value: 2345 (23.45°C)
```

### Test with Button Press

1. **Short press button** on sensor
2. Sensor takes immediate reading
3. Report should be sent to bound device within 1-2 seconds
4. Check target device receives the report

## Removing Bindings

### ZHA

```yaml
service: zha.issue_zigbee_cluster_command
data:
  ieee: "00:12:4b:00:12:34:ab:cd"
  endpoint_id: 1
  cluster_id: 0x0402
  cluster_type: "out"
  command: 1  # Unbind command
  command_type: "client"
  args:
    - target_ieee: "00:12:4b:00:56:78:ef:gh"
    - target_endpoint: 1
    - cluster_id: 0x0402
```

### Zigbee2MQTT

```bash
mosquitto_pub -t 'zigbee2mqtt/bridge/request/device/unbind' -m '{
  "from": "BME280_Sensor",
  "to": "Smart_Plug",
  "clusters": ["msTemperatureMeasurement"]
}'
```

## Binding Table Capacity

- **Max bindings**: 5 entries (configurable in SLCP)
- **Memory usage**: ~16 bytes per binding = 80 bytes total
- **Recommended usage**: 3 measurement clusters + 2 spare

### Check Available Slots

Bindings are managed by the Zigbee stack. Each binding entry stores:
- Source endpoint (1 byte)
- Cluster ID (2 bytes)
- Destination address type (1 byte)
- Destination EUI64 (8 bytes)
- Destination endpoint (1 byte)

## Example Use Case: Temperature-Controlled Heater

### Scenario
Turn on smart plug (heater) when temperature drops below 18°C.

### Setup

1. **Bind sensor to smart plug** (temperature cluster)
2. **Configure automation on smart plug** (if supported) or **use Home Assistant automation**:

```yaml
automation:
  - alias: "Heater Control via Temperature"
    trigger:
      - platform: state
        entity_id: sensor.tradfri_bme280_temperature
    condition:
      - condition: numeric_state
        entity_id: sensor.tradfri_bme280_temperature
        below: 18
    action:
      - service: switch.turn_on
        target:
          entity_id: switch.smart_plug_heater
```

**Benefit of binding**: Reports arrive faster (direct unicast), lower network load, and works even if coordinator is temporarily unavailable.

## Troubleshooting

### Binding Creation Fails

**Symptoms**: Coordinator returns error when creating binding

**Causes**:
1. **Binding table full** - Max 5 bindings, remove unused ones
2. **Target device doesn't support cluster** - Verify target has matching cluster
3. **Incorrect endpoint** - Most devices use endpoint 1, verify first

**Solution**:
```bash
# Check binding table via ZHA
# Settings → Devices → Sensor → Diagnostics → Bindings
```

### Reports Not Received by Target

**Symptoms**: Binding created but target device doesn't receive reports

**Causes**:
1. **Target device cluster mismatch** - Target must have matching cluster
2. **Reportable change threshold not met** - Change must exceed 0.5°C / 0.5%RH / 0.5kPa
3. **Network issues** - Interference or weak signal

**Solution**:
- Press sensor button (short press) to force immediate report
- Check target device supports receiving cluster reports
- Move devices closer to test

### Binding Deleted Unexpectedly

**Symptoms**: Binding works then stops, logs show "Binding X deleted"

**Causes**:
1. **Coordinator cleared bindings** - Some coordinators auto-manage bindings
2. **Network rejoin** - Bindings may be cleared on network leave/join
3. **Firmware update** - OTA update may reset binding table

**Solution**:
- Recreate bindings after network operations
- Consider persistent binding management in coordinator

### How to Check Binding Table

Currently the binding table can only be checked via coordinator tools:

**ZHA**: Device → Diagnostics → Bindings section
**Zigbee2MQTT**: Device page → Bindings tab
**CLI** (future): `plugin binding-table print`

## Performance Impact

### Memory Usage
- **RAM**: 80 bytes (5 × 16 bytes per binding)
- **Flash**: ~8KB additional code (binding components)
- **Impact**: Minimal, well within TRÅDFRI module capacity

### Power Consumption
- **Idle**: No impact (binding table stored in RAM)
- **Active**: Reports use same power whether bound or not
- **Network**: Actually **reduces** power (fewer hops, direct unicast)

### Network Efficiency
- ✅ **Reduced coordinator load** (direct device-to-device)
- ✅ **Lower latency** (no coordinator relay)
- ✅ **Better reliability** (fewer hops)

## Advanced: Multiple Target Devices

You can bind to multiple devices simultaneously:

```bash
# Bind to smart plug
mosquitto_pub -t 'zigbee2mqtt/bridge/request/device/bind' -m '{
  "from": "BME280_Sensor",
  "to": "Smart_Plug",
  "clusters": ["msTemperatureMeasurement"]
}'

# Also bind to display
mosquitto_pub -t 'zigbee2mqtt/bridge/request/device/bind' -m '{
  "from": "BME280_Sensor",
  "to": "E_Ink_Display",
  "clusters": ["msTemperatureMeasurement"]
}'
```

**Result**: Temperature reports sent to **both** smart plug and display simultaneously.

## Security Considerations

- **Network key required**: Binding uses encrypted Zigbee communication
- **Trust Center approval**: Some networks require TC approval for bindings
- **No authentication bypass**: Binding doesn't grant additional access

## References

- [Zigbee Cluster Library Specification - Binding](https://zigbeealliance.org/wp-content/uploads/2019/11/docs-07-5123-05-zigbee-cluster-library-specification.pdf)
- [Silicon Labs AN1233: Zigbee Binding](https://www.silabs.com/documents/public/application-notes/an1233-zigbee-binding.pdf)
- [ZHA Documentation](https://www.home-assistant.io/integrations/zha/)
- [Zigbee2MQTT Binding Guide](https://www.zigbee2mqtt.io/guide/usage/binding.html)

## Summary

✅ **Device-side binding now supported** in firmware v1.0.1
✅ **Works with ZHA, Zigbee2MQTT, deCONZ**
✅ **Up to 5 simultaneous bindings**
✅ **Direct sensor → device communication**
✅ **Lower latency, reduced network load**

**Next steps:**
1. Update firmware to v1.0.1 or later
2. Create bindings via your coordinator
3. Verify reports are received by target device
4. Test with button press (short press)

For coordinator-only binding (no firmware changes), see [BINDING_GUIDE.md](BINDING_GUIDE.md).
