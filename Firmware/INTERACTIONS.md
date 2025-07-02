# Herschel Vario Power Panel - User Interactions Guide

This document describes how users interact with the Herschel Vario Power Panel through physical controls and visual feedback.

## Overview

The Vario Power Panel provides intuitive control through:

- **Single pairing button** with progressive LED feedback
- **Multi-color status LED** showing system state
- **BLE and WiFi provisioning** for network setup

## Button Control System

The pairing button (located on the device) provides comprehensive control through press duration and double-click patterns. The LED provides immediate visual feedback during button presses to show which action will be triggered.

### Button Interaction Patterns

#### Single Press Durations

| Duration          | Action                     | LED Feedback              | Description                                                                                           |
| ----------------- | -------------------------- | ------------------------- | ----------------------------------------------------------------------------------------------------- |
| **0-5 seconds**   | No Action                  | No LED feedback           | Safety window - prevents accidental triggers                                                          |
| **60 seconds**  | BLE Pairing Mode           | Blue breathing            | Primary provisioning method - enter BLE pairing mode for WiFi setup (WiFi disabled for clean pairing) |
| **10-30 seconds** | Factory Reset Confirmation | Purple breathing          | Destructive action requiring confirmation - enter factory reset confirmation mode                     |
| **30+ seconds**   | Bypass Mode (Technician)   | Yellow breathing → steady | Direct heating control mode - technician only, disables automatic control                             |

#### Double-Click Pattern (from BLE Mode)

| Action              | Trigger                        | LED Transition          | Description                                                       |
| ------------------- | ------------------------------ | ----------------------- | ----------------------------------------------------------------- |
| **SoftAP Fallback** | Double-click while in BLE mode | Blue → Dark blue/indigo | Secondary provisioning when BLE fails - creates WiFi access point |

### Progressive LED Feedback

The status LED provides immediate visual feedback during button presses to indicate which action will be triggered:

#### During Button Press (Hold)

1. **0-5 seconds**: No LED feedback - safety window to prevent accidental triggers
2. **5 seconds held**: Blue breathing - BLE pairing ready
3. **10 seconds held**: Purple breathing - Factory reset confirmation ready
4. **30 seconds held**: Yellow breathing - Bypass mode ready (technician only)

#### After Button Release (Action Execution)

- **BLE Pairing**: Blue breathing continues during pairing process
- **SoftAP Mode**: Dark blue/indigo breathing during access point setup
- **Bypass Mode**: Yellow steady LED while bypass mode is active
- **Factory Reset**: Purple breathing while awaiting confirmation

### Special Modes

#### BLE Pairing Mode (5-10 seconds)

**Purpose**: Primary method for WiFi provisioning and device setup

**Activation**: Hold button for 5-10 seconds, release when blue breathing LED appears

**Behavior**:

- WiFi is automatically disabled to prevent interference and ensure optimal BLE performance
- Device enters BLE pairing mode with enhanced advertising
- Blue breathing LED indicates pairing mode is active
- WiFi is re-enabled upon successful credential provisioning and connection

**Setup Process**:

1. Connect to device via BLE (device name: `HERSCHEL_000001`)
2. Send WiFi credentials through BLE provisioning service
3. Device will connect to WiFi and restore normal operation
4. LED will change to green steady when setup is complete

#### SoftAP Fallback Mode (Double-click from BLE)

**Purpose**: Secondary provisioning method when BLE is not available

**Activation**: While in BLE pairing mode, quickly press button twice (within 500ms)

**Behavior**:

- Device creates WiFi access point (`HERSCHEL_000001`)
- Dark blue/indigo breathing LED indicates SoftAP mode
- Connect to the access point and navigate to `http://192.168.4.1/`
- Enter WiFi credentials through web interface

#### Factory Reset Confirmation (10-30 seconds)

**Purpose**: Complete device reset with safety confirmation

**Activation Process**:

1. **Hold button 10-30 seconds**: Purple breathing LED appears
2. **Release button**: Device enters 5-second confirmation window
3. **Press button again within 5 seconds**: Execute factory reset (purple fast blink)
4. **Wait 5 seconds without pressing**: Cancel factory reset, return to normal operation

**What gets reset**:

- WiFi credentials and network settings
- Device configuration and preferences
- All stored data (energy consumption, etc.)
- Device returns to factory default state

#### Bypass Mode (30+ seconds, Technician Only)

**Purpose**: Direct heating control without automatic temperature management

**Activation**: Hold button for 30+ seconds, release when yellow breathing changes to steady

**Behavior**:

- Both heating elements activate immediately at full power
- No automatic temperature control or presence detection
- Yellow steady LED while bypass mode is active
- Intended for technician testing and commissioning only

**Deactivation**: Device restart, BLE command, or switching to different operating mode

## Status LED System

The WS2812 status LED follows a priority system where higher priority states override lower priority ones:

### LED Priority Levels

| Priority Level  | LED Colors      | Description                                   |
| --------------- | --------------- | --------------------------------------------- |
| **Highest**     | Red patterns    | Critical errors override all other states     |
| **High**        | Purple patterns | Factory reset confirmation, firmware updates  |
| **Medium-High** | Blue patterns   | BLE pairing (blue), SoftAP (dark blue/indigo) |
| **Medium**      | Yellow patterns | Bypass mode active, warnings                  |
| **Lowest**      | Green patterns  | Normal operation                              |

### LED States and Meanings

#### Red - Critical Errors (Highest Priority)

| Pattern               | Description            | User Action Required                     |
| --------------------- | ---------------------- | ---------------------------------------- |
| **Fast Blink**        | Critical system error  | Contact technical support                |
| **Slow Double Blink** | Firmware update failed | Retry firmware update or contact support |

#### Purple - System Maintenance (High Priority)

| Pattern          | Description                         | User Action Required                           |
| ---------------- | ----------------------------------- | ---------------------------------------------- |
| **Breathing**    | Factory reset confirmation required | Press button again within 5 seconds to confirm |
| **Fast Blink**   | Factory reset in progress           | Wait for completion (30-60 seconds)            |
| **Medium Pulse** | Firmware update in progress         | Do not power off device                        |
| **Triple Flash** | Firmware update completed           | Device will restart automatically              |

#### Blue - Network Provisioning (Medium-High Priority)

| Pattern        | Description                   | User Action Required                          |
| -------------- | ----------------------------- | --------------------------------------------- |
| **Breathing**  | BLE pairing mode active       | Connect via BLE app and send WiFi credentials |
| **Fast Pulse** | WiFi provisioning in progress | Wait for connection to complete               |
| **Slow Blink** | Ready for WiFi credentials    | Send credentials via BLE or web interface     |

#### Dark Blue/Indigo - SoftAP Mode (Medium-High Priority)

| Pattern       | Description        | User Action Required                                              |
| ------------- | ------------------ | ----------------------------------------------------------------- |
| **Breathing** | SoftAP mode active | Connect to `HERSCHEL_000001` WiFi and visit `http://192.168.4.1/` |

#### Yellow - Warnings & Bypass Mode (Medium Priority)

| Pattern        | Description                     | User Action Required                                             |
| -------------- | ------------------------------- | ---------------------------------------------------------------- |
| **Steady**     | Bypass mode active (technician) | Normal for technician testing - deactivate when testing complete |
| **Slow Blink** | Non-critical warning            | Check WiFi connection, sensor connections                        |

#### Green - Normal Operation (Lowest Priority)

| Pattern          | Description                           | User Action Required                           |
| ---------------- | ------------------------------------- | ---------------------------------------------- |
| **Steady**       | Normal operation, all systems healthy | None - device operating normally               |
| **Single Blink** | Action completed successfully         | None - confirms successful operation           |
| **Triple Flash** | Major operation completed             | None - indicates successful setup/provisioning |

## Troubleshooting

### Common Issues and Solutions

#### LED stays red

- **Cause**: Critical system error
- **Solution**: Power cycle device, contact support if persists

#### LED stays yellow blinking

- **Cause**: WiFi connection lost, sensor disconnected
- **Solution**: Check WiFi network, verify sensor connections

#### BLE pairing not working

- **Cause**: Previous pairing interference
- **Solution**: Try SoftAP mode (double-click from BLE mode)

#### Factory reset not working

- **Cause**: Timing issue with confirmation
- **Solution**: Ensure button press is within 5-second confirmation window

#### Device not responding to button

- **Cause**: Button debounce or hardware issue
- **Solution**: Hold button firmly for full duration, ensure clean press/release

### Device Timing Constants

```c
// Button press durations
BLE_PAIRING_DURATION     = 60 seconds
SOFTAP_DURATION     = 5 minutes
FACTORY_RESET_DURATION   = 10-30 seconds
BYPASS_MODE_DURATION     = 30+ seconds

// Double-click detection
DOUBLE_CLICK_INTERVAL    = 500ms max between clicks
BUTTON_DEBOUNCE         = 50ms

// Factory reset confirmation
CONFIRMATION_TIMEOUT     = 5 seconds
```

## Integration with Mobile Apps

The device is designed to work seamlessly with mobile applications that support:

- **BLE connectivity** for initial setup and control
- **WiFi provisioning** through BLE or SoftAP
- **Real-time monitoring** of temperature and presence data
- **Remote control** of heating modes and schedules

For BLE integration details, see [BLE_INTERFACE.md](BLE_INTERFACE.md).

---

**Note**: This guide covers user-facing interactions. For technical implementation details, development setup, and API documentation, see the main [README.md](../README.md) and [BLE_INTERFACE.md](BLE_INTERFACE.md).
