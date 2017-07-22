#!/usr/bin/env python
# -------------------------------------------------------------------------------
#
# Name:         Wirecast WIFI Tally Bridge
# Purpose:      Control WIFI Tally Lights via Wirecast
#
# Copyright 2016-2017 Benjamin Yaroch
#
# This is free software: you can redistribute it and/or modify 
# it under the terms of the GNU General Public License as published by the 
# Free Software Foundation, either version 3 of the License, or (at your 
# option) any later version.
#
# The software is distributed in the hope that it will be useful, but 
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
# or FITNESS FOR A PARTICULAR PURPOSE. 
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along 
# with this software. If not, see: http://www.gnu.org/licenses/
#
# IMPORTANT: If you want to use this software in your own projects and/or products,
# please follow the license rules!
#
# -------------------------------------------------------------------------------

from __future__ import print_function
import sys
import argparse
import re
import time
import socket
import win32com.client
from ast import literal_eval

# Constants
DefaultTallyLayer = 3
Live = 2
Preview = 1
Clear = 0

# Variables/Config
tallyLayer = 3
TallySendInterval = 0.250               # Interval in seconds
regex = "\[(.*?)\]"                     # Regular Expression to find Tally info in shot name between []

# Network Interface Settings
MCAST_GRP = "224.0.0.20"                # Multicast Group IP
MCAST_PORT = 3000                       # Multicast port number
TTL = 2                                 # Time-to-live
Bind = False                            # Bind to specific network adapter
localIP = 0

TallyState = [0 for i in range(256)]    # Tally State buffer 0-255
dict = {'Live': '', 'Preview': '', 'Autolive': -1, 'isActive': -1}
    
# =======================================================
# Helper Functions
# =======================================================
def parseTallyInfo(shotName, state):                                    # Get Tally info from Shot Name - [T:1,2,3]
    tally_data = re.findall(regex, str(shotName))                       # Save what's between the brackets []

    if tally_data:
        key_value = tally_data[0].split(":")                            # Split Key/Value pair

        if key_value[0] == "T" or key_value[0] == "t":                  # If key exists...
            tally_ids = key_value[1].split(",")                         # ...get comma separated values.

            for idx in range(0, len(tally_ids)):                        # Process each value as a tally ID
                try:
                    tallyNum = int(tally_ids[idx])-1                    # Convert ASCII tally ID to integer
                    TallyState[tallyNum] = state                        # Write tally state to buffer (live or preview)
                except ValueError:
                    pass

def getWirecastData():
    try:
        wc = win32com.client.GetActiveObject("Wirecast.Application").DocumentByIndex(1)

        # Get Preview Shot Name
        PreviewShotID = wc.LayerByIndex(tallyLayer).PreviewShotID()
        dict['Preview'] = wc.ShotByShotID(PreviewShotID).Name

        # Get Live Shot Name
        LiveShotID = wc.LayerByIndex(tallyLayer).LiveShotID()
        dict['Live'] = wc.ShotByShotID(LiveShotID).Name

        # Get Autolive State
        AutoLiveActiveState = wc.AutoLive

        if dict['Autolive'] != AutoLiveActiveState:
            dict['Autolive'] = AutoLiveActiveState
            if AutoLiveActiveState == 1:
                print("Autolive: ON")
            else:  # Preview
                print("Autolive: OFF")

        if dict['isActive'] != True:
            dict['isActive'] = True
            print("Wirecast: OPEN")

        return True

    except:
        if dict['isActive'] != False:
            dict['isActive'] = False
            print("Wirecast: CLOSED")

        return False

def handleArguments():
    global tallyLayer
    global localIP
    global Bind

    parser = argparse.ArgumentParser()
    parser.add_argument("-l", "--layer", help="sets layer tally monitors (1-5)", type=int, default=3)
    parser.add_argument("-b", "--bind", help="binds to internet connected network adapter", action="store_true")
    args = parser.parse_args()

    if args.layer >= 1 and args.layer <= 5:         # Set Layer Number based on argument value
        tallyLayer = args.layer
    else:
        print("ERROR: Invalid layer number, must be 1-5")
        sys.exit()

    if args.bind:                                   # Get local IP and set connection binding to True
        localIP = getLocalIp()
        if localIP:
            print("Binding: " + localIP)
            Bind = True
        else:
            Bind = False
    else:
        print("Binding: Disabled")
        Bind = False

def getLocalIp():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.connect(("8.8.8.8", 80))
    localIP = s.getsockname()[0]                    # IP of NIC with internet connection
    s.close()

    return localIP

# =======================================================
# Main
# =======================================================
if __name__ == "__main__":
    handleArguments()

    print("Multicast: " + MCAST_GRP + ":" + str(MCAST_PORT))
    print("Layer: " + str(tallyLayer))

    mcastSock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    mcastSock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, TTL)
    if Bind:
        mcastSock.bind((localIP, MCAST_PORT))                               # May be needed for multiple NICs

    while True:
        getWirecastData()                                                   # Get latest Wirecast information

        for idx in range(0, len(TallyState)):                               # Clear Tally State buffer to be rewritten
            TallyState[idx] = Clear

        if dict['isActive']:
            if dict['Autolive'] == 0:                                       # If Autolive is Off check preview
                parseTallyInfo(dict['Preview'], Preview)

            parseTallyInfo(dict['Live'], Live)

        mcastSock.sendto(bytearray(TallyState), (MCAST_GRP, MCAST_PORT))    # Send multicast packet to tally lights

        time.sleep(TallySendInterval)
