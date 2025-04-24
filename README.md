# Vario Power Panels
#### Client: Stephen Long Jr

## Architecture

- ESP32-C6 microcontroller with 8MB flash
- MLX90614 IR temperature sensor (3V variant, I2C interface, 1 second update interval)
- LD2410C motion sensor (GPIO binary presence output)
- Multicolor Status LED (RGB)

## Board Configuration

| Component       | GPIO Pins | Notes                         |
| --------------- | --------- | ----------------------------- |
| Heating Low     | GPIO18    | Controls lower power element  |
| Heating High    | GPIO19    | Controls higher power element |
| I2C Bus - SDA   | GPIO21    | SDA for MLX90614              |
| I2C Bus - SCL   | GPIO22    | SCL for MLX90614              |
| Binary Presence | GPIO23    | LD2410C Binary Output         |

## Relay Control Logic

The power panel supports two operation modes for controlling heating elements:

### Operation Modes

| Mode      | Description                                          |
| --------- | ---------------------------------------------------- |
| Manual    | Direct control of relay states from user/app         |
| Automatic | Temperature-based control using configured setpoints |

### Temperature Processing

- Operative temperature derived from ambient and object temperature samples
- Temperature values processed using sliding window moving average of last 5 negative-filtered samples
- Sampling rate matches MLX90614 update interval (1 second)

#### Negative-Filtered Sliding Window Implementation

The temperature processing algorithm works as follows:

1. **Negative Filtering**: Any temperature readings below 0°C are considered sensor errors and are discarded from processing
2. **Sample Collection**: Valid temperature samples (≥ 0°C) are collected in a circular buffer of size 5
3. **Moving Average**: The system calculates the average of all valid samples currently in the buffer
4. **Buffer Management**:
   - If a new sample is valid, it replaces the oldest sample in the buffer
   - If a new sample is invalid (negative), it is discarded and the buffer remains unchanged
   - If fewer than 3 valid samples are available in the buffer, the system uses the most recent valid sample instead of the average

This approach ensures that temperature readings remain stable and are not affected by occasional sensor errors or environmental noise, while still being responsive to actual temperature changes.

#### Operative Temperature Derivation

The system processes temperature in the following order:

1. Apply negative filtering and sliding window averaging to ambient temperature samples
2. Apply negative filtering and sliding window averaging to object temperature samples
3. Calculate operative temperature using filtered values: `Toperative = 0.7 × Tobject + 0.3 × Tambient`

**Edge Case Handling:**

- If either temperature source fails to provide valid readings (all negative values), use the last valid reading from that source
- If a temperature source has never provided valid readings since startup, use the other temperature source exclusively
- If both sources fail simultaneously, trigger error condition and fall back to last known good operative temperature
- System requires at least one valid operative temperature calculation before enabling automatic control

### Automatic Control Logic

In automatic mode, the system will control relays based on the temperature delta (difference between setpoint and current operative temperature):

| Temperature Delta (°C) | Heating Low | Heating High | Heat State |
| ---------------------- | ----------- | ------------ | ---------- |
| diff > 1.5             | ON          | ON           | high       |
| 0.5 < diff ≤ 1.5       | OFF         | ON           | mid        |
| -0.5 < diff ≤ 0.5      | ON          | OFF          | low        |
| diff ≤ -0.5            | OFF         | OFF          | off        |

These values would need to be tuned for the specific application and environment based on internal testing and feedback, so we need to be able to adjust them.

### Mode Control

- Mode switching (manual/automatic) available via BLE and MQTT interfaces
- Mode and configuration settings stored persistently across power cycles
- Users can control relay states directly in manual mode

## Wi-Fi Provisioning

The ESP32-C6 will need to connect to our servers to activate the heater under a secure connection. We'll need to provision the device over the air.

### BLE provisioning

Primary method of provisioning is via BLE. Company ID is 0x0EC3.
Device and Service IDs are yet to be determined. (TODO: Needs further discussion)

### Wi-Fi Access Point (Soft AP) provisioning

Secondary method of provisioning is via Wi-Fi Access Point. This will be used as a fallback if BLE is not available or not desired.

### Status LED Indications

- Solid Green: Normal operation
- Blinking Blue: Pairing mode
- Solid Red: Error condition
- Blinking Yellow: Firmware update in progress

### Button Interactions
- Short press to turn on/off
- Long press for 5 seconds to enter pairing mode
- Long press for 30 seconds to factory reset
