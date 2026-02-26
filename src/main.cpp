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
#include "pitbuttons.h"
#include <ElegantOTA.h>





String mainMessage = "Starting up...";
// Button state tracking for tare button
static int lastTareButtonState = HIGH; // set it high initially (not pressed)
static int tareButtonState = HIGH; // default state of the tare button

// timing for periodic tasks
static unsigned long lastBatteryReadTime = 0; // track last battery check

void setup()
{
  Serial.begin(115200);

  // initialise the LittleFS
  initLittleFS();

  // get an initial vbat reading
  vbat = readVBAT();

  // initialise the OLED display
  displaysetup();

  // configure tare button pin (use internal pullup so LOW means pressed)
  pinMode(TARE_BUTTON_PIN, INPUT_PULLUP);

  // configure VBAT ADC pin
  analogSetPinAttenuation(VBAT_PIN, ADC_0db);
  analogReadResolution(12);

  // Only parent need to initialise:
  // - Wifi and mDNS
  // - websocket
  // - web server
  if (ESPNOW_IS_PARENT) {
    initWifi();
    initMDNS();
    initwebservers();
    initpitbuttons();
    ElegantOTA.begin(&server);
    // Display hostname and IP on parent OLED
    displayDefaultParent(vbat);
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
  // Get the current time
  unsigned long currentTime = millis();
  
  
  if (ESPNOW_IS_PARENT) {
    // broadcast weight to connected web clients (parent only)
    webBroadcastLoop();

    // Update countdown timers every second
    for (int i = 0; i < NUM_LANES; i++) {
      if (buttonStates[i].countdown > 0 && (currentTime - countdownTimers[i]) >= 1000) {
        buttonStates[i].countdown--;
        countdownTimers[i] = currentTime;
        notifyButtonClients();
      }
    }
    // Check lane switches every 50 ms to debounce
    if (currentTime - lastCheckTime > 50) {
      lastCheckTime = currentTime;
      checkLaneSwitches();
    }
    ElegantOTA.loop();

    // Update parent display (revert to default after 10s if temp message shown)
    updateParentDisplay(vbat);

  } else {
    // Child node: read scale and send weight to parent every 500ms
    if (currentTime - lastCheckTime > 500) { 
      lastCheckTime = currentTime;

      // Check for pending remote tare commands
      uint8_t tareCmd = espnowGetPendingTareCommand();
      if (tareCmd != 0) {
        debugln("Performing pending tare command");
        scaleTare();
        // optional: send ack back (not implemented)
      }

      float reading = scaleRead();  // Read from scale
      if (!isnan(reading)) {
        espnowSendWeight(reading);

        mainMessage = String(reading, 1);
        displayWeight(mainMessage, vbat); // print weight and battery to OLED
      }
    }
  }

  // All nodes
  // Check tare button every loop
  if(checkTareButton()) {
    debugln("Tare button is currently pressed");
    if (!ESPNOW_IS_PARENT) {
      scaleTare(); // send tare command to local node
      debugln("Tare performed locally on Child node");
    } else {
      // Parent node - broadcast tare command to all 4 child nodes
      String tareMessage = "Taring all nodes...";
      displayTextTemporary(tareMessage, vbat);
      Serial.println(tareMessage);
           
      for (uint8_t nodeId = 1; nodeId <= 4; nodeId++) {
        debugln("Sending tare command to node " + String(nodeId));
        espnowSendTare(nodeId);
      }
    }
  }

  // Read battery voltage every 10 seconds using millis()
  if (currentTime - lastBatteryReadTime >= 10000) {
    lastBatteryReadTime = currentTime;
    vbat = readVBAT();
    debugln("Battery Voltage: " + String(vbat, 2) + "v");
  }

  // no delay() here so loop can run as fast as necessary
}