// Include the Wifi Headers
#include "connect-wifi.h"
#include "config.h"
#include "display-oled.h"

int visibleNetworks = 0;

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
    displayText("mDNS responder started");
}

void initWifi()
{
  WiFi.setHostname(HOSTNAME);
  // Scan for known wifi Networks
  int networks = scanForWifi();
  if (networks > 0)
  {
    String wifiName = connectToWifi();
    wifiMessage = "Connected to: \n" + wifiName;
    displayText(wifiMessage);
  }
  else
  {
    wifiMessage = "No networks found";
    displayText(wifiMessage);
    Serial.println(wifiMessage);
    createWifi();
    displayText("Created Wifi: " + String(APNAME));
    // while (true);
    // We should not be here, no need to go further, hang in there, will auto launch the Soft WDT reset
  }
}

int scanForWifi() {
  wifiMessage = "Scan for WiFi...";
  Serial.println(wifiMessage);
  displayText(wifiMessage);
  
  visibleNetworks = WiFi.scanNetworks();
  
  wifiMessage = "Wifi scan done";
  displayText(wifiMessage);
  Serial.println(wifiMessage);
  
  return visibleNetworks;
}

String connectToWifi() {
  int i, n;
  bool wifiFound = false;
  Serial.println(F("Found the following networks:"));
  for (i = 0; i < visibleNetworks; ++i) {
    Serial.println(WiFi.SSID(i));
  }
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
        break; // n is the network index we found
      } else {
        debug(F("\tNot matching "));
        debugln(KNOWN_SSID[n]);
      }
    } // end for each known wifi SSID
    if (wifiFound) break; // break from the "for each visible network" loop
  } // end for each visible network

  if (wifiFound) 
  {
    wifiMessage = "Connecting to \n" + String(KNOWN_SSID[n]);
    Serial.println(wifiMessage);
    displayText(wifiMessage);

    WiFi.begin(KNOWN_SSID[n], KNOWN_PASSWORD[n]);
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
    }
    wifiMessage = "Connected to \n" + String(KNOWN_SSID[n]);
    Serial.println(wifiMessage);
    displayText(wifiMessage);
    delay(2000);

    wifiMessage = "http:\\" + String(WiFi.getHostname()) + "\nIP: " + WiFi.localIP().toString();
    Serial.println(wifiMessage);
    displayText(wifiMessage);
    delay(2000);

    return KNOWN_SSID[n];
  } 
  else 
  {
    wifiMessage = "No WiFi, setup localAP";
    Serial.println(wifiMessage);
    displayText(wifiMessage);
    createWifi();
    return wifiMessage;
  }

}

String createWifi() {
  WiFi.softAP(APNAME, APPASS);
  IPAddress IP = WiFi.softAPIP();
  String apMessage = "Created Wifi " + String(APNAME) + " with IP: " + IP.toString();
  Serial.println(apMessage);
  displayText(apMessage);
  return "DRW.local";
}