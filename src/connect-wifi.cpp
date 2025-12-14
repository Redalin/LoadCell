// Include the Wifi Headers
#include "connect-wifi.h"
#include "config.h"


// Wifi network credentials
const char* KNOWN_SSID[] = {"DRW", "ChrisnAimee.com"};
const char* KNOWN_PASSWORD[] = {"wellington", "carbondell"};
const int   KNOWN_SSID_COUNT = sizeof(KNOWN_SSID) / sizeof(KNOWN_SSID[0]); // number of known networks

int visibleNetworks = 0;

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
    // displayText("mDNS responder started");
}

void initWifi()
{
  WiFi.setHostname(HOSTNAME);
  // Scan for known wifi Networks
  int networks = scanForWifi();
  String wifiMessage;
  if (networks > 0)
  {
    String wifiName = connectToWifi();
    wifiMessage = "Connected to: " + wifiName;
    // displayText(wifiMessage);
  }
  else
  {
    wifiMessage = "No networks found";
    // displayText(wifiMessage);
    Serial.println(wifiMessage);
    createWifi();
    // displayText("Created Wifi: " + String(apName));
    // while (true);
    // We should not be here, no need to go further, hang in there, will auto launch the Soft WDT reset
  }
}

int scanForWifi() {
  Serial.println(F("WiFi scan start"));
  visibleNetworks = WiFi.scanNetworks();
  Serial.println(F("WiFi scan done"));
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
      if (strcmp(KNOWN_SSID[n], WiFi.SSID(i).c_str())) {
        debug(F("\tNot matching "));
        debugln(KNOWN_SSID[n]);
      } else { // we got a match
        wifiFound = true;
        break; // n is the network index we found
      }
    } // end for each known wifi SSID
    if (wifiFound) break; // break from the "for each visible network" loop
  } // end for each visible network

  if (wifiFound) 
  {
    Serial.print(F("\nConnecting to "));
    Serial.println(KNOWN_SSID[n]);
    // displayText("Connecting to " + String(KNOWN_SSID[n]));
    WiFi.begin(KNOWN_SSID[n], KNOWN_PASSWORD[n]);
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");
    Serial.print("IP address:\t");
    Serial.println(WiFi.localIP());
    return KNOWN_SSID[n];
  } 
  else 
  {
    String message = "No WiFi found, creating our own";
    Serial.println(message);
    // displayText(message);
    createWifi();
    return message;
  }

}

String createWifi() {
  WiFi.softAP(APNAME, APPASS);
  IPAddress IP = WiFi.softAPIP();
  String apMessage = "Created Wifi " + String(APNAME) + " with IP: " + IP.toString();
  Serial.println(apMessage);
  // displayText(apMessage);
  return "DRW.local";
}