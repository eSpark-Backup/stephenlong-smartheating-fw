# ThingsBoard Integration Requirements

Notes for firmware devs on what we need to integrate with ThingsBoard for the Vario Power Panel. Build this however you think works best - these are just the requirements.

## Overview

We need to connect the Vario Power Panel (ESP32-C6 with MLX90614 and LD2410C sensors) to ThingsBoard for remote monitoring and control. Use the existing sensor data and heating control - just add ThingsBoard as another interface alongside BLE.

**Device Information:**

- **Device Type**: Vario Power Panel
- **Hardware**: ESP32-C6-WROOM-1 with MLX90614 IR sensor and LD2410C presence sensor
- **Firmware**: ESP-IDF v5.4.1 based with full BLE command interface

## API Endpoints to Implement

### Device Provisioning

Use `POST /api/v1/provision` to exchange the device credentials for an access token. Send device name, provision key, and provision secret. Store the returned access token for all other API calls.

### Sending Data

Use `POST /api/v1/{deviceToken}/telemetry` for all sensor readings and system status. Send `POST /api/v1/{deviceToken}/attributes` for device info and capabilities. Update attributes when the device boots or firmware changes.

### Getting Configuration

Poll `GET /api/v1/{deviceToken}/attributes?sharedKeys=...` to get configuration from ThingsBoard. Check for temperature setpoints, operating modes, telemetry intervals, and OTA firmware info. Poll every few minutes, more often when presence is detected.

### Remote Commands

Device control through shared attributes polling. Poll `GET /api/v1/{deviceToken}/attributes?sharedKeys=temperatureSetpoint,operatingMode,heatingEnabled,statusLedEnabled` every 2-5 minutes, more frequently when presence detected.

Optional RPC polling can be added later using `GET /api/v1/{deviceToken}/rpc` with timeout parameter for near-real-time commands. Respond with results using `POST /api/v1/{deviceToken}/rpc/{requestId}`. Support commands like setTemperature, getTemperature, setOperatingMode, getSystemStatus, etc.

### Firmware Downloads

When OTA is triggered, download firmware chunks using `GET /api/v1/{deviceToken}/firmware?title=...&version=...&size=...&chunk=...`. Use 16KB chunks to work with ESP32-C6 memory limits.

---

## Required Features

### Device Attributes (Static Info)

Tell ThingsBoard who this device is and what it can do. Send these once on boot and when things change:

**Device Identity:**

- `deviceId` - Unique device identifier
- `deviceName` - User-friendly device name
- `deviceModel` - "Vario Power Panel"
- `firmwareVersion` - Current ESP32 firmware version
- `serialNumber` - Manufacturing serial number

**Hardware Capabilities:**

- `hardwareCapabilities.elements` - Array of heating element definitions:
  - `elements[0].id` - "heating_low"
  - `elements[0].name` - "Low Power Element"
  - `elements[0].wattage` - Lower power rating in watts
  - `elements[1].id` - "heating_high"
  - `elements[1].name` - "High Power Element"
  - `elements[1].wattage` - Higher power rating in watts
- `hardwareCapabilities.hasPresenceSensor` - true (LD2410C mmWave radar)
- `hardwareCapabilities.hasTemperatureSensor` - true (MLX90614 IR sensor)
- `hardwareCapabilities.hasBluetooth` - true (Full BLE interface)
- `hardwareCapabilities.ratedVoltage` - 240V AC
- `hardwareCapabilities.installationType` - "wall-mount"

### Shared Attributes (Configuration from Cloud)

These are the settings ThingsBoard sends to control the device:

**Control Settings:**

- `temperatureSetpoint` - Target temperature set by user (°C)
- `operatingMode` - Operating mode:
  - 0: Disabled (heating off)
  - 1: Manual (direct relay control)
  - 2: Auto (temperature-based control)
  - 3: Bypass (full power, no control logic)
- `heatingEnabled` - Master heating enable/disable
- `statusLedEnabled` - Status LED enable/disable

### Telemetry (Real-time Data)

Send all the sensor data we're already collecting:

**Temperature Measurements:**

- `temperatureReadings.ambient` - Room air temperature from MLX90614 (°C)
- `temperatureReadings.object` - Surface/radiant temperature from MLX90614 (°C)
- `temperatureReadings.operative` - Calculated comfort temperature (°C)
- `temperatureReadings.internal` - ESP32 die temperature (°C)

**Presence Detection:**

- `presenceDetection.detected` - Boolean presence status
- `presenceDetection.distances.moving` - Distance to moving target (cm)
- `presenceDetection.distances.stationary` - Distance to stationary target (cm)
- `presenceDetection.signals.moving` - Moving target signal strength (0-100)
- `presenceDetection.signals.stationary` - Stationary target signal strength (0-100)

**Heating Control:**

- `elementStates[0]` - Low power element state (boolean)
- `elementStates[1]` - High power element state (boolean)
- `powerMetrics.currentUsage` - Real-time power consumption (watts)

**Network Status:**

- `networkStatus.wifi.connected` - Connection status
- `networkStatus.wifi.rssi` - Signal strength (dBm)
- `networkStatus.wifi.ssid` - Network name
- `networkStatus.wifi.ipAddress` - Assigned IP address
- `networkStatus.bluetooth.enabled` - BLE radio state
- `networkStatus.bluetooth.advertising` - Advertisement state
- `networkStatus.bluetooth.connected` - Client connection state

**System Health:**

- `systemHealth.uptime` - Seconds since boot
- `systemHealth.memory.free` - Available RAM (KB)
- `systemHealth.memory.usage` - RAM utilization percentage
- `systemHealth.sensors.mlx90614Connected` - Temperature sensor status
- `systemHealth.sensors.ld2410Connected` - Presence sensor status

### Enhanced Telemetry

- `powerMetrics.dailyEnergyUsage` - Daily energy consumption (kWh)
- `powerMetrics.lifetimeEnergyUsage` - Total energy consumed (kWh)
- `deviceStatus.hasError` - Critical error flag
- `deviceStatus.hasWarning` - Warning flag
- `deviceStatus.errorCodes` - Array of error codes
- `deviceStatus.statusLedState` - Current LED color/pattern

### Advanced Configuration

- `sensorConfiguration.mlx90614Emissivity` - IR sensor calibration (0.05-1.0)
- `sensorConfiguration.temperatureWindowSize` - Averaging window size
- `sensorConfiguration.maxDetectionGate` - LD2410C range setting (1-8)
- `sensorConfiguration.noOneWindow` - Presence timeout (seconds)
- `networkConfiguration.bleDeviceName` - BLE advertised name
- `networkConfiguration.timezone` - Local timezone setting

### Time Synchronization

- `timeSync.timestamp` - Current device time (ms since epoch)
- `timeSync.rtcSyncStatus` - NTP sync status
- `timeSync.lastNtpSync` - Last successful sync timestamp

### Safety & Compliance

- `safetyConfiguration.safetyShutoffTemperature` - Maximum surface temperature
- `safetyConfiguration.openWindowDetectionEnabled` - EcoDesign feature
- `safetyConfiguration.maxAllowedWattage` - Circuit protection limit

### Advanced Power Management

- `powerMetrics.powerFactor` - Power factor monitoring
- `controlTimestamps` - State change timestamps

### Firmware Management

- `firmwareStatus.otaStatus` - Update status
- `firmwareStatus.pendingVersion` - Available update version
- `firmwareStatus.lastAttempt` - Last update attempt

---

## Data Mapping to BLE Interface

ThingsBoard data structures map directly to the existing BLE interface. See [BLE_INTERFACE.md](BLE_INTERFACE.md) for complete command reference and data formats.

---

## Implementation Guidelines

### Data Timing Strategy

Keep the current adaptive timing - faster updates when someone's around (10s), normal operation (30s), slower when idle (up to 10min). But make these intervals configurable via ThingsBoard shared attributes.

**Note**: Presence detection currently affects only telemetry intervals, not heating control behavior.

**ThingsBoard Telemetry** (same adaptive timing as BLE):

- When presence detected: 10 seconds
- Normal operation: 30 seconds
- When idle (no activity >10 min): Gradually increases up to 10 minutes

**BLE Streaming** (temperature and presence data):

- When presence detected: 5 seconds
- Normal operation: 30 seconds
- When idle (no activity >10 min): 5 minutes

**Sensor Tasks (internal processing):**

- MLX90614 temperature readings: 1 second
- LD2410C presence readings: 1 second
- Detailed sensor metrics logging: 60 seconds

### Reuse Existing Code

The existing BLE commands and sensor interfaces should map pretty directly to ThingsBoard. Temperature readings from the MLX sensor, presence data from the LD2410, heating control from the relay interface - just package it up for ThingsBoard instead of (or in addition to) BLE.

### Error Handling

If ThingsBoard is unreachable, buffer data locally and send when connection is restored. If commands fail, report the error back. If sensors die, keep reporting what we can and flag the problem.

### Security

Use HTTPS for everything, validate firmware checksums, store the device token securely in NVS. Same security approach as the existing implementation.

---

## Quick Reference

### Key Shared Attribute Names

- `temperatureSetpoint` (float) - target temperature
- `operatingMode` (0-3) - heating mode
- `heatingEnabled` (boolean) - master enable/disable
- `fw_title`, `fw_version`, `fw_checksum`, `fw_checksum_algorithm` - OTA triggers

### Control Methods

**Shared Attributes (Primary):**

- `temperatureSetpoint` (float) - target temperature setting
- `operatingMode` (0-3) - heating mode control
- `heatingEnabled` (boolean) - master enable/disable
- `statusLedEnabled` (boolean) - LED control

**RPC Commands (Optional):**

- `setTemperature`, `getTemperature`
- `setOperatingMode`, `getOperatingMode`
- `getSystemStatus`

### Data Timing

- Presence detected: 10 seconds
- Normal: 30 seconds
- Idle: up to 10 minutes
- Events: immediate

### Implementation Notes

**Core Requirements:**

- Shared attributes polling for device control
- Core telemetry and device provisioning (already working)
- Basic configuration management via attributes

**Optional Enhancements:**

- RPC polling for real-time commands
- Enhanced telemetry and advanced configuration
- OTA firmware management
- Advanced features requiring persistent connections

### Integration Strategy

Focus on core heating functionality with real-time sensor monitoring first, leveraging the existing BLE interface for data collection and device control.
