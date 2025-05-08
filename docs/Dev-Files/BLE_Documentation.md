
# 📱 BLE Services & Characteristics – Documentation for Mobile App Developers

## Overview
This document provides the Bluetooth Low Energy (BLE) **Service** and **Characteristic UUIDs**, descriptions, data formats, and read/write/notify permissions to help mobile app developers integrate with the device over BLE.


Company identifier according to [SIG specifications](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Assigned_Numbers/out/en/Assigned_Numbers.pdf)

|Company Name|Identifier|
|---|---|
| Herschel Infrared Ltd. | 0x0EC3 | 

---
## Primary Services

- **Service Name:** Herschel WiFI Provision service (custom)
- **Service UUID:** `7E40007E-B5A3-1000-<COMPANY_IDENTIFIER>-E50E24DCCA9E`

- **Service Name:** Herschel UART service (custom)
- **Service UUID:** `7E40007E-B5A3-2000-<COMPANY_IDENTIFIER>-E50E24DCCA9E`

---

## Characteristic: 

### WiFi Provsion

- **UUID:** `7E40007E-B5A3-1001-<COMPANY_IDENTIFIER>-E50E24DCCA9E`
- **Properties:** `Write`
- **Data Format:**
  - WiFi credential command as described in section "BLE COMMANDS"
- **Description:** Used to save wifi credentials to the device over BLE.

### Receive

- **UUID:** `7E40007E-B5A3-2001-<COMPANY_IDENTIFIER>-E50E24DCCA9E`
- **Properties:** `Write`
- **Data Format:**
  - Commands as described in section "BLE COMMANDS"
- **Description:** Used to issue command to the device over BLE.


### Transmit

- **UUID:** `7E40007E-B5A3-2002-<COMPANY_IDENTIFIER>-E50E24DCCA9E`
- **Properties:** `Notify`
- **Data Format:**
  - Commands response as described in section "BLE COMMANDS"
- **Example Response:** `0x06 0xA1 (success)` or `0x06 0xA2 (failed)`
- **Description:** Response to the command issued.

---

## BLE COMMANDS: structure and usage

Hex Code | Command | Type | Input | Response Format | Description
-- | -- | -- | -- | -- | --
0x01 | Power Off / Reboot | Control | 0x01 | 0x01 0xA1 (success)<br>0x01 0xA2 (failed) | Powers off or reboots the device.<br>Currently only reboot supported.
0x02 | Factory Reset | Control | 0x02 | 0x02 0xA1 (success)<br>0x02 0xA2 (failed) | Resets device to factory settings.
0x03 | Change Wi-Fi SSID/Password | Set | 0x03 [SSID_LEN] [SSID] [PWD_LEN] [PASSWORD] | 0x03 0xA1 (success)<br>0x03 0xA2 (failed) | Changes the Wi-Fi SSID and password.<br>Only available in "WiFi Provsion" BLE service.
0x04 | Read Internal Temperature | Get | 0x04 | 0x04 [FLOAT*] | Reads internal temperature of the device.
0x05 | Read Temp | Get | 0x05<br>0x05 [TARGET] | 0x05 [FLOAT*] | Reads temperature from IR sensor.<br>0x00 Object temp,<br>0x01 Ambient temp<br>If no parameter, defaults to object temprature.
0x06 | Set Heater Target Temperature | Set | 0x06 [FLOAT*] | 0x06 0xA1 (success)<br>0x06 0xA2 (failed) | Sets the desired target temperature.
0x07 | Read Relay States | Get | 0x07 | 0x07 [RELAY_STATE] | Gets the current state of relay outputs
0x08 | Set Relay States | Set | 0x08 [RELAY_STATE] | 0x08 0xA1 (success)<br>0x08 0xA2 (failed) | Controls the state of relays
0x09 | Set Device Mode | Set | 0x09 [0x01]<br>0x09 [0x02] | 0x09 0xA1 (success)<br>0x09 0xA2 (failed) | Sets the device mode (e.g. auto or manual)
0x0A | Read Presence | Get | 0x0A | 0x0A [BOOL**] | Retrieves presence status.
---
 \* Float value. Four bytes, LSB first.<br>
 \** Bool value. One byte. 0x00 if presence not detected, 0x01 if presence detected.


## Connection Behavior

- **Peripheral Role:** Advertises service on startup
- **MTU Size:** 517 bytes (negotiable)
- **Preferred Connection Interval:** 7.5–20 ms

---

## Example Workflow

1. **Scan for device** named `HERSCHEL_000001`.
2. **Connect** and discover services.
3. **Enable notifications** on UART characteristic.
4. **Provide wifi credentials** to the WiFi Provsion service.
5. **Write commands** to the control or trigger an action.
