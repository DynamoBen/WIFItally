# WIFI Tally Light 

## Description
I was doing events that could benefit from some [tally lights]( https://en.wikipedia.org/wiki/Tally_light). But after researching my options I found that commercial systems were very expensive and didn't connect to the software I was using ([Wirecast]( https://www.telestream.net/wirecast/overview.htm)). So, I set out to build my own. Initially I was going to build a wired system based on an [open source design by Skaarhoj]( http://skaarhoj.com/designs/tally-box-system), the problem was I didn't want to carry and run more cables. So instead I decided to design and build my own WIFI tally light system inspired by another [open source project I found](https://github.com/henne-/wifitally). Since I already use a private WIFI network for events I didn’t need to bring anything additional to run tally lights.

My design allows for up to 256 tally lights (1-256). Each light has independently dimmable Tally and Operator light(s) and the operator sees both live and preview indicators. Each tally light is configurable via a webpage, allowing the user to change WIFI settings, the intensity of each LED, and upgrade firmware. Because this design uses WIFI it can be used internationally without any special licensing. 

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

## Build this project
[Check out the wiki for more information](https://github.com/DynamoBen/WIFItally/wiki)

## Licenses
### Software
This software is free so you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.

The software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the [GNU General Public License]( http://www.gnu.org/licenses) for more details. 

### Everything else
Unless otherwise noted everything else (schematics, PCB designs, enclosure designs, documents, spreadsheets, etc.) is free and released under [Creative Commons BY-SA](http://creativecommons.org/licenses/by-sa/3.0/). 

**IMPORTANT: If you want to use any of this in your own projects and/or products, please follow the license rules!**
