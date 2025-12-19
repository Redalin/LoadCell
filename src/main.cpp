#include "config.h"
#include <Arduino.h>
#include "connect-wifi.h"
#include "littlefs-conf.h"
#include "scale.h"
#include "webpage.h"
#include "settings.h"
#include "display-oled.h"
#include "espnow.h"

String mainMessage = "Starting up...";

void setup()
{
  Serial.begin(115200);

  // initialise the LittleFS
  initLittleFS();

  // initialise the OLED display
  displaysetup();

  // Ok even the Child nodes need Wifi

    // Only parent need to initialise:
    // - Wifi and mDNS
    // - websocket
    // - web server
  if (ESPNOW_IS_PARENT) {
    initWifi();
    initMDNS();
    initwebservers();
  }

  // initialise ESP-NOW (after WiFi so channel is correct for peers)
  espnowInit();

  // initialise the scale
  initScale();

  // load persisted settings
  settingsInit();

}

void loop()
{
  // broadcast weight to connected web clients (parent only)
  if (ESPNOW_IS_PARENT) {
    webBroadcastLoop();
  } else {
    // Child node: read scale, buffer it, and send averaged data every 500ms
    float w = scaleGetUnits1();  // Read from primary scale
    if (!isnan(w)) {
      espnowBufferWeight(w);  // Buffer the reading
    }
    
    espnowSendAveragedWeightIfReady();  // Send average every 500ms if buffer has data
    
    // Check for pending tare commands
    uint8_t tareScale = espnowGetPendingTareCommand();
    if (tareScale > 0) {
      scaleTare(tareScale);  // Tare the specified scale
    }
  }

  // optional: print weight to Serial and display every 2 seconds
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= 2000) {
    lastPrint = millis();
    float w;
    if (ESPNOW_IS_PARENT) {
      // w = scaleGetDummyUnits();  // Parent shows dummy for demo
    } else {
      w = scaleGetUnits();  // Child shows its own scale
      mainMessage = String(w);
      Serial.println(mainMessage + "g");
      displayWeight(mainMessage); // print weight to OLED
    }
      w = scaleGetUnits();  // Child shows its own scale    
      mainMessage = String(w);
      Serial.println(mainMessage + "g");
      displayWeight(mainMessage); // print weight to OLED
    }
  }
  delay(200);
}