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
  } else {
    // We are a Child node so initialise the scale only
    initScale();
  }

  // initialise ESP-NOW (after WiFi so channel is correct for peers)
  espnowInit();

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
    float reading = scaleRead();  // Read from primary scale
    if (!isnan(reading)) {
      espnowSendWeight(reading);

      mainMessage = String(reading);
      displayWeight(mainMessage); // print weight to OLED
    }

    // espnowSendAveragedWeightIfReady();  // Send average every 500ms if buffer has data
    
    // Check for pending tare commands
    uint8_t tareCmd = espnowGetPendingTareCommand();
    if (tareCmd != 0) {
      Serial.println("Performing pending tare command");
      scaleTare();
      // optional: send ack back (not implemented)
    }
  }
  delay(200);
}