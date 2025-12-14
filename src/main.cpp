#include "config.h"
#include <Arduino.h>
#include "connect-wifi.h"
#include "littlefs-conf.h"
#include "scale.h"
#include "webpage.h"

void setup()
{
  Serial.begin(115200);

  // initialise the LittleFS
  initLittleFS();

  // initialise Wifi as per the connect-wifi file
  initWifi();
  initMDNS();

  // initialise the scale
  initScale();

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
    float w = scaleGetUnits();
    if (!isnan(w)) {
      // Serial.print("Weight: ");
      // Serial.print(w);
      // Serial.println(" g");
    } else {
      Serial.println("HX711 not ready");
    }
  }
  delay(200);
}