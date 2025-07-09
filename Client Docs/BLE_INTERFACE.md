# Herschel Infrared BLE Interface

> **ESP32-C6 Firmware Implementation**  
> This firmware provides **automatic sensor data streaming**. Simply connect to start receiving temperature, presence, and WiFi status notifications immediately!

## Overview

This document defines the Bluetooth® Low Energy GATT services for Herschel Infrared heater control and monitoring. The services provide Wi-Fi provisioning, device control, and sensor data streaming functionality specifically designed for infrared heating applications.

**For complete heater operational behavior, control logic, and mobile app integration guidelines, see [modes.md](modes.md).**

The implementation follows Nordic Semiconductor patterns and is compatible with standard BLE development frameworks.

**Firmware Features:**

- **Automatic Streaming**: Sensor data streams immediately after connection
- **Multi-Client Support**: Up to 2 concurrent connections
- **Real-time Data**: 5-second intervals for temperature and presence
- **Binary Commands**: 17 commands (0x01-0x11) for complete device control
- **WiFi Provisioning**: JSON-based network configuration service
- **Security**: "Just Works" encryption for all connections

### BLE Interface Structure

The BLE interface consists of two primary services:

- **Wi-Fi Provisioning Service**: Handles initial device setup and Wi-Fi credential configuration
- **Herschel Control Service**: Provides device control commands and sensor data streaming

### Company Identifier

The Herschel Infrared company identifier is registered with the Bluetooth SIG:

| Company Name           | Identifier |
| ---------------------- | ---------- |
| Herschel Infrared Ltd. | 0x0EC3     |

---

## Wi-Fi Provisioning Service

This service implements Wi-Fi credential provisioning using the Nordic UART Service (NUS) pattern. It enables initial device setup and Wi-Fi network configuration.

### Wi-Fi Service Declaration

| Service Name               | UUID                                   |
| -------------------------- | -------------------------------------- |
| Wi-Fi Provisioning Service | `6E400001-B5A3-F393-E0A9-E50E24DCCA9E` |

### Wi-Fi Service Characteristics

| Characteristic Name   | UUID                                   |
| --------------------- | -------------------------------------- |
| RX (Credential Input) | `6E400002-B5A3-F393-E0A9-E50E24DCCA9E` |
| TX (Status Output)    | `6E400003-B5A3-F393-E0A9-E50E24DCCA9E` |

### Wi-Fi Characteristic Properties

| Characteristic        | Properties                    | Security              |
| --------------------- | ----------------------------- | --------------------- |
| RX (Credential Input) | Write, Write Without Response | Just Works encryption |
| TX (Status Output)    | Notify, Read                  | Just Works encryption |

### Provisioning Protocol

The provisioning protocol uses JSON message format for human-readable credential exchange.

#### Wi-Fi Credential Input (RX Characteristic)

**Message Format**: JSON string

**Example**: `{"ssid":"MyNetwork","pass":"MyPassword"}`

**Fields**:

- `ssid`: Wi-Fi network name (string, max 32 characters)
- `pass`: Wi-Fi password (string, max 63 characters)

#### Status Output (TX Characteristic)

**Message Format**: JSON string

**Examples**:

- Connection success: `{"status":"connected","ip":"192.168.1.100"}`
- Connection failure: `{"status":"failed","error":"Invalid credentials"}`
- Connection in progress: `{"status":"connecting","ssid":"MyNetwork"}`

**Fields**:

- `status`: Connection state (`connecting`, `connected`, `failed`)
- `ip`: Assigned IP address (present when connected)
- `ssid`: Network name (present during connection attempts)
- `error`: Error description (present when failed)

---

## Herschel Control Service

This service provides device control commands and sensor data streaming for infrared heater management.

### Herschel Service Declaration

| Service Name             | UUID                                   |
| ------------------------ | -------------------------------------- |
| Herschel Control Service | `7E400001-B5A3-F393-0EC3-E50E24DCCA9E` |

### Herschel Service Characteristics

| Characteristic Name | UUID                                   |
| ------------------- | -------------------------------------- |
| Control Point       | `7E400002-B5A3-F393-0EC3-E50E24DCCA9E` |
| Response            | `7E400003-B5A3-F393-0EC3-E50E24DCCA9E` |
| Temperature Data    | `7E400004-B5A3-F393-0EC3-E50E24DCCA9E` |
| Presence Data       | `7E400005-B5A3-F393-0EC3-E50E24DCCA9E` |

### Herschel Characteristic Properties

| Characteristic   | Properties      | Security              |
| ---------------- | --------------- | --------------------- |
| Control Point    | Write, Indicate | Just Works encryption |
| Response         | Notify, Read    | Just Works encryption |
| Temperature Data | Notify, Read    | Just Works encryption |
| Presence Data    | Notify, Read    | Just Works encryption |

### Control Protocol

The control protocol uses binary command-response messaging for efficient device control.

#### Command Structure

All commands follow the format: `[COMMAND_CODE] [PARAMETERS...]`

#### Response Structure

All responses follow the format: `[COMMAND_CODE] [STATUS] [DATA...]`

**Status Codes**:

- `0xA1`: Success
- `0xA2`: Failed

**Data Format**:

- `FLOAT*`: IEEE 754 single-precision float, 4 bytes, little-endian
- `[LENGTH]`: Single byte length indicator for variable-length data
- Multi-byte integers are little-endian

### Data Streaming

All notification characteristics automatically stream data to connected clients immediately after connection.

#### Stream Behavior

- **Temperature Notifications**: Automatic streaming to all connected clients
- **Presence Notifications**: Automatic streaming to all connected clients
- **WiFi Status Notifications**: Automatic streaming to all connected clients
- **Control Point Indications**: Available via CCCD (for command responses)
- **Response Notifications**: Available via CCCD (for command responses)
- **Connection Required**: Simply connect to start receiving data immediately

#### nRF Connect Compatibility

The firmware provides Client Characteristic Configuration Descriptors (CCCDs) for:

- **WiFi TX Characteristic**: Subscribe to WiFi status notifications
- **Control Point**: Subscribe to command response indications
- **Response Characteristic**: Subscribe to command response notifications

Temperature and Presence data streams automatically without requiring CCCD subscription.

#### Temperature Data Stream

**Data Format**: `[TIMESTAMP] [AMBIENT_TEMP] [OBJECT_TEMP]`

**Fields**:

- `TIMESTAMP`: Unix timestamp (uint32, 4 bytes)
- `AMBIENT_TEMP`: MLX90614 ambient temperature (float32, 4 bytes)
- `OBJECT_TEMP`: MLX90614 object temperature (float32, 4 bytes)

**Total Size**: 12 bytes per notification

**Streaming Intervals**:

- **BLE Data Streaming**: Adaptive intervals (5s when presence detected, 30s normal, 5min idle)
- **Other Telemetry**: Smart intervals (10s presence, 30s normal, up to 10min idle)

#### Presence Data Stream

**Data Format**: `[TIMESTAMP] [HAS_TARGET] [MOVE_DIST] [STILL_DIST] [MOVE_ENERGY] [STILL_ENERGY]`

**Fields**:

- `TIMESTAMP`: Unix timestamp (uint32, 4 bytes)
- `HAS_TARGET`: Presence detected (uint8, 1 byte: 0=no, 1=yes)
- `MOVE_DIST`: Moving target distance in cm (float32, 4 bytes)
- `STILL_DIST`: Still target distance in cm (float32, 4 bytes)
- `MOVE_ENERGY`: Moving signal energy 0-100 (uint8, 1 byte)
- `STILL_ENERGY`: Still signal energy 0-100 (uint8, 1 byte)

**Total Size**: 18 bytes per notification

**Streaming Intervals**:

- **Current Implementation**: Adaptive timing - 5s when presence detected, 30s normal, 5min idle
- **Automatic**: No configuration required, streams immediately on connection
- **Multi-Client**: All connected clients receive identical data streams

#### Implementation Details

**Key Benefits**:

- **Immediate Data**: Start receiving notifications after connection
- **Multi-Client Support**: Up to 2 concurrent clients supported
- **Reliable Delivery**: Uses ESP32 BLE stack notification system
- **Real-time Updates**: 5-second intervals for sensor data

**Technical Implementation**:

- Notifications sent to all active client connections automatically
- Simplified client state management
- Compatible with standard BLE notification patterns

**Error Handling**:

- Failed notifications are logged but don't stop streaming
- Client disconnections automatically cleanup streaming state
- Robust multi-client connection management

---

## Command Reference

### Power and System Control

#### Power Control (0x01)

- **Command**: `0x01`
- **Response**: `0x01 0xA1` (success) or `0x01 0xA2` (failed)
- **Description**: Reboots the device (power off not supported)

#### Factory Reset (0x02)

- **Command**: `0x02`
- **Response**: `0x02 0xA1` (success) or `0x02 0xA2` (failed)
- **Description**: Clears Wi-Fi credentials and resets device to factory settings

### Temperature Monitoring

#### Read Internal Temperature (0x03)

- **Command**: `0x03`
- **Response**: `0x03 [TEMP_FLOAT]`
- **Description**: Reads ESP32 internal die temperature for thermal management

#### Read IR Sensor Temperatures (0x04)

- **Command**: `0x04`
- **Response**: `0x04 [AMBIENT_FLOAT] [OBJECT_FLOAT]`
- **Description**: Reads MLX90614 ambient and object temperatures

### Heating Control

#### Set Target Temperature (0x05)

- **Command**: `0x05 [TEMP_FLOAT]`
- **Response**: `0x05 0xA1` (success) or `0x05 0xA2` (failed)
- **Description**: Sets target temperature for controller with special UI trigger values

**Temperature Range**: UI_MIN to UI_MAX for normal operation (currently 0°C to 35°C)

**Special Trigger Values** (relative to UI temperature range):

- **Restore Trigger**: `UI_MIN - 2.0` - Switches to auto mode
- **Off Trigger**: `UI_MIN - 1.0` - Sets manual mode with heater off
- **Boost Trigger**: `UI_MAX + 1.0` - Sets manual mode with heater high

**Response Behavior**:

- **All operations**: Returns only status (`0xA1` success or `0xA2` failed)
- **Normal temperatures (UI_MIN to UI_MAX)**: Stores temperature in NVS and returns success
- **Restore trigger (UI_MIN - 2.0)**: Sets device to auto mode and returns success
- **Off/Boost triggers (UI_MIN - 1.0, UI_MAX + 1.0)**: Sets manual mode with specified heater state and returns success
- **Invalid range**: Returns `0xA2` (failed) status

**Usage Examples** (with UI_MIN=0°C, UI_MAX=35°C):

- Set 22°C: `0x05 [22.0f]` → Response: `0x05 0xA1` (temperature stored)
- Off trigger: `0x05 [-1.0f]` → Response: `0x05 0xA1` (heater turns off, stored temp unchanged)
- Boost trigger: `0x05 [36.0f]` → Response: `0x05 0xA1` (heater on high, stored temp unchanged)
- Restore trigger: `0x05 [-2.0f]` → Response: `0x05 0xA1` (switches to auto mode)

**Implementation Notes**:

- SET_TARGET_TEMP only returns status - use GET_TARGET_TEMP to read current values
- Trigger values are calculated relative to UI_MIN_TEMP_CELSIUS and UI_MAX_TEMP_CELSIUS constants
- Off and Boost triggers preserve the previously stored target temperature
- Restore trigger enables automatic temperature control using the stored target temperature
- All trigger operations immediately affect device mode and heater state

#### Get Target Temperature (0x06)

- **Command**: `0x06`
- **Response**: `0x06 [TEMP_FLOAT]`
- **Description**: Reads current target temperature or trigger value based on device mode and state

**Response Behavior**:

- **Auto Mode**: Returns the actual stored target temperature from NVS
- **Manual Mode - Heater OFF**: Returns off trigger value (`UI_MIN - 1.0`)
- **Manual Mode - Heater HIGH**: Returns boost trigger value (`UI_MAX + 1.0`)
- **Manual Mode - Other States**: Returns the actual stored target temperature

**Usage Examples** (with UI_MIN=0°C, UI_MAX=35°C):

- Auto mode with 22°C stored: `0x06` → Response: `0x06 [22.0f]`
- Manual mode, heater off: `0x06` → Response: `0x06 [-1.0f]` (off trigger)
- Manual mode, heater high: `0x06` → Response: `0x06 [36.0f]` (boost trigger)
- Manual mode, heater mid/low: `0x06` → Response: `0x06 [22.0f]` (stored temperature)

**Implementation Notes**:

- This command reflects the current UI state - what value the UI should display
- In manual off/boost modes, returns the trigger values to maintain UI consistency
- Use this command after SET_TARGET_TEMP operations to confirm current display value
- Trigger values are calculated relative to UI_MIN_TEMP_CELSIUS and UI_MAX_TEMP_CELSIUS constants

#### Set Operating Mode (0x07)

- **Command**: `0x07 [MODE]`
- **Response**: `0x07 0xA1` (success) or `0x07 0xA2` (failed)
- **Description**: Controls heater operating mode and master enable state

**Operating Modes**:

- `0x00`: Disabled (master heating off)
- `0x01`: Manual (direct relay control via commands)
- `0x02`: Auto (internal logic with target temp + presence)
- `0x03`: Bypass (full power on boot, no control logic)

**For detailed operating mode behavior, control logic, and mobile app integration, see [modes.md](modes.md).**

#### Get Operating Mode (0x08)

- **Command**: `0x08`
- **Response**: `0x08 [MODE]`
- **Description**: Reads current operating mode

#### Read Relay States and Power (0x09)

- **Command**: `0x09`
- **Response**: `0x09 [LOW_STATE] [HIGH_STATE] [CURRENT_POWER]`
- **Description**: Gets heating relay states and calculated power usage

**Fields**:

- `LOW_STATE`: Low power relay state (0x00=off, 0x01=on)
- `HIGH_STATE`: High power relay state (0x00=off, 0x01=on)
- `CURRENT_POWER`: Real-time power consumption (float32)

#### Set Relay States (0x0A)

- **Command**: `0x0A [LOW_STATE] [HIGH_STATE]`
- **Response**: `0x0A 0xA1` (success) or `0x0A 0xA2` (failed)
- **Description**: Manually controls heating relay states

### System Diagnostics

#### Control Sensor Board (0x0B)

- **Command**: `0x0B [STATE]`
- **Response**: `0x0B 0xA1` (success) or `0x0B 0xA2` (failed)
- **Description**: Controls sensor board MOSFET for MLX90614 power cycling
- **Purpose**: Critical for sensor recovery when MLX90614 becomes unresponsive

#### Read Presence Data (0x0C)

- **Command**: `0x0C`
- **Response**: `0x0C [HAS_TARGET] [MOVE_DIST] [STILL_DIST] [MOVE_ENERGY] [STILL_ENERGY]`
- **Description**: Complete presence detection data including state and LD2410 sensor measurements

**Fields**:

- `HAS_TARGET`: Presence detected (uint8, 1 byte: 0=no, 1=yes)
- `MOVE_DIST`: Moving target distance in cm (float32, 4 bytes)
- `STILL_DIST`: Still target distance in cm (float32, 4 bytes)
- `MOVE_ENERGY`: Moving signal energy 0-100 (uint8, 1 byte)
- `STILL_ENERGY`: Still signal energy 0-100 (uint8, 1 byte)

#### Read Wi-Fi Status (0x0D)

- **Command**: `0x0D`
- **Response**: `0x0D [WIFI_CONNECTED] [INTERNET_AVAILABLE] [RSSI] [SSID_LEN] [SSID] [IP_ADDR]`
- **Description**: Wi-Fi and internet connectivity status

**Fields**:

- `WIFI_CONNECTED`: Wi-Fi connection state (uint8, 1 byte: 0=disconnected, 1=connected)
- `INTERNET_AVAILABLE`: Internet connectivity test result (uint8, 1 byte: 0=no, 1=yes)
- `RSSI`: Wi-Fi signal strength in dBm (int8, 1 byte)
- `SSID_LEN`: Length of SSID string (uint8, 1 byte)
- `SSID`: Connected network name (string, variable length)
- `IP_ADDR`: Assigned IP address (uint32, 4 bytes, network byte order)

#### Read System Diagnostics (0x0E)

- **Command**: `0x0E`
- **Response**: `0x0E [FREE_MEM] [UPTIME] [RESET_REASON]`
- **Description**: System health and diagnostic information

#### Read Energy Metrics (0x0F)

- **Command**: `0x0F`
- **Response**: `0x0F [CURRENT_POWER] [DAILY_ENERGY] [LIFETIME_ENERGY]`
- **Description**: Power consumption and energy usage data

### User Interface

#### Control Front LED (0x10)

- **Command**: `0x10 [ENABLED]`
- **Response**: `0x10 0xA1` (success) or `0x10 0xA2` (failed)
- **Description**: Controls whether the front status LED is enabled or disabled

**Fields**:

- `ENABLED`: LED state (uint8, 1 byte: 0=disabled, 1=enabled)

### Device Information

#### Read Device Information (0x11)

- **Command**: `0x11`
- **Response**: `0x11 [NAME_LEN] [NAME] [MODEL_LEN] [MODEL] [FW_VERSION] [MAC_ADDR]`
- **Description**: Device identification and version information

---

## Usage Workflow

### Initial Setup

1. Scan for device `HERSCHEL_000001`
2. Connect and discover services
3. WiFi status notifications begin automatically
4. Send Wi-Fi credentials via RX characteristic
5. Monitor connection status via TX notifications

### Device Control

1. Send commands via Control Point characteristic
2. Receive responses via Response notifications
3. Use command-response pattern for device control
4. All response notifications are automatic

### Continuous Monitoring

1. Temperature data streams automatically after connection
2. Presence data streams automatically after connection
3. Receive continuous sensor data at 5-second intervals
4. Use diagnostic commands (0x0D, 0x0E, 0x0F) for additional system information

---

## Error Handling

**BLE Protocol Error Handling**:

- Failed notifications are logged but don't stop streaming
- Client disconnections automatically cleanup streaming state
- Robust multi-client connection management
- Use diagnostic commands (0x0B, 0x0D, 0x0E) for system monitoring

**For complete safety systems, sensor recovery, and operational contexts, see [modes.md](modes.md).**

---

## Security Model

### Simplified Security

The device uses "Just Works" encryption for all BLE connections, providing a balance between security and ease of use.

**All services accessible after connection:**

- **Wi-Fi Provisioning Service** - All characteristics for network setup
- **Herschel Control Service** - Device control, monitoring, diagnostics
- **All command-response functionality** - Temperature control, relay control, etc.
- **All streaming data** - Temperature and presence notifications
- **Device information** - Model, firmware version, etc.

### Security Requirements

- **All Connections**: "Just Works" encryption automatically applied
- **No Bonding Required**: Immediate access to all functionality
- **Encrypted Communication**: All data transmitted over encrypted BLE channel

This simplified security model provides encrypted communication while allowing immediate access to all device functionality.

---

## Connection Behavior

- **Device Role**: Peripheral (advertises services)
- **Advertisement Name**: `HERSCHEL_000001`
- **MTU Size**: 517 bytes (negotiable)
- **Connection Interval**: 7.5-20 ms (preferred)
- **Security**: "Just Works" encryption for all connections
- **Automatic Notifications**: All sensor data streams immediately after connection
- **Multi-Client**: Supports up to 2 concurrent connections with identical data streams

---

## Implementation Notes

This specification is designed for implementation with:

- **ESP32/ESPHome**: Firmware implementation
- **Mobile platforms**: iOS and Android applications
- **Web Bluetooth**: Browser-based applications
- **Standard BLE frameworks**: Compatible with common BLE development tools

The protocol follows established Nordic patterns for maximum compatibility and developer familiarity.

### Client Implementation Notes

**Connection Process:**

1. **Connect** to the device
2. **Pair** with the device ("Just Works" encryption automatically applied)
3. **Listen** for notifications on desired characteristics
4. **Send commands** via Control Point as needed

**Data Reception:**

- All sensor data begins streaming automatically after connection
- No additional setup or configuration required
- Multiple clients can receive identical data streams
- Standard BLE notification handling applies

---

## Local Scheduling Implementation Notes

Local scheduling functionality planned for autonomous operation when cloud connectivity is unavailable.

### ESP32-C6 Resources

- HP SRAM: 512KB (~400KB available after system overhead)
- No PSRAM support
- Flash: Available for schedule storage
- **Conclusion**: Comprehensive local scheduling feasible

### Implementation Plan

**Core Features:**

- Time-based scheduling with presence integration
- Local web interface for schedule management
- Multi-zone coordination via HTTP API
- Schedule storage in flash with RAM caching

**Implementation Phases:**

1. Basic scheduling engine and web interface
2. Zone coordination and multi-device features
3. Advanced conditional logic and optimization

**Future BLE Commands:**

- Schedule status control
- Time synchronization
- Schedule execution override
- Schedule upload (chunked transfer)
