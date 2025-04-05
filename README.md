# Smart Heating System Firmware Development
## Instructions
- Flash the esp32c6 with the provided binaries.
- Once the device is flashed with the updated firmware, navigate to your mobile to wifi and bluetooth discovery. Device with name `zyntellitech_{MAC}` will appear.
- Now connect to the softAP connection by entering the password `zyntellitech_123456`.
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