## LoRa Boat Monitor

![LoRa Boat Monitor](project/pictures/LoRa_Bootsmonitor.png)

Image: LoRa Boat Monitor

# LoRa-Boat-Monitor
Fork of [LoRa Boat Monitor](https://github.com/norbert-walter/LoRa-Boat-Monitor) from GitHub from Norbert Walter

I made several changes to the code:
- Put the files for the web server (*.html) from linked header files into file system in the flash.
    You can copy and modify single files, without flashing the hole image.
- use the "MCCI LoRaWAN LMIC library" instead of "IBM LMIC framework".
- added the option to bring device into sleep mode.
- send data via Lora or Wifi.
-  

Small hardware changes:
  - cut the +5V line to the GPS module, and wire the GPS +5V pin to one the relay pins (X3-3).
  - The Other (X3-2) connect to +5V of the Board.
This gives you the option, to shutdown the GPS Modul in sleep mode.

Another change that i made is the related to the IN1 (alarm1):
 - connect X1-4 to GND
 - connect X1-3 to the output of your Batterie switch (turn Main Batterie on, will start the Lora-Boat-Monitor, turn of your switch will turn the Lora-Boat-Monitor into Standby)
    (I changed the IN1/alarm1 logic from "low active" to "high active")
This makes ist possible to set the ESP into sleep mode and reduce power consumption.

Project home page: https://open-boat-projects.org/en/lora-bootsmonitor/

The LoRa boat monitor is used to monitor the boat when it is absent. Various measured values are continuously recorded at freely adjustable time intervals and transferred to the [LoRaWAN](https://www.lora-wan.de/) forwarded. The data is from the TTN server ([The Thinks Network](https://thethingsnetwork.org/)) received in Amsterdam and cached and then sent to [Ubidots](https://ubidots.com/) forwarded as a web frontend. The data transmission is secured by encryption up to Ubidots. The measured data is displayed in Ubidots and various notifications can be sent by email when measured values are exceeded.

There are a large number of [LoRa gateways](https://thethingsnetwork.org/map) which can receive the sent measurement data and forward it to the TTN server. Many LoRa gateways are run by private individuals on a non-profit basis. Anyone who wants can operate their own gateway and make it available to the general public. The radio technology uses the license-free frequency range around 868 MHz and uses a special transmission technology (chirp) to achieve large ranges of up to 50 km at low data rates. The ranges depend on the type of transmission, the antenna height and the environment. In built-up areas such as cities, typical ranges of 1… 4 km are possible. In open environments such as lakes and the sea, up to 50 km can be reached. There are no costs for data transmission when sending LoRa telegrams. This is the big difference to other long range transmission technologies like [SIGFOX](https://www.sigfox.com/) and mobile data networks such as 3G / 4G / 5G. If no LoRa gateway is within range, a simple 1-channel or 3-channel LoRa gateway can be set up with the same board. Only a few components are then left out and a different firmware used. Alternatively, the measured values can also be sent directly to Ubidots via WLAN, provided that a WLAN is within range.


![LoRa Data Transmission](project/pictures/LoRaWAN_Technology.jpg)

Image: LoRa data transmission Semtech GmbH

![LoRa Data Transmission](project/pictures/LoRa_Blockschaltbild.png)

Image: Block diagram LoRa boat monitor

## The LoRa boat monitor has the following functions:

* 10… 32V supply voltage
* 1.2W power consumption
* LoRa transmitter and receiver with OLED display
* 868 MHz, SF7… SF12, 100 mW transmission power
* Dynamic spreeding factor adjustable SF7… FS10
* Supports channel 0...7 dynamic and fix
* Data transfer rate: 0.3 to 50 kbit / s
* Max. Telegram length for user data: 200 bytes
* Range: built-up area 1… 4km, open area up to 50km
* LoRa transmission interval 30s… 2.1h
* One-channel, three-channel and eight-channel modes can be set
* Feeding the data into The Thinks Network (TTN)
* Parameterization of the LoRa boat monitor possible via return channel (channel, SF, transmission interval, relay)
* WLAN (2.4 GHz) for alternative data transmission
* Web interface for operation
* Firmware update possible via WLAN and Internet
* GPS sensor for geographic location coordinates
* BME280 for measuring temperature, humidity and air pressure
* 1x battery voltage measurement (0… 32V service battery)
* 1x potential-free alarm contact eg for bilge and door monitoring
* 2x tank sensors (0 ... 180 Ohm) with percentage display
* Calibratable tank and voltage sensors
* 1x Reais output for potential-free switching of loads with specified time (5min ... 21h) up to 3A (12V or 230V)
* 2x 1Wire connection for temperature sensors DS18B20 for battery monitoring and refrigerator
* Monitoring and alerting via Ubidots web frontend
* Alerting by e-mail if limit values are exceeded via Ubidots
* Automatic data storage for the last 31 days at Ubidots
* Use of cheap embedded modules
* No SMD components on the board and easy to build
* Board can be ordered here:[https://aisler.net/p/GEIYWWXZ](https://aisler.net/p/GEIYWWXZ)
* Android app

## Create your own code

You have the option of creating your own code using the online [software development environment Gitpod](https://gitpod.io/#https://github.com/norbert-walter/LoRa-Boat-Monitor). Follow this link and a finished development environment similar to PlatformIO will be created that runs in your browser with all code components. To compile the software, enter ***bash run*** in the terminal. In the workspace you will find the compiled binary code under ***.pio/build/heltec_wifi_lora_32_V2/firmware.bin***. With a right click you can download the binary file. You need a account for Gitpod.

## Device flash process

With the [Web Flash Tool](https://norbert-walter.github.io/LoRa-Boat-Monitor/flash_tool/esp_flash_tool.html) you can very easily flash the device with the firmware from the web browser. You need a Chrome or Edge browser and a USB connection cable. If the device is connected to the USB port of the computer, the serial connection can be selected and the flash process can then be started.

## How can I use it?


## What is the current status?

I have completed many tasks but still not everything is perfect so far.

- [x] Technical recherche
- [x] Evaluation of function
    - [x] LoRa trasmission tests
    - [x] Accessibility of gateways
    - [x] Measurement of telegram losses 
    - [x] Test spreeding factors
    - [x] Antenna test
    - [x] TTN configuration
    - [x] Ubitots configutarion and restrictions
- [x] Mechanical design
    - [x] 3D parts für Onshape
- [x] Electronic design
    - [x] Circuit design
    - [x] Board design
    - [x] Customer sample circuit
- [ ] Software
    - [x] Basic functionality for LoRa
    - [x] LoRa back channel for configuration
    - [x] Dynamic spreedung factor
    - [x] Voltage measurement
    - [x] Tank sensors
    - [x] GPS modul
    - [x] BME280
    - [x] Alarm sensors
    - [x] Relay function
    - [x] JSON
    - [x] Web-Updater
    - [x] Web-Configurator
    - [x] Web server as data display
    - [x] VE.Direct support for Victron
    - [ ] Ubidots connection via WiFi
    - [x] NMEA0183 WiFi
    - [x] NMEA0183 Serial
    - [ ] MQTT
    - [x] Andoid App
    - [x] LoRa gateway firmware
    - [x] Migration to TTS V3
- [x] Components ordering
    - [x] Electronic board
    - [x] Mechanical parts
    - [x] Fittings
- [x] Prototyp creation
    - [x] LoRa boat monitor
    - [x] LoRa gateway
- [ ] Production Zero series
    - [ ] DIY Kits
    - [x] Assembled LoRa boat monitor
- [x] Shipping Zero Series
- [x] Documentation
    - [x] Documentation and update server on GitLab
    - [x] Technical description
    - [x] Construction manual
    - [x] Part list
    - [x] User manual
    - [x] Data sheet
    - [x] Flash instructions
- [x] Application tests
    - [x] Temperature 20...40°C
    - [x] LoRa range test (SF7...SF12)
    - [x] WiFi stability test
    - [x] Connection test
    - [x] Long time test
    - [x] User experiences
- [ ] Extensions
    - [ ] Battery current sensor
    - [ ] Actions and alarms depends on events and measuring values
    - [ ] Build a LoRa gateway structure on two towers for the IJsselmeer in Netherlands 
- [ ] Certification
    - [ ] CE Certification


## Questions?

For questions or suggestions please get in contact via email at norbert-walter(at)web.de.

If you like to help or consider this project useful, please donate. Thanks for your support!

![Donate](project/pictures/Donate.gif)

[Donate with PayPal](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=5QZJZBM252F2L)
