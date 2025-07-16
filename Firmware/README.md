# ESP32-C6 Vario Power Panel Firmware

Advanced infrared heater control system with comprehensive sensor integration and cloud connectivity.

## Hardware Architecture

### Core Components

- **ESP32-C6-WROOM-1 microcontroller** (8MB flash)
- **MLX90614 3V ESF-BAA IR temperature sensor** (non-contact infrared, TO-39 package)
- **Hi-Link LD2410C 24GHz mmWave radar sensor** (5V, UART and binary output for still and moving presence detection)
- **WS2812 Addressable RGB Status LED**
- **MOSFET for MLX90614 power control**
- **240V AC Relay control** for dual heating elements
- **Pairing Button** (GPIO switch with long-press detection)

### Board Configuration

| Component                  | GPIO Pins | Notes                                                                |
| -------------------------- | --------- | -------------------------------------------------------------------- |
| Heating Low Relay          | GPIO18    | Controls lower power element                                         |
| Heating High Relay         | GPIO19    | Controls higher power element                                        |
| I2C Data (SDA)             | GPIO21    | MLX90614 communication                                               |
| I2C Clock (SCL)            | GPIO22    | MLX90614 communication                                               |
| Sensor Board Power Control | GPIO10    | Controls MOSFET for sensor board power (MLX90614 and LD2410 sensors) |
| Presence UART TX           | GPIO17    | LD2410C UART Transmit                                                |
| Presence UART RX           | GPIO16    | LD2410C UART Receive                                                 |
| Binary Presence Output     | GPIO23    | LD2410C Binary Output                                                |
| Status LED Data In         | GPIO11    | WS2812 RGB LED Data In                                               |
| Pairing Button             | GPIO2     | Input for pairing and factory reset                                  |

## Development Environment

This project uses **ESP-IDF v5.4.1** for ESP32-C6 development.

### Build Commands

All commands should be run from this directory (`c6-vario/`):

- **Setup ESP-IDF environment**: `get_idf` (or `source ~/esp/esp-idf/export.sh`)
- **Build project**: `idf.py build`
- **Flash firmware**: `idf.py -p /dev/cu.usbmodemXXXX flash` (macOS) or `idf.py -p COMXX flash` (Windows)
- **Monitor serial output**: `idf.py -p /dev/cu.usbmodemXXXX monitor`
- **Flash and monitor**: `idf.py -p /dev/cu.usbmodemXXXX flash monitor`
- **Erase flash**: `idf.py -p /dev/cu.usbmodemXXXX erase-flash`
- **Full clean build**: `idf.py fullclean && idf.py build`

**Note**: The ESP-IDF environment must be activated before running build commands.

### Port Detection

Use `ls /dev/cu.usbmodem*` on macOS to find the correct USB serial port.

## Project Structure

```text
c6-vario/
├── components/           # ESP-IDF Components
│   ├── App/              # Device configuration and NVS management
│   ├── Heater_Interface/ # Relay control and automatic heating logic
│   ├── LD2410_interface/ # mmWave presence sensor integration
│   ├── MLX90614_Interface/ # IR temperature sensor with filtering
│   ├── Status_led/       # WS2812 LED status indication
│   ├── ThingsBoard/      # Cloud telemetry and HTTP API
│   └── Wireless_Interface/ # BLE services and WiFi provisioning
├── libraries/            # Hardware driver libraries
│   ├── LD2410b_driver/   # mmWave radar sensor driver
│   ├── LED_strip_encoder/ # WS2812 LED driver
│   └── MLX90614_driver/  # IR temperature sensor driver
├── main/                 # Application entry point
│   ├── app_main.cpp      # Main application and BLE command execution
│   └── defines.h         # GPIO definitions and system constants
├── schematic/           # Hardware schematics and designs
├── BLE_INTERFACE.md      # Complete BLE specification and commands
├── INTERACTIONS.md       # User interaction guide (button, LED, pairing)
└── THINGSBOARD_SCHEMA.md # Cloud integration data structure
```

## Connectivity

The ESP32-C6 connects to ThingsBoard IoT platform for device management and control. The device supports BLE and WiFi provisioning for network setup.

### Provisioning Methods

- **Primary**: BLE provisioning (Company ID: 0x0EC3, Device: `HERSCHEL_000001`)
- **Fallback**: WiFi Access Point (SoftAP) mode

For detailed provisioning procedures and user interactions, see [INTERACTIONS.md](INTERACTIONS.md).

## Documentation References

**Complete Product Documentation:**

- **[BLE_INTERFACE.md](BLE_INTERFACE.md)**: BLE API specification, commands, and data streaming
- **[INTERACTIONS.md](INTERACTIONS.md)**: Button controls, LED patterns, and user interface
- **[modes.md](modes.md)**: Operating modes, heating control logic, and safety systems
- **[THINGSBOARD_SCHEMA.md](THINGSBOARD_SCHEMA.md)**: Cloud platform integration and data structures
