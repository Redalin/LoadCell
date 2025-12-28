#include "config.h"
#include <Arduino.h>
#include "connect-wifi.h"
#include "littlefs-conf.h"
#include "scale.h"
#include "webpage.h"
#include "settings.h"
#include "display-oled.h"
#include "espnow.h"

// read VBAT helper (uses analogRead)
static float readVBAT() {
  // analogRead returns 0..4095 on ESP32 (12-bit)
  const int raw = analogRead(VBAT_PIN);
  const float measured = (raw / ADC_RESOLUTION) * REF_VOLTAGE; // measured at ADC pin
  const float actual = measured * VBAT_DIVIDER;
  
  Serial.print("VBAT raw: "); Serial.print(raw);
  Serial.print(" measured: "); Serial.print(measured, 2);
  Serial.println("v");
  return actual;
  
}

String mainMessage = "Starting up...";
// Button state tracking for tare button
static int lastTareButtonState = HIGH; // set it high initially (not pressed)

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
  analogSetPinAttenuation(VBAT_PIN, ADC_11db);
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

      mainMessage = String(reading, 1);
      float vbat = readVBAT();
      displayWeight(mainMessage, vbat); // print weight and battery to OLED
    }

    // Check tare button (rising edge with debounce)
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

    // Check for pending tare commands
    uint8_t tareCmd = espnowGetPendingTareCommand();
    if (tareCmd != 0) {
      debugln("Performing pending tare command");
      scaleTare();
      // optional: send ack back (not implemented)
    }
    // slow down loop for scale readings
    delay(500);
  }
  delay(500);
}