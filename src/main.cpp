#include "config.h"
#include <Arduino.h>
#include "connect-wifi.h"
#include "littlefs-conf.h"
#include "scale.h"
#include "webpage.h"
#include "settings.h"
#include "display-oled.h"
#include "espnow.h"
#include "battery.h"





String mainMessage = "Starting up...";
// Button state tracking for tare button
static int lastTareButtonState = HIGH; // set it high initially (not pressed)
static int batteryReadCounter = 0;  // Counter for battery reading
static int scaleReadCounter = 0;    // Counter for scale reading

float vbat = 0; // Initial definition of battery voltage

void setup()
{
  Serial.begin(115200);

  // initialise the LittleFS
  initLittleFS();

  // initialise the OLED display
  displaysetup();

  // configure tare button pin (use internal pullup so LOW means pressed)
  pinMode(TARE_BUTTON_PIN, INPUT_PULLUP);

  // configure VBAT ADC pin
  analogSetPinAttenuation(VBAT_PIN, ADC_0db);
  analogReadResolution(12);

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

  // get an initial vbat reading
  vbat = readVBAT();

}

void loop()
{
  // broadcast weight to connected web clients (parent only)
  if (ESPNOW_IS_PARENT) {
    webBroadcastLoop();
  } else {
    // Child node: read scale and send weight to parent every 500ms
    if (++scaleReadCounter >= 5) { // 5 * 100ms = 500ms
      scaleReadCounter = 0;
      float reading = scaleRead();  // Read from primary scale
      if (!isnan(reading)) {
        espnowSendWeight(reading);

        mainMessage = String(reading, 1);
        displayWeight(mainMessage, vbat); // print weight and battery to OLED
      }

      // Check for pending tare commands
      uint8_t tareCmd = espnowGetPendingTareCommand();
      if (tareCmd != 0) {
        debugln("Performing pending tare command");
        scaleTare();
        // optional: send ack back (not implemented)
      }
    }

    // Check tare button every loop
    int btnState = digitalRead(TARE_BUTTON_PIN);
    debug("Tare Button State: ");
    debug(btnState);
    debug(" Last State: ");
    debugln(lastTareButtonState);

    if (btnState == LOW && lastTareButtonState == HIGH) {
      debugln("Tare button is PRESSED - sending tare command");
      scaleTare(); //send tare command
    } else if (btnState == LOW && lastTareButtonState == LOW) {
      debugln("Tare button is STILL PRESSED");
    } else {
      debugln("Tare button is NOT pressed");
    }
    lastTareButtonState = btnState; // update button state
  }

  // Read battery voltage every 100 loops (~10 seconds)
  batteryReadCounter++;
  debug("Battery Read Counter: ");
  debugln(batteryReadCounter);
  if (batteryReadCounter >= 100) {
    vbat = readVBAT();
    batteryReadCounter = 0;
    debugln("Battery Voltage: " + String(vbat, 2) + "v");
  }

  delay(100);
}