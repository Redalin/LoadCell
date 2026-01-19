// Include the Wifi Headers
#include "connect-wifi.h"
#include "config.h"
#include "display-oled.h"

int visibleNetworks = 0;
String goodSSID = "";
String wifiPass = "";

String wifiMessage;

void initMDNS() {
    // Initialize mDNS
    if (!MDNS.begin(HOSTNAME))
    { // Set the hostname
        Serial.println("Error setting up MDNS responder!");
        while (1)
        {
            delay(1000);
        }
    }
    Serial.println("mDNS responder started");
}

void initWifi() {
  WiFi.setHostname(HOSTNAME);
  // Scan for known wifi Networks
  // int networks = scanForWifi();
  if (scanForWifi() > 0 && checkValidSSID()) {

      connectToWifi(goodSSID);
  } else {
      // No known networks found, create access point
      createWifi();
  }
  // stop wifi from sleeping
  WiFi.setSleep(false);
}



int scanForWifi() {
  wifiMessage = "Scan for WiFi...";
  Serial.println(wifiMessage);
  displayText(wifiMessage, vbat);
  
  visibleNetworks = WiFi.scanNetworks();
  
  wifiMessage = "Wifi scan done - " + String(visibleNetworks) + " networks found";
  Serial.println(wifiMessage);
  displayText(wifiMessage, vbat);
  
  return visibleNetworks;
}

bool checkValidSSID() {
  int i, n;
  bool wifiFound = false;

  // ----------------------------------------------------------------
  // check if we recognize one by comparing the visible networks
  // one by one with our list of known networks
  // ----------------------------------------------------------------
  for (i = 0; i < visibleNetworks; ++i) {
    Serial.print(F("Checking: "));
    Serial.println(WiFi.SSID(i)); // Print current SSID

    for (n = 0; n < KNOWN_SSID_COUNT; n++) { // walk through the list of known SSID and check for a match
      if (strcmp(KNOWN_SSID[n], WiFi.SSID(i).c_str()) == 0) {
        wifiFound = true;
        Serial.print(F("Found known SSID: "));
        Serial.println(KNOWN_SSID[n]);
        goodSSID = String(KNOWN_SSID[n]);
        wifiPass = String(KNOWN_PASSWORD[n]);
        return wifiFound;
        // break out of the loop
      } else {
        debug(F("\tNot matching "));
        debugln(KNOWN_SSID[n]);
      }
    } // end for each known wifi SSID
    if (wifiFound) break; // break from the "for each visible network" loop
  } // end for each visible network
  return wifiFound;

}

void connectToWifi(String SSID) {

  WiFi.mode(WIFI_STA);
  wifiMessage = "Connecting to \n" + SSID;
  Serial.println(wifiMessage);
  displayText(wifiMessage, vbat);

    WiFi.begin(SSID, wifiPass);
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
    }
    wifiMessage = "Connected to \n" + SSID + "\nChannel: " + String(WiFi.channel());
    Serial.println(wifiMessage);
    displayText(wifiMessage, vbat);
    delay(1000);

    wifiMessage = "http:\\\\" + String(WiFi.getHostname()) + "\nIP: " + WiFi.localIP().toString();
    Serial.println(wifiMessage);
    displayText(wifiMessage, vbat);
    delay(1000);
    
}

String createWifi() {

  wifiMessage = "Creating Access Point\n" + String(APNAME);
  Serial.println(wifiMessage);
  displayText(wifiMessage, vbat);

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(APNAME, APPASS, 6, 0, 4); // channel 6, not hidden, max 4 connections
  IPAddress IP = WiFi.softAPIP();
  // String apMessage = "Wifi: " + String(APNAME) + "\nIP: " + IP.toString();
  String apMessage = "Wifi: " + String(APNAME) + "\nPass: " + String(APPASS) + "\nIP: " + IP.toString();
  Serial.println(apMessage);
  displayText(apMessage, vbat);
  return APNAME;
}