# Herschel Infrared Heater - Operational Modes and Control

This document provides comprehensive coverage of heater operational modes, control behaviors, and system contexts for mobile app developers and system integrators. It explains how the heater actually operates, not just the BLE protocol commands.

## Overview

The Herschel infrared heater system operates in multiple contexts that affect heating behavior, control availability, and system responses. Understanding these operational modes is essential for proper integration and user interface design.

**Key Control Hierarchies:**

- **Physical Controls**: Button patterns override all other control methods
- **Network Connectivity**: WiFi/Cloud takes precedence over BLE when available
- **Safety Systems**: Automatic shutdowns override all manual controls
- **Operating Modes**: Determine which control methods are active

---

## Physical Control Modes

Physical button patterns trigger specific operational states that affect heater behavior and control availability.

### Button-Triggered Operational States

#### BLE Pairing Mode (5-10 second hold)

**Activation**: Hold pairing button for 5-10 seconds, release when blue breathing LED appears

**Heater Behavior:**

- **Heating Status**: Continues current heating state during pairing
- **Control Availability**: BLE commands available, WiFi disabled temporarily
- **Safety**: All safety systems remain active
- **Duration**: Active until WiFi credentials provided or 1-minute timeout (BLE_PAIRING_TIMEOUT - prevents indefinite pairing mode)

**System Changes:**

- WiFi automatically disabled to prevent BLE interference
- Enhanced BLE advertising for reliable connection
- Heating control remains available via BLE commands
- WiFi re-enabled after successful credential provisioning

**Mobile App Considerations:**

- Primary provisioning method - most reliable for initial setup
- All BLE commands remain functional during pairing
- Heating control transferred to BLE during this mode

#### SoftAP Mode (Double-click from BLE mode)

**Activation**: While in BLE pairing mode, double-click button (within 500ms)

**Heater Behavior:**

- **Heating Status**: Continues current heating state
- **Control Availability**: Web interface only (192.168.4.1)
- **Safety**: All safety systems remain active
- **Network**: Creates WiFi access point `HERSCHEL_000001`

**System Changes:**

- BLE becomes unavailable during SoftAP operation
- Heating control only via web interface or pre-set schedule
- No real-time sensor data streaming during setup
- Returns to normal operation after WiFi configuration

#### Factory Reset (10-30 second hold + confirmation)

**Activation Process:**

1. Hold button 10-30 seconds (purple breathing LED)
2. Release button → 5-second confirmation window
3. Press button again within 5 seconds to confirm (FACTORY_RESET_CONFIRMATION_TIMEOUT - safety window)

**Heater Behavior:**

- **Heating Status**: All heating disabled immediately upon confirmation
- **Control Availability**: No external control during reset
- **Duration**: 30-60 seconds for complete reset
- **Recovery**: Device restarts with factory defaults

**What Gets Reset:**

- WiFi credentials and network settings
- Target temperature and operating mode (returns to Manual mode)
- Energy consumption history and device preferences
- BLE pairing bonds and device configuration

#### Bypass Mode (30+ second hold, Technician Only)

**Activation**: Hold button for 30+ seconds, release when yellow breathing changes to steady

**Heater Behavior:**

- **Heating Status**: Both elements activate immediately at full power
- **Control Logic**: All automatic control disabled
- **Safety Overrides**: Temperature and presence detection bypassed
- **Power Output**: Maximum power regardless of target temperature

**System Changes:**

- No automatic temperature control or presence detection
- BLE/WiFi commands for heating control are ignored
- Only safety shutdowns (over-temperature) remain active
- Yellow steady LED indicates bypass mode is active

**Deactivation Methods:**

- Device restart/power cycle
- BLE command 0x07 (Set Operating Mode)
- 30-minute automatic timeout for safety (prevents indefinite bypass mode)

---

## Heater Operating Modes

Operating modes determine how the heater responds to commands and environmental conditions.

### Manual Mode (0x00)

**Control Method**: Direct relay control via BLE/WiFi commands

**Heating Behavior:**

- **Temperature Control**: None - relies on manual relay commands
- **Presence Integration**: No automatic presence-based control
- **User Control**: Full manual control of both heating elements
- **Safety**: Over-temperature protection only

**Command Availability:**

- Set Relay States (0x0A): ✅ Full control
- Set Target Temperature (0x05): ⚠️ Accepted but not used
- Automatic heating tasks: ❌ Disabled

**Mobile App Integration:**

- Direct relay control interface required
- Manual temperature monitoring and control
- No automatic heating scheduling
- User responsible for all heating decisions

### Auto Mode (0x01)

**Control Method**: Internal logic using target temperature only (no presence integration)

**Heating Behavior:**

- **Temperature Control**: Automatic based on target temperature differential
- **Presence Integration**: None - heating operates independently of presence status
- **Safety**: Temperature-based safety only (no connectivity/presence timeouts implemented)
- **Response Time**: Updates every 1 second (operate_heater() task)

**Temperature Control Algorithm (from Heater_interface.h/c):**

| Temperature Delta (°C)                            | Low Relay (GPIO18) | High Relay (GPIO19) | Heat State  | Actual Power (HC-1200-VP) |
| ------------------------------------------------- | ------------------ | ------------------- | ----------- | ------------------------- |
| diff > THRESHOLD_HIGH (1.5°C)                     | ON                 | ON                  | HEATER_HIGH | 1200W (500W + 700W)       |
| THRESHOLD_MID < diff ≤ THRESHOLD_HIGH (0.5-1.5°C) | OFF                | ON                  | HEATER_MID  | 700W (HEATING_HIGH_WATTS) |
| THRESHOLD_LOW < diff ≤ THRESHOLD_MID (-0.5-0.5°C) | ON                 | OFF                 | HEATER_LOW  | 500W (HEATING_LOW_WATTS)  |
| diff ≤ THRESHOLD_LOW (-0.5°C)                     | OFF                | OFF                 | HEATER_OFF  | 0W                        |

_Where diff = target_temperature - current_operative_temperature_
_Constants defined in Heater_interface.h and defines.h_

**Panel Configurations:**

- **VarioPower 600W (HC-600-VP)**: 300W edge + 300W center (600W total)
- **VarioPower 850W (HC-850-VP)**: 350W edge + 500W center (850W total)
- **VarioPower 1050W (HC-1050-VP)**: 450W edge + 600W center (1050W total)
- **VarioPower 1200W (HC-1200-VP)**: 500W edge + 700W center (1200W total) ← **Standard Reference**
- **VarioPower 1250W (HC-1250-VP)**: 500W edge + 750W center (1250W total)

**HC-1200-VP Firmware Configuration (from defines.h):**

- **Low Element (GPIO18)**: 500W (HEATING_LOW_WATTS) - Edge/lower power element
- **High Element (GPIO19)**: 700W (HEATING_HIGH_WATTS) - Center/higher power element
- **Total Power**: 1200W when both elements active

_Note: Firmware currently configured for HC-1200-VP. Other panel configurations require different HEATING_LOW_WATTS and HEATING_HIGH_WATTS values._

**Element Configuration (Actual Implementation):**

- **Low Relay (GPIO18)**: 500W element (HEATING_LOW_WATTS)
- **High Relay (GPIO19)**: 700W element (HEATING_HIGH_WATTS)
- **Heating Control**: Low relay for HEATER_LOW, high relay for HEATER_MID, both for HEATER_HIGH

**Presence Integration (Current Implementation):**

- **No Automatic Presence Control**: Heating operates purely on temperature differential
- **Presence Detection**: Used only for telemetry interval adjustment (PRESENCE_TELEMETRY_TIME vs NORMAL_TELEMETRY_TIME)
- **Safety Note**: System will heat indefinitely if target temperature is set, regardless of presence status

**Command Availability:**

- Set Target Temperature (0x05): ✅ Controls automatic heating
- Set Relay States (0x0A): ⚠️ Temporary override only
- Automatic heating tasks: ✅ Fully active

**Mobile App Integration:**

- Target temperature control interface
- Real-time temperature monitoring
- Presence-based heating optimization
- Schedule and automation support

### Bypass Mode (0x02, Technician Only)

**Control Method**: Full power heating with all safety overrides

**Heating Behavior:**

- **Power Output**: Both elements at full power immediately
- **Temperature Control**: No automatic control
- **Presence Integration**: Presence detection ignored
- **Safety**: Only critical over-temperature protection (>90°C)

**System Override:**

- All automatic heating logic disabled
- BLE/WiFi heating commands ignored
- No energy optimization or presence detection
- Maximum power output for system testing

**Command Availability:**

- Set Target Temperature (0x05): ❌ Ignored
- Set Relay States (0x0A): ❌ Ignored
- All heating control: ❌ Overridden

**Mobile App Integration:**

- Read-only mode for heating controls
- Monitoring and diagnostics only
- Clear indication that heating is in technician mode
- No user heating control available

---

## Network Connectivity Context

Network connectivity directly affects heater control hierarchy and behavior.

### WiFi Connected State

**Control Hierarchy:**

1. **ThingsBoard/Cloud**: Primary control method
2. **BLE**: Backup control available
3. **Physical Button**: Override capability
4. **Local Schedule**: Autonomous operation

**Heater Behavior:**

- **Normal Operation**: Full functionality via cloud integration
- **Control Availability**: All control methods active
- **Data Reporting**: Real-time telemetry to cloud platform
- **Safety**: All systems active with cloud monitoring

**Mobile App Integration:**

- Cloud-based control recommended
- BLE available as backup
- Real-time status updates via cloud
- Full scheduling and automation features

### WiFi Disconnected State

**Control Hierarchy:**

1. **BLE**: Primary control method
2. **Local Schedule**: Autonomous operation (if configured)
3. **Physical Button**: Override capability
4. **Safety Systems**: Automatic shutdowns

**Heater Behavior:**

- **Control Transition**: Automatic switch to BLE control
- **Safety Timeout**: Heating shutdown after 5 minutes if no BLE connection (safety feature to prevent uncontrolled heating)
- **Data Storage**: Local logging until WiFi restored
- **Recovery**: Automatic WiFi reconnection attempts

**Mobile App Integration:**

- BLE connection required for control
- Local control interface activation
- Limited scheduling (local only)
- Clear indication of offline status

### Provisioning States

#### BLE Provisioning Mode

**Heater Behavior:**

- **Heating Control**: Available via BLE commands
- **WiFi Status**: Disabled during provisioning
- **Control Methods**: BLE only
- **Duration**: Until WiFi credentials provided

**Mobile App Integration:**

- Provisioning interface required
- Heating control available during setup
- Status monitoring via BLE only
- Clear setup progress indication

#### SoftAP Provisioning Mode

**Heater Behavior:**

- **Heating Control**: Web interface or pre-set schedule only
- **BLE Status**: Disabled during SoftAP mode
- **Network**: Acts as WiFi access point
- **Duration**: Until WiFi configuration complete

**Mobile App Integration:**

- Web interface redirection
- Limited heating control
- No real-time data streaming
- Setup completion notification

---

## Heating Control Logic

### Temperature Processing

**Sensor Integration:**

- **MLX90614 IR Sensor**: Non-contact temperature measurement
- **Filtering**: 5-sample circular buffer with negative value filtering
- **Adaptive Operative Temperature (Actual Implementation)**:
  - `T_operative = T_object` (when heater_state != HEATER_OFF)
  - `T_operative = 0.5 × T_object + 0.5 × T_ambient` (when heater_state == HEATER_OFF)
- **Update Rate**: 1-second intervals (ENERGY_UPDATE_INTERVAL_MS)

**Temperature Control Algorithm:**

**Delta Calculation**: `temperature_delta = target_temperature - operative_temperature`

**Control Thresholds (from Heater_interface.h):**

- **HEATER_HIGH** (diff > THRESHOLD_HIGH = 1.5°C): Both elements active (HEATING_LOW_WATTS + HEATING_HIGH_WATTS = 1200W)
- **HEATER_MID** (THRESHOLD_MID < diff ≤ THRESHOLD_HIGH, 0.5-1.5°C): High element only (HEATING_HIGH_WATTS = 700W)
- **HEATER_LOW** (THRESHOLD_LOW < diff ≤ THRESHOLD_MID, -0.5-0.5°C): Low element only (HEATING_LOW_WATTS = 500W)
- **HEATER_OFF** (diff ≤ THRESHOLD_LOW = -0.5°C): Both elements off (0W)

**Power Ranges Across All Models:**

- **High Heating**: 600W-1250W (both elements)
- **Medium Heating**: 300W-750W (center element only)
- **Low Heating**: 300W-500W (edge element only)

**Hysteresis**: Built-in via threshold ranges to prevent rapid cycling

### Presence Detection Integration

**LD2410C mmWave Sensor:**

- **Detection Range**: Configurable 0.75-6m
- **Binary Output**: Immediate presence detection
- **Detailed Metrics**: Moving/stationary target distance and energy
- **Update Rate**: 1-second intervals

**Current Presence Integration (Implementation Status):**

- **Heating Control**: No integration - presence status does not affect heating behavior
- **Telemetry Only**: Presence used for telemetry interval adjustment (PRESENCE_TELEMETRY_TIME vs NORMAL_TELEMETRY_TIME)
- **Data Streaming**: Presence data available via BLE command 0x0C and automatic streaming
- **Future Enhancement**: Presence-based heating control could be added but is not currently implemented

### Safety Systems and Automatic Shutdowns

#### Temperature Safety

**Over-Temperature Protection:**

- **ESP32 Internal**: >90°C triggers immediate heating shutdown
- **Sensor Health**: Invalid readings trigger heating shutdown
- **Recovery**: Automatic restart when temperature normalizes

#### Connectivity Safety (Current Implementation Status)

**WiFi/BLE Disconnection Safety:**

- **Current Status**: NOT IMPLEMENTED - heating continues regardless of connectivity status
- **Behavior**: Auto mode heating operates independently of network connectivity
- **Risk**: Heating will continue indefinitely based solely on temperature control logic
- **Note**: Only manual control (BLE commands) or physical button can override heating when connected

#### Sensor Safety

**MLX90614 Sensor Failure:**

- **Detection**: Consecutive invalid readings or I2C failures
- **Action**: Heating disabled until sensor recovery
- **Recovery**: Automatic sensor power cycling via GPIO10
- **Backup**: Manual sensor board control via BLE command 0x0B

**LD2410C Sensor Failure:**

- **Detection**: UART communication failure
- **Action**: Presence detection disabled (heating continues)
- **Recovery**: Automatic UART re-initialization
- **Fallback**: Heating continues without presence integration

#### Power Management

**Energy Tracking:**

- **Real-time**: Power consumption calculation based on relay states
- **Daily Tracking**: Energy consumption totals
- **Safety Limits**: Configurable maximum power/energy limits
- **Reporting**: Energy data via BLE command 0x0F

---

## Mobile App Integration Guidelines

### State Management

**Connection Priority:**

1. **WiFi/Cloud**: Use when available for full functionality
2. **BLE**: Fallback for direct control when WiFi unavailable
3. **Status Monitoring**: Always prefer real-time data over cached values

**State Persistence:**

- **Operating Mode**: Persists across connections and power cycles
- **Target Temperature**: Persists in device memory
- **Network Credentials**: Persist until factory reset
- **Energy Data**: Persist across power cycles

### UI Synchronization

**Mode Transitions:**

- **Physical Button**: Monitor for mode changes via status notifications
- **Network Changes**: Handle WiFi/BLE connectivity changes gracefully
- **Operating Mode**: Update UI immediately when mode changes detected

**Real-time Updates:**

- **Temperature Data**: Adaptive BLE streaming (5s when presence detected, 30s normal, 5min idle)
- **Presence Data**: Adaptive BLE streaming (5s when presence detected, 30s normal, 5min idle)
- **System Status**: Monitor via WiFi status notifications
- **Error Conditions**: Immediate notification via status LED changes

### Control Availability Mapping

**Manual Mode:**

- ✅ Direct relay control interface
- ✅ Target temperature setting (for reference)
- ❌ Automatic scheduling
- ✅ Manual energy monitoring

**Auto Mode:**

- ❌ Direct relay control (automatic only)
- ✅ Target temperature control
- ✅ Schedule and automation features
- ⚠️ Presence monitoring (telemetry only, not heating control)

**Bypass Mode:**

- ❌ All heating controls (technician override)
- ✅ Monitoring and diagnostics
- ⚠️ Clear "Technician Mode" indication
- ✅ System status monitoring

### Error Handling and Recovery

**Connection Loss:**

- **WiFi Loss**: Automatic BLE fallback with user notification
- **BLE Loss**: Clear offline status indication
- **Both Lost**: Safety shutdown warning and reconnection prompts

**Sensor Failures:**

- **Temperature Sensor**: Clear error indication, heating disabled warning
- **Presence Sensor**: Reduced functionality warning, heating continues
- **Multiple Sensors**: System failure warning, contact support

**Safety Shutdowns:**

- **Over-temperature**: Clear warning, cooling period indication
- **Connectivity Timeout**: Safety shutdown explanation, reconnection guide
- **Sensor Failure**: Diagnostic information, recovery instructions

---

## Implementation Notes

### BLE Integration

Device control is available via BLE commands with mode-dependent functionality. See [BLE_INTERFACE.md](BLE_INTERFACE.md) for complete command reference and protocol details.

### Technical Implementation

**Stack Requirements:**

- All sensor tasks require 8192-byte stacks for enhanced logging
- Never reduce stack sizes - prevents system corruption

**Initialization Order:**

1. GPIO10 MOSFET control (critical for sensor power)
2. Power stabilization delay (100-250ms)
3. Sensor initialization (MLX90614, LD2410C)
4. BLE service initialization
5. WiFi and networking setup

**Safety Implementation:**

- Never log inside ISR handlers (causes crashes)
- Button uses pulldown configuration (HIGH = pressed)
- Temperature filtering via circular buffer with negative value rejection
- Automatic sensor recovery with power cycling capability

---

## Conclusion

This comprehensive operational guide provides the context mobile app developers need to understand heater behavior beyond the BLE protocol specification. The heater operates in multiple modes with different control hierarchies, safety systems, and connectivity dependencies that must be properly handled in user interfaces.

**Key Takeaways:**

- **Physical controls override all other control methods**
- **Network connectivity determines control hierarchy**
- **Operating modes affect which commands are effective**
- **Safety systems can override user control**
- **Real-time status monitoring is essential for proper UI state management**

For BLE protocol details, see [BLE_INTERFACE.md](BLE_INTERFACE.md).  
For physical interaction details, see [INTERACTIONS.md](INTERACTIONS.md).
