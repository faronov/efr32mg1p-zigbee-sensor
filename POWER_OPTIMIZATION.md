# Power Optimization Guide

## Current Power Usage Analysis

### High Power Consumption Areas

1. **Sensor readings every 30 seconds** - Too frequent for battery operation
2. **Network joining attempts** - No backoff when network unavailable
3. **Debug UART active** - Serial output consumes power continuously
4. **Sensor reads before network join** - Wasteful when not connected

## Recommended Optimizations

### 1. Disable Automatic Sensor Readings Before Network Join

**Current behavior**: Sensors read every 30s even when not joined
**Optimized**: Only read sensors when network is up

### 2. Increase Sensor Reading Interval

**Current**: 30 seconds
**Recommended for battery**: 5-15 minutes (300000-900000 ms)
**Best**: On-demand only (polling from coordinator)

### 3. Network Join Retry Backoff

**Current**: Continuous retry attempts
**Optimized**: Exponential backoff with max attempts
- Try 1: Immediate
- Try 2: Wait 30 seconds
- Try 3: Wait 60 seconds
- Try 4: Wait 120 seconds
- After 5 tries: Wait 10 minutes between attempts

### 4. Disable Debug Prints for Production

**Current**: UART debug enabled
**Optimized**: Compile-time flag to disable for production builds

### 5. Sleep Configuration

**Zigbee sleepy end device settings**:
- Poll interval: How often to check for messages from parent
- Sleep duration: Maximum time in EM2 sleep between polls

### 6. Event-Driven Sensor Readings

**Optimal approach**:
- Remove periodic timer
- Only read sensor:
  - On button press (manual trigger)
  - When coordinator polls the attribute
  - On bind/reporting configuration

## Implementation Priority

### High Priority (Immediate battery savings)
- ✅ LED turns off after 3s
- ⚠️ Stop sensor reads when network down
- ⚠️ Increase sensor interval to 5 minutes or on-demand
- ⚠️ Add network join backoff

### Medium Priority
- Disable debug prints build flag
- Configure poll interval for sleepy end device
- Optimize I2C clock speed

### Low Priority (Advanced)
- Completely event-driven (no periodic timer)
- Deep sleep between polls
- Power down BME280 between readings

## Estimated Battery Life

**Current configuration** (30s reads, debug on):
- ~2-4 weeks on CR2032

**With high priority optimizations**:
- ~3-6 months on CR2032

**With full optimization (event-driven only)**:
- ~6-12 months on CR2032

## Configuration Changes Needed

1. **SENSOR_UPDATE_INTERVAL_MS**: Change from 30000 to 300000 (5 min)
2. **Add build flag**: `-DPRODUCTION_BUILD` to disable debug
3. **Zigbee poll interval**: Configure to 5-10 seconds for responsive messages
4. **Network steering**: Add max retry count and exponential backoff
