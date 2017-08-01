#!/usr/bin/python
# -------------------------------------------------------------------------------
#
# Name:         vMix WIFI Tally Bridge
# Purpose:      Control WIFI Tally Lights via vMix
#
# Python Version: 3.x or higher
#
# Copyright 2017 Benjamin Yaroch
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
# please heed the license rules!
#
# -------------------------------------------------------------------------------

from __future__ import print_function
import argparse
import re
import time
import socket
import urllib.request
import xml.etree.cElementTree as ET

# Constants
Live = 2
Preview = 1
Clear = 0

# Variables/config
TallySendInterval = 0.250               # Interval in seconds
regex = "\[(.*?)\]"                     # Regular Expression to find Tally info in shot name between []

Destination = "127.0.0.1"
Port = "8088"

# Network Interface Settings
MCAST_GRP = "224.0.0.20"                # Multicast Group IP
MCAST_PORT = 3000                       # Multicast port number
TTL = 2                                 # Time-to-live
Bind = False                            # Bind to specific network adapter
Local_IP = 0

TallyState = [0 for i in range(256)]    # Tally State buffer 0-255
dict = {'Live': '', 'Preview': '', 'isActive': -1}
overlayShots = [0 for i in range(6)]


# =======================================================
# Helper Functions
# =======================================================
def parse_tally_info(shotName, state):                      # Get Tally info from Shot Name - [T:1,2,3]
    tally_data = re.findall(regex, str(shotName))           # Save what's between the brackets []

    if tally_data:
        key_value = tally_data[0].split(":")                # Split Key/Value pair

        if key_value[0] == "T" or key_value[0] == "t":      # If key exists...
            tally_ids = key_value[1].split(",")             # ...get comma separated values.

            for idx in range(0, len(tally_ids)):            # Process each value as a tally ID
                try:
                    tally_num = int(tally_ids[idx]) - 1     # Convert ASCII tally ID to integer
                    TallyState[tally_num] = state           # Write tally state to buffer (live or preview)
                except ValueError:
                    pass


def get_vmix_data():
    try:
        feed = urllib.request.urlopen('http://' + Destination + ':' + Port +'/API')
        tree = ET.parse(feed)

        # Get Preview and Live Shot Name
        preview_shot_id = tree.find('preview').text
        live_shot_id = tree.find('active').text

        # Get Shot Name
        for input in tree.iter('input'):
            if input.get('number') == preview_shot_id:
                dict['Preview'] = input.get('title')

            if input.get('number') == live_shot_id:
                dict['Live'] = input.get('title')

        # Get Overlay and Shot Name
        for overlay in tree.iter('overlay'):
            if overlay.text:
                for input in tree.iter('input'):
                    if input.get('number') == overlay.text:
                        overlayShots[int(overlay.get('number')) - 1] = input.get('title')
            else:
                if overlay.get('number'):
                    overlayShots[int(overlay.get('number')) - 1] = 0

        # vMix is Open/Active
        if dict['isActive'] != True:
            dict['isActive'] = True
            print("VMix: OPEN")

        return True

    except:
        # vMix is Closed/Inactive
        if dict['isActive'] != False:
            dict['isActive'] = False
            print("VMix: CLOSED")

        return False


def handle_arguments():
    global Local_IP
    global Bind
    global Destination

    parser = argparse.ArgumentParser()
    parser.add_argument("-a", "--address", help="change address (IP or name)")
    parser.add_argument("-p", "--port", help="change port number", type=int)
    parser.add_argument("-b", "--bind", help="binds to internet connected network adapter", action="store_true")
    args = parser.parse_args()

    if args.address:                                    # Set custom IP or Name for destination
        Destination = args.address

    if args.bind:                                       # Get local IP and set connection binding to True
        Local_IP = get_local_ip()
        if Local_IP:
            print("Binding: " + Local_IP)
            Bind = True
        else:
            Bind = False
    else:
        print("Binding: Disabled")
        Bind = False


def get_local_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.connect(("8.8.8.8", 80))
    local_ip = s.getsockname()[0]                       # IP of NIC with internet connection
    s.close()

    return local_ip

# =======================================================
# Main
# =======================================================
if __name__ == "__main__":
    handle_arguments()

    print("Address: " + Destination + ':' + Port)
    print("Multicast: " + MCAST_GRP + ":" + str(MCAST_PORT))

    mcastSock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    mcastSock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, TTL)
    if Bind:
        mcastSock.bind((Local_IP, MCAST_PORT))          # May be needed for multiple NICs

    while True:
        get_vmix_data()                                 # Get latest VMix information

        for idx in range(0, len(TallyState)):           # Clear Tally State buffer to be rewritten
            TallyState[idx] = Clear

        if dict['isActive']:
            parse_tally_info(dict['Preview'], Preview)  # Parse preview shot name
            parse_tally_info(dict['Live'], Live)        # Parse live shot name

            # Parse overlay shot names
            for idx in range(0, len(overlayShots)):
                if overlayShots[idx]:
                    parse_tally_info(overlayShots[idx], Live)

        mcastSock.sendto(bytearray(TallyState), (MCAST_GRP, MCAST_PORT))  # Send multicast packet to tally lights

        time.sleep(TallySendInterval)
