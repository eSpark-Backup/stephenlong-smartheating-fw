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
- To build the project, download the project zip from the following link: [Download Link](https://mega.nz/file/WdxGla4B#1Je9ymL9tHM3FN-_onfRJI2DFWKOXPMozMmlD3qI4s4).
- Unzip to your desired path and open project folder `wifi_prov_esp32c6_v5.4.1` in your vscode.
- Run `idf.py build` to build your project.
- Run `idf.py -p COMXX flash` to flash your esp32c6 chip.
- Run `idf.py -p COMXX monitor` to monitor your serial monitor.
- Else you can also run `idf.py -p COMXX flash monitor` to flash and monitor your serial monitor.
- Once the Wifi Provisioning is done, to bring your controller in SoftAP provisioning mode you have to erase your esp32c6 chip with the following command, `idf.py -p COMXX erase-flash`.
- However, you can always update your wifi credentials through ble since it is always turned on.
