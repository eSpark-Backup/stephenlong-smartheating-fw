
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
- **Service UUID:** `7E40007E-B5A3-1000-<COMPANY IDENTIFIER>-E50E24DCCA9E`

- **Service Name:** Herschel UART service (custom)
- **Service UUID:** `7E40007E-B5A3-2000-<COMPANY IDENTIFIER>-E50E24DCCA9E`

---

## Characteristic: 

### WiFi Provsion

- **UUID:** `7E40007E-B5A3-1001-<COMPANY IDENTIFIER>-E50E24DCCA9E`
- **Properties:** `Write`
- **Data Format:**
  - WiFi credential command as described in section "BLE COMMANDS"
- **Description:** Used to save wifi credentials to the device over BLE.

### Receive

- **UUID:** `7E40007E-B5A3-2001-<COMPANY IDENTIFIER>-E50E24DCCA9E`
- **Properties:** `Write`
- **Data Format:**
  - Commands as described in section "BLE COMMANDS"
- **Description:** Used to issue command to the device over BLE.


### Transmit

- **UUID:** `7E40007E-B5A3-2002-<COMPANY IDENTIFIER>-E50E24DCCA9E`
- **Properties:** `Notify`
- **Data Format:**
  - Commands response as described in section "BLE COMMANDS"
- **Example Response:** `0x06 0xA1 (success)` or `0x06 0xA2 (failed)`
- **Description:** Response to the command issued.

---

## BLE COMMANDS: structure and usage

Hex Code | Command | Type | Input | Response Format
-- | -- | -- | -- | --
0x01 | Power Off / Reboot | Control | 0x01 | 0x01 0xA1 (success) or 0x01 0xA2 (failed)
0x02 | Factory Reset | Control | 0x02 | 0x02 0xA1 (success) or 0x02 0xA2 (failed)
0x03 | Change Wi-Fi SSID/Password | Set | 0x03 [SSID_LEN] [SSID] [PWD_LEN] [PASSWORD] | 0x03 0xA1 (success) or 0x03 0xA2 (failed)
0x04 | Read Internal Temperature | Read | 0x04 | 0x04  [TEMP_H] [TEMP_L]
0x05 | Read Temp / Presence | Read | 0x05 | 0x05 [TEMP_H] [TEMP_L] 
0x06 | Set Target Temperature | Set | 0x06 [TEMP_H] [TEMP_L] | 0x06 0xA1 (success) or 0x06 0xA2 (failed)
0x07 | Read Relay States | Read | 0x07 | 0x07  [RELAY_STATE]
0x08 | Set Relay States | Set | 0x08  [RELAY_STATE] | 0x08 0xA1 (success) or 0x08 0xA2 (failed)
0x09 | Set Device Mode | Set | 0x09  [0x01] (or) [0x02] | 0x08 0xA1 (success) or 0x08 0xA2 (failed)

---

## Connection Behavior

- **Peripheral Role:** Advertises service on startup
- **MTU Size:** 517 bytes (negotiable)
- **Preferred Connection Interval:** 7.5–20 ms

---

## Example Workflow

1. **Scan for device** named `HERSCHEL_000001`.
2. **Connect** and discover services.
3. **Enable notifications** on UART characteristic.
4. **Write commands** to the control or trigger an action.
