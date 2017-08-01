# WIFI Tally Light 

## Description
I was doing events that could benefit from some [tally lights]( https://en.wikipedia.org/wiki/Tally_light). But after researching my options I found that commercial systems were very expensive and didn't connect to the software I was using ([Wirecast]( https://www.telestream.net/wirecast/overview.htm)). So, I set out to build my own. Initially I was going to build a wired system based on an [open source design by Skaarhoj]( http://skaarhoj.com/designs/tally-box-system), the problem was I didn't want to carry and run more cables. So instead I decided to design and build my own WIFI tally light system inspired by another [open source project I found](https://github.com/henne-/wifitally). Since I already use a private WIFI network for events I didn’t need to bring anything additional to run tally lights.

My design allows for up to 255 tally lights (1-256). Each light has independently dimmable Tally and Operator light(s) and the operator sees both live and preview indicators. Each tally light is configurable via a webpage, allowing the user to change WIFI settings, the intensity of each LED, and upgrade firmware. Because this design uses WIFI it can be used internationally without any special licensing. 

One of the nicest parts of this design is the software bridge used to control them. It can control all the affected cameras for a given shot, even a PIP shot, and does so just by including special text in the shot name. 

### Features
* Tally and Operator LEDs
* Live and Preview Indication
* Dimmable LEDs
* WIFI connectivity (802.11 b / g / n)
* Webpage configuration
* Up to 256 Individually addressable lights
* Upgradable firmware
* Wirecast, vMix, Blackmagic ATEM ([additional hardware required]( https://github.com/henne-/wifitally))

## Hardware
### Programming
The initial programming of the tally light requires a USB to serial converter, after that new firmware can be uploaded via the web configuration. 

Initial programming is a two-part process: 
* Compile and Load the firmware via the Arduino IDE
* Upload static files to flash (stored in SPIFFS)

This tutorial explains how to program the Adafruit Huzzah with the Arduino IDE: https://learn.adafruit.com/adafruit-huzzah-esp8266-breakout/using-arduino-ide

To upload the static files via the Arduino IDE you will need this tool: https://github.com/esp8266/arduino-esp8266fs-plugin

### Configuration
1. Power on tally light wait ~30 seconds, both operator LEDs will stop flashing (for more info see “LED Indicators”)
2. Press and Hold the configuration switch until both operator LEDs are lit.
3. From a mobile device or laptop connect to the access point named WIFITally_xxxxxx (the Xs represent the last 3 digits of the devices MAC address).
4. Once connected open a browser and enter the following: http://192.168.4.1
5. The device configuration page will load, now you can modify the configuration settings as needed.

Once connected to the network the tally light can be reconfigured at any time. Just open a browser and enter the devices name http://WIFITally_xxxxxx (the Xs represent the last 3 digits of the devices MAC address) or the IP address http://192.168.1.2

NOTE: When you modify the SSID or Password the device will automatically reset.

### LED Indicators
**Normal Operation** - The green operator LED means the camera is in Preview and the red operator and tally LEDs mean the camera is Live. 

**Red Blinking** - At startup the red operator LED blinks the device ID. Each digit, three in total, is preceded by the LED being lit at 50% followed by a series blinks indicating a single digit. No blinks equals 0. 

EXAMPLE: If the device ID is set to 3 the sequence would be 0-0-3, 256 would be 2-5-6.

**Red & Green Blinking** - While trying to connect to the wireless network both the red and green operator LEDs will blink. 

**Red & Green Glowing or Off** – When both glow the tally light is connected to the wireless network, if both are off the tally light is disconnected.  

**Red & Green On** – The internal Access Point (AP) is active. 

### Configuration Switch
The configuration switch has two functions, activating the internal Access Point (AP) and restoring the default configuration. 

**Activate the Internal AP** - Press and hold the configuration switch until both the red and green operator LEDs turn on, then release. To deactivate the AP just power cycle the tally light. 

**Restore Default Configuration** - Press and hold the configuration switch then power on the tally light. Keep holding the switch until both the red and green operator LEDs turn on, then release. The configuration will be returned to defaults and activate the internal AP.

### Upgrade Firmware
Open a browser and enter the devices name http://WIFITally_xxxxxx (the Xs represent the last 3 digits of the devices MAC address) or the IP address http://192.168.1.2 then click “Upgrade.” Browse to the binary file and click “UPDATE.” If successful a message will appear saying the tally light is rebooting and after ~30 seconds the page will refresh and return to the homepage.

## Software
### Installation
The software is written in Python, which needs to be installed prior to use. The preferred version is Python 3.x. 

The Wirecast version of this application uses COM to communicate so additional modules will need to be installed. You can either use your preferred package manager and install them manually or install [ActivePython]( https://www.activestate.com/activepython/downloads) which includes all the necessary modules. 
### Launching Application
You can launch the application by either double-clicking the script or running it from a command-line prompt (“python3 wcTallyBridge.py” or “python3 vmixTallyBridge.py”). For Wirecast the software monitors a single layer in Wirecast (3 by default) and changes the tally lights based on what is currently live or in preview on that layer. For vMix the software monitors input and overlay and changes the tally lights based on what is currently live or in preview.
### Mapping Lights to Shot
To map a tally light to a shot you must add special characters to the shot name. You need add two brackets, a “T”, a colon, and the ID numbers of the tally lights you want to control. For multiple IDs each must be separate by a comma. 

So, for a single tally light you would add the following to the shot name [T:2] which would activate the tally light with an ID of 2. For a PIP shot [T:1,2] which would activate tally lights with an ID of 1 and 2. 

### Command-line Options
**Wirecast**
By default, the software will monitor layer 3 in Wirecast, however you can change to a different layer (1-5) by using the “Set Layer” command-line option. 

Wirecast command-line options and examples of their use. 
* **Set Layer:** -l [layer number] or --layer [layer number] (“wcTallyBridge.py -l 2”)
* **Bind to Adapter:** -b or –-bind  (“wcTallyBridge.py –bind”)
* **Help:** -h or -–help  (“wcTallyBridge.py -h”)


**vMix**
By default, the software will use a localhost IP of 127.0.0.1 and a Port of 8088. These are the defaults for vMix, however you can change to a remote IP or name allowing you to run this application on a separate machine.

vMix command-line options and examples of their use. 
* **Set Address:** -a [IP or Computer Name] or --address [IP or Computer Name] (“vmixTallyBridge.py -a 192.168.1.2”)
* **Port:** -p [Port Number] or --port [Port Number] (“vmixTallyBridge.py -p 8082”)
* **Bind to Adapter:** -b or –-bind  (“wcTallyBridge.py –bind”)
* **Help:** -h or -–help  (“vmixTallyBridge.py -h”)


### Troubleshooting
**Lights Not Responding** – This can occur when data is being sent to the wrong network adapter. The easiest way to resolve this is to disable all the network adapters you aren’t using. Alternatively, you can try the “Bind to Adapter” command-line option which will send data to the network card that is connected to the internet. 

Another reason the lights might not respond is due to the network configuration. This software uses multicast to send data to the lights. Most routed networks block multicast data across subnets so you need to ensure that multicast is allowed on the network. A simple test is to connect the entire system to a private access point or router, if everything functions correctly multicast is likely blocked on your network.

## Licenses
### Software
This software is free so you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.

The software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the [GNU General Public License]( http://www.gnu.org/licenses) for more details. 

### Everything else
Unless otherwise noted everything else (schematics, PCB designs, enclosure designs, documents, spreadsheets, etc.) is free and released under [Creative Commons BY-SA](http://creativecommons.org/licenses/by-sa/3.0/). 

**IMPORTANT: If you want to use any of this in your own projects and/or products, please follow the license rules!**
