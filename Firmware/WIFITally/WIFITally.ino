/* 
ESP8266 WIFI Tally 

Copyright 2017 Benjamin Yaroch
Inspired by: https://github.com/henne-/wifitally

This software is free: you can redistribute it and/or modify 
it under the terms of the GNU General Public License as published by the 
Free Software Foundation, either version 3 of the License, or (at your 
option) any later version.

The software is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
or FITNESS FOR A PARTICULAR PURPOSE. 
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along 
with this software. If not, see: http://www.gnu.org/licenses/.

IMPORTANT: If you want to use this software in your own projects and/or products,
please follow the license rules!
*/

#include <ESP8266WiFi.h>          // ESP8266 Core WiFi Library
#include <WiFiClient.h>           // WIFI Client Library
#include <WiFiUdp.h>              // Multicast Library
#include <ESP8266WebServer.h>     // WebServer Library (used to serve the configuration portal)
#include <EEPROM.h>               // EEProm Library for saving/reading configuration settings
#include <ESP8266mDNS.h>          // DNS Library used in conjuction with Webserver
#include "FS.h"                   // File System Access Library
#include "StreamString.h"         // Used by HTTP OTA updates

const char* DefaultSsid         = "WIFITally";  
const char* DefaultPassword     = "wififtally1234";
const char* DefaultvMixHostName = "vMixHostName";

ESP8266WebServer httpServer(80);
WiFiUDP udp; 
IPAddress multicastAddress (224, 0, 0, 20);
WiFiClient client;

// Constants
  // -- GPIO --
const int OpLedGrn    = 13,         // Operator PREVIEW (Green) LED pin
          OpLedRed    = 14,         // Operator LIVE (Red) LED pin
          TallyRed    = 5,          // Tally LIVE (Red) LED pin          
          CnfgBtn     = 12;         // Config pin        
  // -- Other --
const int BufferSize    = 512,      // Multicast data buffer max length            
          WifiTimeout   = 30,       // Wait time for WIFI connect (seconds)
          SsidMaxLength = 32,       // Maximum size of a SSID
          PasswordMaxLength = 64,   // Length of passphrase. (Valid lengths are 8-63)
          DevicenameMaxLength = 32, // Maximum size of Device Name 
          vMixHostnameMaxLength = 63; // Maximum size of vMix Hostname  
            
// Variables
byte  lastTally       = -1,         // Stores state of last update
      tallyRedMax     = 100,        // Front LED max intensity Red (Default = 100%)
      operatorLedMax  = 100,        // Rear LED max intensity Red and Green (Default = 100%)
      operatorLedMin  = 0,          // Rear LED min intensity Red and Green (Default = 0%)
      id              = 0,          // Device ID 0-255 (Default = 0)  
      tallyProtocol   = 0;          // Receive Tally data via: 0 = Multicast, 1 = vMix TCP    
long  lastPacket      = 0,          // Time since last Multicast packet
      lastRetry       = 0,          // Time since last retry
      vMixRetry       = 5000,       // Retry interval for vMix
      dataTimeout     = 3000;       // Timeout for loss of UDP data   
bool  apEnabled       = false;      // User configuring device via AP

char  ssid[SsidMaxLength];
char  passphrase[PasswordMaxLength];    // Valid characters in a passphrase must be between ASCII 32-126 (decimal)
char  deviceName[DevicenameMaxLength];  // Device name, genearted dynamically based on MAC address
byte  packetBuffer[BufferSize];         // Multicast data buffer

String _updaterError  = "";             // Firmware update error string buffer

// vMix Specific Settings
int   vMixPort;                             // Configurable Port number (default = 8099)
char  vMixHostName[vMixHostnameMaxLength];  // Hostname of the PC running vMix

// ===== SETUP FUNCTIONS =====

// ----- EEProm Read/Write -----
void writeConfig() {     
  long ptr = 0;                                     // Clear EEProm Pointer; used to write consecutively  
 
  for (int i = 0; i < 512; i++)                     // Clear EEProm data (Write a 0 to all 512 bytes of the EEPROM)
    EEPROM.write(i, 0);
      
  for (int i = 0; i < SsidMaxLength; i++) {         // AP SSID 
    EEPROM.write(ptr, ssid[i]);
    ptr++;                                          // Increment EEProm Pointer
  }

  for (int i = 0; i < PasswordMaxLength; i++) {     // AP Password 
    EEPROM.write(ptr, passphrase[i]);
    ptr++;                                       
  }   
      
  EEPROM.write(ptr, id);                            // Tally ID
  ptr++;                                        
        
  EEPROM.write(ptr, tallyRedMax);                   // Talent LED Intensity
  ptr++;                                          
  
  EEPROM.write(ptr, operatorLedMax);                // Operator LED Intensity
  ptr++;

  for (int i = 0; i < vMixHostnameMaxLength; i++) { // vMix Hostname 
    EEPROM.write(ptr, vMixHostName[i]);
    ptr++;                                       
  }

  EEPROM.write(ptr, highByte(vMixPort));             // vMix Port 
  ptr++;
  EEPROM.write(ptr, lowByte(vMixPort ));
  ptr++;  

  EEPROM.write(ptr, tallyProtocol);                 // Tally Protocol
  ptr++;
     
  // Calculate EEProm CRC16
  long checksum = eeprom_crc(ptr);                        
  EEPROM.write(ptr, (checksum & 0xFF));             // Write long (4 bytes) into the eeprom memory.
  EEPROM.write(ptr + 1, ((checksum >> 8) & 0xFF));
  EEPROM.write(ptr + 2, ((checksum >> 16) & 0xFF));
  EEPROM.write(ptr + 3, ((checksum >> 24) & 0xFF));

  EEPROM.commit();                                  // Write EEProm data
}

void readConfig() {
  long ptr = 0;                                     // Clear EEProm Pointer; used to read consecutively 
  
  for (int i = ptr; i < SsidMaxLength; i++) {       // AP SSID read
    ssid[i] = EEPROM.read(ptr);
    ptr++;                                          // Increment EEProm Pointer
  }
  
  for (int i = 0; i <  PasswordMaxLength; i++) {    // AP Password
    passphrase[i] = EEPROM.read(ptr);
    ptr++;                                                  
  }
  
  id = EEPROM.read(ptr);                            // Tally ID
  ptr++;                                                   
  
  tallyRedMax = EEPROM.read(ptr);                   // Talent LED Intensity
  ptr++;                                                    
 
  operatorLedMax = EEPROM.read(ptr);                // Operator LED Intensity
  ptr++;  

  for (int i = 0; i <  vMixHostnameMaxLength; i++) {  // vMix Hostname 
    vMixHostName[i] = EEPROM.read(ptr);
    ptr++;                                                  
  }

  vMixPort = word(EEPROM.read(ptr++), EEPROM.read(ptr++));  // vMix Port

  tallyProtocol = EEPROM.read(ptr);                   // Tally Protocol
  ptr++;
  
  // Calculate EEProm CRC16
  long checksum = eeprom_crc(ptr);                                              
  long calc_chksum = (EEPROM.read(ptr) << 0) & 0xFF;      // Read 4 bytes (Long) from the eeprom memory and recomposed long by using bitshifting
  calc_chksum = calc_chksum + (EEPROM.read(ptr + 1) << 8) & 0xFFFF; 
  calc_chksum = calc_chksum + (EEPROM.read(ptr + 2) << 16) & 0xFFFFFF; 
  calc_chksum = calc_chksum + (EEPROM.read(ptr + 3) << 24) & 0xFFFFFFFF;

  if (checksum != calc_chksum) {                          // If checksums don't match EEProm data is corrupt
      Serial.println("EEProm Corrupt, Loading Defaults");
      defaultConfig();                                    // Rewrite defaults to EEprom
  }      
}

unsigned long eeprom_crc(int len) {                       // Calculate EEPROM CRC32 
  const unsigned long crc_table[16] = {
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
  };
  unsigned long crc = ~0L;
  for (int index = 0 ; index < len  ; ++index) {
    crc = crc_table[(crc ^ EEPROM.read(index)) & 0x0f] ^ (crc >> 4);
    crc = crc_table[(crc ^ (EEPROM.read(index) >> 4)) & 0x0f] ^ (crc >> 4);
    crc = ~crc;
  }
  return crc;
}

void defaultConfig() {   
  generateDevicename();
  
  id = 0;
  tallyRedMax = 100;
  operatorLedMax = 100; 
  vMixPort = 8099;
  tallyProtocol = 0;
  
  for (int i = 0;  i< SsidMaxLength; i++){
    ssid[i] = DefaultSsid[i];
  }  
    
  for (int i = 0;  i< PasswordMaxLength; i++){
    passphrase[i] = DefaultPassword[i];
  }  

  for (int i = 0;  i< vMixHostnameMaxLength; i++){
    vMixHostName[i] = DefaultvMixHostName[i];
  }  

  writeConfig(); 
}

void generateDevicename() {     
  sprintf(deviceName, "WIFITally_%06X", ESP.getChipId());     // Create device name based on ESP ID "WIFITally_xxxxxx" 
                                                              // NOTE: Last four digits are last four digits of MAC address
}

// ===== Network Comms & LED Control =====

// ----- Read Multicast Packet -----
void readMulticastData(int pcktSize) {
  if (pcktSize < 1) {                   // Packet has no data
    return; 
  }  
  else if(pcktSize > BufferSize) {      // Packet bigger than buffer, only read to buffer max size to avoid overflow
    pcktSize = BufferSize; 
  }
  
  udp.read(packetBuffer, pcktSize);     // Not too big or small, read packet.
  lastPacket = millis();                // Remember current (relative) time in msecs.

  char mine = packetBuffer[id];
  if (mine != lastTally) { 
    if (mine == 1) preview();           // In Preview
    else if (mine == 2) live();         // In Live
    else off();                         // NOT in Preview or Live        
  }  
}

// ----- Multicast Data Timeout -----
long noDataSince() {                    // Calculate the amount of time since last data was received
  long now = millis();
  return (now - lastPacket);
}

// ----- Read vMix Tally Packet -----
void readvMixData(String data)
{
  // Check if server data is tally data
  if (data.indexOf("TALLY") == 0) {
    int tallyState = (data.charAt((data.indexOf("OK") + 3) + id) - 48);  // Convert target tally state value (by device ID) to int
    
    if (tallyState == 1) live();          // In Live
    else if (tallyState == 2) preview();  // In Preview
    else off();                           // NOT in Preview or Live       
  } else {
    Serial.print("vMix Response: ");
    Serial.println(data);
  }
}

// ----- LED Control -----
int dimCurve(int val) {                 // LED Dimmer Curve (makes 25% look visually like 25%)
  int R = (100 * log10(2))/(log10(PWMRANGE));
  int brightness = pow (2, (val / R)) - 1;

  return brightness;
}

void preview() {                        // Preview LED state change
  if (lastTally != 1) {
    Serial.println("Tally: Preview");    
  }
   lastTally = 1;                                    // Update to current tally state (preview)   
  analogWrite(OpLedGrn, dimCurve(operatorLedMax));   // Turn on Green Operator LED
  analogWrite(OpLedRed, dimCurve(operatorLedMin));   // Turn off Red Operator LED
  analogWrite(TallyRed, dimCurve(0));                // Turn off Live LEDs 
}

void live() {                           // Live LED state change
  if (lastTally != 2) {
    Serial.println("Tally: Live");  
  }
   lastTally = 2;                                    // Update to current state (live)
  analogWrite(OpLedGrn, dimCurve(operatorLedMin));   // Turn off Green Operator LED
  analogWrite(OpLedRed, dimCurve(operatorLedMax));   // Turn on Red Operator LED
  analogWrite(TallyRed, dimCurve(tallyRedMax));      // Turn on Live LEDs 
}

void off() {                            // Off LED state change
  if (lastTally != 0) {
    Serial.println("Tally: Off");  
  }
   lastTally = 0;                                    // Update to current state (off)   
  analogWrite(OpLedGrn, dimCurve(operatorLedMin));   // Turn off Green Operator LED
  analogWrite(OpLedRed, dimCurve(operatorLedMin));   // Turn off Red Operator LED
  analogWrite(TallyRed, dimCurve(0));                // Turn off Live LEDs 
}

// ----- Device ID -----
void deviceID() {
  int _id = id + 1;
  
  Serial.println();
  Serial.print("Device ID: ");         
  Serial.println(_id);                              // Print current Device ID 

  char digits[3];
  digits[2] = (_id%10);                             // Ones
  digits[1] = ((_id/10)%10);                        // Tens
  digits[0] = ((_id/100)%10);                       // Hundreds

  for (int i=0; i<=2; i++) {
    analogWrite(OpLedGrn, dimCurve(50));
    delay(1500); 
    analogWrite(OpLedGrn, dimCurve(0));
    delay(250);
          
    if (digits[i] > 0) {        // 1 - 9
      for (int c=1; c <= digits[i]; c++) {  
        analogWrite(OpLedGrn, dimCurve(100));
        delay(250);
        analogWrite(OpLedGrn, dimCurve(0));   
        delay(150); 
      }       
    }
    else {                      // Zero
      analogWrite(OpLedGrn, dimCurve(0));
      delay(1000);   
    }
  }
  delay(1500);
}

// ----- vMix Connect -----
void vMixConnect() {    
  Serial.print("Connecting to vMix host: ");
  Serial.println(vMixHostName);
  
  if (client.connect(vMixHostName, vMixPort))
  {
    Serial.println("Connected!");

    // Subscribe to the tally events
    client.print("SUBSCRIBE TALLY\r\n");
    Serial.println("Subscribe to Tally");
  }
  else
  {
    lastRetry = millis();                             // Remember current (relative) time in msecs.
    Serial.println("FAILED!");
  }
}

// ----- vMix Reconnect Retry -----
long sinceRetry() {                                 // Calculate the amount of time since last data was received
  long now = millis();
  return (now - lastRetry);
}

// ----- WIFI Client Connect -----
void wifiConnect() {
  int timeout = WifiTimeout;                        // Set WIFI connect retried

  Serial.println();
  Serial.println("WIFI Connect");
  Serial.println("------------");
  Serial.print("SSID: ");                            // Display SSID, Password, and Hostname
  Serial.println(ssid);
  Serial.print("Passphrase: "); 
  Serial.println(passphrase); 
  Serial.print("Hostname: "); 
  Serial.println(deviceName);  
  
  WiFi.mode(WIFI_STA); 
  WiFi.hostname(deviceName);                          // Set device hostname (Don't move this, doesn't work elsewhere)                                
  WiFi.begin(ssid, passphrase);                       // Try connecting to WIFI network as client 
  
  Serial.print("Waiting for connection.");            // Wait for connection
  while (WiFi.status() != WL_CONNECTED and timeout > 0) {  
    analogWrite(OpLedGrn, dimCurve(100));             // Blink Operator LEDs while waiting
    analogWrite(OpLedRed, dimCurve(100));
    delay(500);
    analogWrite(OpLedGrn, dimCurve(00));
    analogWrite(OpLedRed, dimCurve(0));
    delay(500);             
    timeout--;                                        // Decrease timeout counter
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {                // Connected, Success!
    Serial.println("Success!"); 
    Serial.print("IP address: ");                     // Print out IP address
    Serial.println(WiFi.localIP());
  } else {                                            // Didn't connect, print out failure message (might be good to blink out error?)
    if (WiFi.status() == WL_IDLE_STATUS) Serial.println("Idle");    
    else if(WiFi.status() == WL_NO_SSID_AVAIL) Serial.println("SSID Unavailable"); 
    else if(WiFi.status() == WL_SCAN_COMPLETED) Serial.println("Scan Completed"); 
    else if(WiFi.status() == WL_CONNECT_FAILED) Serial.println("Connection Failed"); 
    else if(WiFi.status() == WL_CONNECTION_LOST) Serial.println("Connection Lost"); 
    else if(WiFi.status() == WL_DISCONNECTED) Serial.println("Disconnected"); 
    else Serial.println("Unknown Failure"); 
  }

  apEnabled = false;
}

// ----- AP Start up -----
void apStart() {   
  // Makes the ESP8266 an access point and provides access to the configuration webpage
  // Just connect to SSID that matches the device name then go to http://192.168.4.1
  Serial.println();
  Serial.println("AP Start");
  Serial.println("--------");

  Serial.print("AP SSID: ");
  Serial.println(deviceName);    

  WiFi.mode(WIFI_AP); 
  WiFi.softAP(deviceName);
  delay(100);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("IP address: ");
  Serial.println(myIP); 
   
  apEnabled = true;   
}

// ======================= WEBPAGE HANDLING =======================
// Root webpage (aka Settings/Home)
void rootPageHandler() { 
  String response_message = "";

  response_message += "<form action='/save' method='post' enctype='multipart/form-data' data-ajax='false'>";
  response_message += "<p>SSID:<br>";    
  response_message += "<input type='text' size='32' maxlength='32' name='ssid' value='" + String(ssid) + "'></p>";      
  response_message += "<p>Passphrase (if required):<br>";
  response_message += "<input type='password' size='32' maxlength='64' name='password' value='" + String(passphrase) + "'></p>";
  response_message += "<p>Device ID (1-256):<br>";
  response_message += "<input type='text' size='3' maxlength='3' name='id' value='" + String(id + 1) + "'></p>";
  response_message += "<p>Tally Intensity (0-100%):<br>";
  response_message += "<input type='text' size='3' maxlength='3' name='frontLED' value='" + String(tallyRedMax) + "'></p>";  
  response_message += "<p>Operator Intensity (0-100%):<br>";
  response_message += "<input type='text' size='3' maxlength='3' name='rearLED' value='" + String(operatorLedMax) + "'></p>";   

  response_message += "<fieldset data-role='controlgroup' />";
  response_message += "<legend>Protocol:</legend>";
  response_message += "<input type='radio' id='radio0' name='protocol-radio-choice-0' value='protocol-choice-0'/>";
  response_message += "<label for='radio0'>Multicast</label>";
  response_message += "<input type='radio' id='radio1' name='protocol-radio-choice-0' value='protocol-choice-1'/>";
  response_message += "<label for='radio1'>vMix TCP</label>";
  response_message += "</fieldset></p>";
  
  response_message += "<p>vMix Hostname:<br>";
  response_message += "<input type='text' size='63' maxlength='63' name='vMixHostName' value='" + String(vMixHostName) + "'></p>";
  response_message += "<p>vMix Port:<br>";
  response_message += "<input type='text' size='4' maxlength='4' name='vMixPort' value='" + String(vMixPort) + "'></p>";
   
  response_message += "<input type='submit' value='SAVE' class='btn'></form>"; 
  
  sendWebpage(response_message, "Settings");
}

// Settings POST handler 
void handleSave() {
  bool restart = false;
  
  httpServer.sendHeader("Location", String("/"), true);
  httpServer.send (302, "text/plain", "Redirected to: /");

  // Check if there are any arugments
  if (httpServer.hasArg("ssid")) {   
    if (httpServer.arg("ssid").length() <= SsidMaxLength) {
      if (httpServer.arg("ssid") != ssid) {
        httpServer.arg("ssid").toCharArray(ssid, SsidMaxLength);  
        restart = true;
      }        
    }      
    if (httpServer.hasArg("password")) { 
      if (httpServer.arg("password").length() <= PasswordMaxLength) {
        if (httpServer.arg("password") != passphrase) {
          httpServer.arg("password").toCharArray(passphrase, PasswordMaxLength);
          restart = true;  
        }             
      }
    }
    
    if (httpServer.arg("id").toInt() > 0 and httpServer.arg("id").toInt() <= 256) {
      id = (httpServer.arg("id").toInt() - 1);        
    }     

    if (httpServer.arg("frontLED").toInt() <= 100) {
      tallyRedMax = httpServer.arg("frontLED").toInt(); 
    }  

    if (httpServer.arg("rearLED").toInt() <= 100) {
      operatorLedMax = httpServer.arg("rearLED").toInt();  
    }  

    if (httpServer.arg("protocol-radio-choice-0")) {
      if (httpServer.arg("protocol-radio-choice-0") == "protocol-choice-0") {        
        if (tallyProtocol != 0) {
          tallyProtocol = 0;
          restart = true; 
        }
      }
      if (httpServer.arg("protocol-radio-choice-0") == "protocol-choice-1") {
          if (tallyProtocol != 1) {
            tallyProtocol = 1;
            restart = true; 
          }   
      }
    }    

    if (httpServer.hasArg("vMixHostName")) { 
      if (httpServer.arg("vMixHostName").length() <= vMixHostnameMaxLength) {
        if (httpServer.arg("vMixHostName") != vMixHostName) {
          httpServer.arg("vMixHostName").toCharArray(vMixHostName, vMixHostnameMaxLength);
          restart = true; 
        }             
      }
    }    

    if (httpServer.hasArg("vMixPort")){
      if (httpServer.arg("vMixPort").toInt() != vMixPort) {
        vMixPort = httpServer.arg("vMixPort").toInt(); 
        restart = true; 
      } 
    }  
    
    lastTally = -1;    
    writeConfig();  
    
    if (restart == true) {                        // Set when WIFI settings are changed (SSID and/or Password)
      ESP.restart();                              // Reset device       
    }
  }
}

// Firmware Upgrade webpage
void updatePageHandler() {     
  String response_message = "";  
  response_message += "Please select a file to upgrade";
  response_message += "<form action='/update' method='post' enctype='multipart/form-data' data-ajax='false'>";
  response_message += "<input type='file' name='update'>";
  response_message += "<input type='submit' value='UPDATE' class='btn'></form>";
 
  sendWebpage(response_message, "Firmware Upgrade");             
}

// Firmware Upgrade POST handler
void updatePostHandler() { 
  String response_message = ""; 
  response_message += "<html>";
  response_message += "<head>";
  response_message += "<title>" + String(deviceName) + "</title>";   
  response_message += "<meta name='viewport' content='device-width, initial-scale=1'>";  
  response_message += "<meta http-equiv='Content-Type' content='text/html; charset=utf-8'>"; 
  if (!Update.hasError()) {
    response_message += "<META http-equiv='refresh' content='30;URL=/'>";
  }     
  response_message += "<link rel='icon' type='image/x-icon' href='favicon.ico'>";  
  response_message += "<link rel='stylesheet' href='jquery.mobile-1.4.5.min.css'>";   
  response_message += "<script src='jquery-1.12.4.min.js'></script>";  
  response_message += "<script src='jquery.mobile-1.4.5.min.js'></script>";   
   
  response_message += "<style>h3, h4 {text-align: center;}span {font-weight: bold;}</style>";
  response_message += "</head>";  
  
  response_message += "<body>";
  response_message += "<div data-role='page' data-title=" + String(deviceName) + " data-theme='b'>";  
  
  response_message += "<div data-role='header'>";  
  response_message += "<a href='/' target='_top' class='ui-btn ui-icon-home ui-btn-icon-left'>Home</a>"; 
  response_message += "<h1> Firmware Upgrade </h1>";
  response_message += "<a href='update' target='_top' class='ui-btn ui-icon-gear ui-btn-icon-left'>Upgrade</a>";
  response_message += "</div>";
  
  response_message += "<div data-role='content'>"; 
  if (Update.hasError()) {
    response_message += "<center><strong>" + String(_updaterError) + "</strong></center><br>";      
  } else {
    response_message += "<center><strong>Upgrade Successful! Rebooting...</strong></center>";
  }     
  response_message += "</div>"; 
  response_message += "</div>";
  response_message += "</body>";
  response_message += "</html>";

  httpServer.sendHeader("Connection", "close");
  httpServer.send(200, "text/html", String(response_message));           
  
  if(!Update.hasError()) {
    delay(100);
    httpServer.client().stop();
    ESP.restart();    
  } 
}

// Firmware Upgrade
void handleUpdate() {
  HTTPUpload& upload = httpServer.upload();
  
  if(upload.status == UPLOAD_FILE_START){
    _updaterError = String();
    WiFiUDP::stopAll();
    
    Serial.printf("Update: %s\n", upload.filename.c_str());    
    uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    if(!Update.begin(maxSketchSpace)){                          // Start with max available size
     _setUpdaterError();
    }
  } else if(upload.status == UPLOAD_FILE_WRITE){
    Serial.printf(".");
    if(Update.write(upload.buf, upload.currentSize) != upload.currentSize){      
      _setUpdaterError();
    }
  } else if(upload.status == UPLOAD_FILE_END){
    if(Update.end(true)){                                       // True to set the size to the current progress
      Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
    } else {
      _setUpdaterError();
    }
  }
  
  yield();
}

// Firmware Upgrade error 
void _setUpdaterError() {
  Update.printError(Serial);
  StreamString str;
  Update.printError(str);
  _updaterError = str.c_str();
}

// Webpage template
void sendWebpage(String body, String PageName) {
  String response_message = ""; 
  response_message += "<html>";
  response_message += "<head>";
  response_message += "<title>" + String(deviceName) + "</title>";   
  response_message += "<meta name='viewport' content='device-width, initial-scale=1'>";  
  response_message += "<meta http-equiv='Content-Type' content='text/html; charset=utf-8'>"; 
  response_message += "<link rel='icon' type='image/x-icon' href='favicon.ico'>";  
  response_message += "<link rel='stylesheet' href='jquery.mobile-1.4.5.min.css'>";   
  response_message += "<script src='jquery-1.12.4.min.js'></script>";  
  response_message += "<script src='jquery.mobile-1.4.5.min.js'></script>";    

  response_message += "<script type='text/javascript'>";
  response_message += "$(document).ready(function(){";
  response_message += "$('input:radio[name=protocol-radio-choice-0]:nth(" + String(tallyProtocol) + ")').attr('checked',true).checkboxradio('refresh');});";
  response_message += "</script>";

  response_message += "<style>h3, h4 {text-align: center;}span {font-weight: bold;}</style>";
  response_message += "</head>";  
  
  response_message += "<body>";
  response_message += "<div data-role='page' data-title=" + String(deviceName) + " data-theme='b'>";  
  
  response_message += "<div data-role='header'>";  
  response_message += "<a href='/' target='_top' class='ui-btn ui-icon-home ui-btn-icon-left'>Home</a>"; 
  response_message += "<h1>" + PageName + "</h1>";
  response_message += "<a href='update' target='_top' class='ui-btn ui-icon-gear ui-btn-icon-left'>Upgrade</a>";
  response_message += "</div>";
  
  response_message += "<div data-role='content'>";
  response_message += body;
  response_message += "</div>"; 
  
  response_message += "<div data-role='footer' style='text-align:center;'>";
  char ip[13];  
  sprintf(ip, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);  
  response_message += "<p>IP: " + String(ip) + "</p>"; 
  sprintf(ip, "%d.%d.%d.%d", WiFi.subnetMask()[0], WiFi.subnetMask()[1], WiFi.subnetMask()[2], WiFi.subnetMask()[3]);  
  response_message += "<p>Subnet Mask: " + String(ip) + "</p>";   
  sprintf(ip, "%d.%d.%d.%d", WiFi.gatewayIP()[0], WiFi.gatewayIP()[1], WiFi.gatewayIP()[2], WiFi.gatewayIP()[3]);  
  response_message += "<p>Gateway: " + String(ip) + "</p>";   
  response_message += "<p>MAC: " + String(WiFi.macAddress()) + "</p>";    
  response_message += "<p>Signal Strength: " + String(WiFi.RSSI()) + " dBm </p>";  
  response_message += "<p>Device Name: " + String(deviceName) + "</p>"; 
  
  if (WiFi.status() == WL_CONNECTED) { 
    response_message += "<p>Status: Connected</p>"; 
  }
  else {
    response_message += "<p>Status: Disconnected</p>";
  }        
  
  if (apEnabled == true) { 
    sprintf(ip, "%d.%d.%d.%d", WiFi.softAPIP()[0], WiFi.softAPIP()[1], WiFi.softAPIP()[2], WiFi.softAPIP()[3]);  
    response_message += "<p>AP: Active (" + String(ip) + ")</p>";     
  }
  else {
    response_message += "<p>AP: Inactive</p>";
  }    
  response_message += "</div>";  
  
  response_message += "</div>";
  response_message += "</body>";
  response_message += "</html>";

  httpServer.sendHeader("Connection", "close");
  httpServer.send(200, "text/html", String(response_message));  
}

// SPIFFS file handling
bool loadFromSpiffs(String path){
  String dataType = "text/plain";
  
  if(path.endsWith("/")) path += "index.htm";
  if(path.endsWith(".src")) path = path.substring(0, path.lastIndexOf("."));
  else if(path.endsWith(".htm"))  dataType = "text/html";
  else if(path.endsWith(".css"))  dataType = "text/css";
  else if(path.endsWith(".js"))   dataType = "application/javascript";
  else if(path.endsWith(".png"))  dataType = "image/png";
  else if(path.endsWith(".gif"))  dataType = "image/gif";
  else if(path.endsWith(".jpg"))  dataType = "image/jpeg";
  else if(path.endsWith(".ico"))  dataType = "image/x-icon";
  else if(path.endsWith(".xml"))  dataType = "text/xml";
  else if(path.endsWith(".pdf"))  dataType = "application/pdf";
  else if(path.endsWith(".zip"))  dataType = "application/zip";

  if (SPIFFS.exists(path.c_str())) {
    File dataFile = SPIFFS.open(path.c_str(), "r"); 
    if (httpServer.hasArg("download")) dataType = "application/octet-stream";
    if (httpServer.streamFile(dataFile, dataType) != dataFile.size()) {
      }    
    dataFile.close();
    return true;
    } else {
      return false;
    }
}

// Page not found (aka 404)
void handleNotFound(){   
  if (loadFromSpiffs(httpServer.uri()) == false) {
    String message = "File Not Found\n\n";  
    message += "URI: ";
    message += httpServer.uri();
    message += "\nMethod: ";
    message += (httpServer.method() == HTTP_GET)?"GET":"POST";
    message += "\nArguments: ";
    message += httpServer.args();
    message += "\n\n";
     
    for (uint8_t i = 0; i < httpServer.args(); i++) {
      message += httpServer.argName(i) + ":" + httpServer.arg(i) + "\n";
    }
    httpServer.send(404, "text/plain", message);
    Serial.println(message);
  }
}

// ===== SETUP =====
void setup(void) {
  // Switches and LEDs  
  pinMode(CnfgBtn, INPUT_PULLUP);         // configuration pin as input with pull-up  
  pinMode(OpLedGrn, OUTPUT);              // LED output pins
  pinMode(OpLedRed, OUTPUT);  
  pinMode(TallyRed, OUTPUT);    
  analogWrite(OpLedGrn, dimCurve(0));     // Turn off LEDs
  analogWrite(OpLedRed, dimCurve(0));        
  analogWrite(TallyRed, dimCurve(0));

  // Serial Debug
  Serial.begin(115200);                   // Serial debug      
  Serial.println();
  
  // EEProm and File System (flash)
  EEPROM.begin(1024);
  SPIFFS.begin();

  // Configuration
  generateDevicename(); 
  readConfig();                           // Read configuration settings from EEProm    

  // Reset to Default Config
  
  // If button pressed > 10 secs the default config is written...
  //...and the device goes into AP mode for configuration. 
  if(digitalRead(CnfgBtn) == LOW) {
    unsigned long now = millis();
    while(digitalRead(CnfgBtn) == LOW) {
      unsigned long switchDuration = millis() - now;
      yield();
      if(switchDuration >= 10000) {        // if button pressed > 10 secs
        defaultConfig(); 
        apStart();  
        break;
      }
    }
  }     

  // WIFI Client Connect
  if (apEnabled == false) {
    deviceID();                           // Indicate current Device ID.
    WiFi.persistent(false);               // Turn off persistent to fix flash crashing issue.   
    WiFi.mode(WIFI_OFF);                  // https://github.com/esp8266/Arduino/issues/3100 
    wifiConnect();                        // Connect to AP as client   
  }

  // Tally Protocol
  if (tallyProtocol == 0) {               // Multicast
    udp.beginMulticast(WiFi.localIP(), multicastAddress, 3000); 
    Serial.println("Multicast: Started");  
  } else if (tallyProtocol == 1) {        // vMix TCP
    vMixConnect();
  }  

  // Webpage handler functions
  httpServer.on("/", HTTP_GET, rootPageHandler);
  httpServer.on("/save", HTTP_POST, handleSave);
  httpServer.on("/update", HTTP_GET, updatePageHandler);
  httpServer.on("/update", HTTP_POST, updatePostHandler, handleUpdate);
  httpServer.onNotFound(handleNotFound);
  httpServer.serveStatic("/", SPIFFS, "/", "max-age=315360000");  // serves all SPIFFS content with max-age cache control
  
  // Start TCP (HTTP) server
  MDNS.begin(deviceName);
  httpServer.begin();
  MDNS.addService("http", "tcp", 80);   
  Serial.printf("Webserver ready: http://%s \n", deviceName);
}

// ========= MAIN LOOP =========
void loop(void) {  
  httpServer.handleClient();                                    // Webserver processing

  // WIFI connectivity and data processing 
  if (WiFi.status() == WL_CONNECTED and apEnabled == false) {   // If connected to Access Point and not in AP mode...
    operatorLedMin = 10;                                        // ...glow operator LEDs.

    // == Multicast ==
    if (tallyProtocol == 0) {
      int pcktSize = udp.parsePacket();                         // Get multicast packet size
      if(pcktSize) readMulticastData(pcktSize);                 // If more than one byte received, process multicast packet.   

      // Loss of multicast data timeout
      if (noDataSince() > dataTimeout) off();                   // Turn off Tally LEDs if data is lost.               
    }

    // == vMix ==
    if (tallyProtocol == 1){
      if (client.connected()) {
        if (client.available()) {
          String data = client.readStringUntil('\n');
          readvMixData(data);
        } 
      } else {
        off();                                                  // Turn off Tally LEDs if connection is lost...  
        if (sinceRetry()> vMixRetry) vMixConnect();             // ...try to reconnect.
      }
    }
     
  } else if (apEnabled == true) {                               // In AP Mode...
    operatorLedMin = 100;                                       // ...operator LEDs on @ Full to indicate AP Mode   
    off();  
     
  } else {                                                      // Loss of WIFI connection...
    operatorLedMin = 0;                                         // ...operator and tally LEDs off.           
    off();                             
  }      
                                   
  // Switch to AP mode  
  if (digitalRead(CnfgBtn) == LOW and apEnabled == false) {     // If button is pressed and not already in AP Mode
    unsigned long now = millis();
    while(digitalRead(CnfgBtn) == LOW) {
      unsigned long switchDuration = millis() - now;
      yield();
      if (switchDuration >= 1000) {       
        apStart();                      
        break;                          
      }
    }
  }  
}
