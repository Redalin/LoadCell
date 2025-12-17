#include "config.h"
#include <Arduino.h>
#include "connect-wifi.h"
#include "littlefs-conf.h"
#include "scale.h"
#include "webpage.h"
#include "settings.h"
#include "display-oled.h"

String mainMessage = "Starting up...";

void setup()
{
  Serial.begin(115200);

  // initialise the LittleFS
  initLittleFS();

  // initialise the OLED display
  displaysetup();

  // initialise Wifi as per the connect-wifi file
  initWifi();
  initMDNS();

  // initialise the scale
  initScale();

  // load persisted settings
  settingsInit();

  // initialise the websocket and web server
  initwebservers();
}

void loop()
{
  // broadcast weight to connected web clients
  webBroadcastLoop();

  // optional: print weight to Serial every 2 seconds
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= 2000) {
    lastPrint = millis();
    float w = scaleGetDummyUnits(); // added Dummy for testing without scale
    mainMessage = String(w);
    Serial.println(mainMessage + "g");
    displayWeight(mainMessage);
  }
  delay(200);
}