# ThingsBoard Data Schema - Vario Power Panel

This document defines the ThingsBoard data structure for the ESP32-C6 Vario Power Panel heating system, organized by implementation phases based on current firmware capabilities.

## Device Information

- **Device Type**: Vario Power Panel
- **Hardware**: ESP32-C6-WROOM-1 with MLX90614 IR sensor and LD2410C presence sensor
- **Firmware**: ESP-IDF v5.4.1 based with full BLE command interface

---

## Phase 1: Currently Implemented (Ready for ThingsBoard)

### Device Attributes

_Static properties reported by the device - read-only from ThingsBoard_

#### Device Identity

- `deviceId` - Unique device identifier
- `deviceName` - User-friendly device name
- `deviceModel` - "Vario Power Panel"
- `firmwareVersion` - Current ESP32 firmware version
- `serialNumber` - Manufacturing serial number

#### Hardware Capabilities

- `hardwareCapabilities` - JSON object containing:
  - `elements` - Array of heating element definitions:
    - `elements[0].id` - "heating_low"
    - `elements[0].name` - "Low Power Element"
    - `elements[0].wattage` - Lower power rating in watts
    - `elements[1].id` - "heating_high"
    - `elements[1].name` - "High Power Element"
    - `elements[1].wattage` - Higher power rating in watts
  - `hasPresenceSensor` - true (LD2410C mmWave radar)
  - `hasTemperatureSensor` - true (MLX90614 IR sensor)
  - `hasBluetooth` - true (Full BLE interface)
  - `ratedVoltage` - 240V AC
  - `installationType` - "wall-mount"

### Shared Attributes

_Configuration sent to device from ThingsBoard_

#### Control Settings

- `temperatureSetpoint` - Target temperature set by user (°C)
- `operatingMode` - Operating mode:
  - 0: Disabled (heating off)
  - 1: Manual (direct relay control)
  - 2: Auto (temperature-based control)
  - 3: Bypass (full power, no control logic)
- `heatingEnabled` - Master heating enable/disable
- `statusLedEnabled` - Status LED enable/disable

### Telemetry

_Real-time data sent from device to ThingsBoard_

#### Temperature Measurements

- `temperatureReadings` - JSON object:
  - `ambient` - Room air temperature from MLX90614 (°C)
  - `object` - Surface/radiant temperature from MLX90614 (°C)
  - `operative` - Calculated comfort temperature (°C)
  - `internal` - ESP32 die temperature (°C)

#### Presence Detection

- `presenceDetection` - JSON object:
  - `detected` - Boolean presence status
  - `distances` - JSON object:
    - `moving` - Distance to moving target (cm)
    - `stationary` - Distance to stationary target (cm)
  - `signals` - JSON object:
    - `moving` - Moving target signal strength (0-100)
    - `stationary` - Stationary target signal strength (0-100)

#### Heating Control

- `elementStates` - Array of boolean relay states:
  - `[0]` - Low power element state
  - `[1]` - High power element state
- `powerMetrics` - JSON object:
  - `currentUsage` - Real-time power consumption (watts)

#### Network Status

- `networkStatus` - JSON object:
  - `wifi` - WiFi connection details:
    - `connected` - Connection status
    - `rssi` - Signal strength (dBm)
    - `ssid` - Network name
    - `ipAddress` - Assigned IP address
  - `bluetooth` - BLE status:
    - `enabled` - BLE radio state
    - `advertising` - Advertisement state
    - `connected` - Client connection state

#### System Health

- `systemHealth` - JSON object:
  - `uptime` - Seconds since boot
  - `memory` - Memory usage:
    - `free` - Available RAM (KB)
    - `usage` - RAM utilization percentage
  - `sensors` - Sensor status:
    - `mlx90614Connected` - Temperature sensor status
    - `ld2410Connected` - Presence sensor status

---

## Phase 2: Near-term Extensions

### Enhanced Telemetry

- `powerMetrics.dailyEnergyUsage` - Daily energy consumption (kWh)
- `powerMetrics.lifetimeEnergyUsage` - Total energy consumed (kWh)
- `deviceStatus` - JSON object:
  - `hasError` - Critical error flag
  - `hasWarning` - Warning flag
  - `errorCodes` - Array of error codes
  - `statusLedState` - Current LED color/pattern

### Advanced Configuration

- `sensorConfiguration` - JSON object:
  - `mlx90614Emissivity` - IR sensor calibration (0.05-1.0)
  - `temperatureWindowSize` - Averaging window size
  - `maxDetectionGate` - LD2410C range setting (1-8)
  - `noOneWindow` - Presence timeout (seconds)
- `networkConfiguration` - JSON object:
  - `bleDeviceName` - BLE advertised name
  - `timezone` - Local timezone setting

### Time Synchronization

- `timeSync` - JSON object:
  - `timestamp` - Current device time (ms since epoch)
  - `rtcSyncStatus` - NTP sync status
  - `lastNtpSync` - Last successful sync timestamp

---

## Phase 3: Future Features

### Safety & Compliance

- `safetyConfiguration` - JSON object:
  - `safetyShutoffTemperature` - Maximum surface temperature
  - `openWindowDetectionEnabled` - EcoDesign feature
  - `maxAllowedWattage` - Circuit protection limit

### Advanced Power Management

- `powerMetrics.powerFactor` - Power factor monitoring
- `controlTimestamps` - State change timestamps

### Firmware Management

- `firmwareStatus` - JSON object:
  - `otaStatus` - Update status
  - `pendingVersion` - Available update version
  - `lastAttempt` - Last update attempt

---

## Data Mapping to BLE Commands

### Device Attributes

- Device info: BLE command 0x11
- Hardware capabilities: Static configuration

### Shared Attributes

- Temperature setpoint: BLE commands 0x05/0x06
- Operating mode: BLE commands 0x07/0x08
- LED control: BLE command 0x10

### Telemetry

- Temperature data: BLE streaming + command 0x04
- Presence data: BLE streaming + command 0x0C
- Power/relay states: BLE command 0x09
- Network status: BLE command 0x0D
- System health: BLE command 0x0E
- Energy metrics: BLE command 0x0F

---

## Implementation Notes

### Data Streaming Intervals

**ThingsBoard Telemetry** (same adaptive timing as BLE):

- When presence detected: 10 seconds
- Normal operation: 30 seconds
- When idle (no activity >10 min): Gradually increases up to 10 minutes

**BLE Streaming** (temperature and presence data):

- When presence detected: 10 seconds
- Normal operation: 30 seconds
- When idle (no activity >10 min): Gradually increases up to 10 minutes

**Sensor Tasks** (internal processing):

- MLX90614 temperature readings: 1 second
- LD2410C presence readings: 1 second
- Detailed sensor metrics logging: 60 seconds

### Phase 1 Priority

Focus on core heating functionality with real-time sensor monitoring. This provides complete basic operation with temperature control, presence detection, and system monitoring.

### Integration Strategy

1. Implement Phase 1 telemetry and attributes first
2. Add Phase 2 enhancements for better monitoring
3. Phase 3 features as product requirements evolve

This schema leverages the complete BLE command interface already implemented in the firmware, providing a direct path from BLE functionality to ThingsBoard cloud integration.
