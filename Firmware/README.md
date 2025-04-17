# Smart Heating System Firmware Development
## Instructions
- Flash the esp32c6 with the provided binaries.
- Once the device is flashed with the updated firmware, navigate to your mobile to wifi and bluetooth discovery. Device with name `HERSCHEL_000001` will appear.
- Now connect to the softAP connection by entering the password `12345678`.
- Once connected, open any browser and open url `192.168.4.1`.
- A web page will be opened, enter ssid and password there and click on submit.
- If the creds are correct, the device will be connected to the wifi network.
- Open nrf connect and you can still see your device there. Conenct to your device.
- There's a service appearing from which you can write wifi creds in json format like given below:
```
{
    "ssid": "abc_network",
    "password": "xyz_password"
}
```

## Build Instructions
- Clone a repository and go to folder `c6-vario` in the ESP-IDF CMD or PowerShell.
- Run `idf.py build` to build your project.
- Run `idf.py -p COMXX flash` to flash your esp32c6 chip.
- Run `idf.py -p COMXX monitor` to monitor your serial monitor.
- Else you can also run `idf.py -p COMXX flash monitor` to flash and monitor your serial monitor.
- Once the Wifi Provisioning is done, to bring your controller in SoftAP provisioning mode you have to erase your esp32c6 chip with the following command, `idf.py -p COMXX erase-flash`.
- However, you can always update your wifi credentials through ble since it is always turned on.

## UART SERVICE:
- UUID : 7e400001-b5a3-f393-e0a9-e50e24dcca9e

- This serviced is developed on the nordic uart service pattern

- Contains 2 characteristics (Tx and Rx) and required descriptors

- Rx Characteristic requires Notifications to be enabled from the BLE client

- In-order to send data larger than 20 bytes , BLE client is required to request larger MTU size (as per the requirement) from the UART service.

#### Rx Characteristic:
- UUID : 7e400002-b5a3-f393-e0a9-e50e24dcca9e
- Properties: Notify

#### Tx Characteristic:
- UUID : 7e400003-b5a3-f393-e0a9-e50e24dcca9e
- Properties: Write