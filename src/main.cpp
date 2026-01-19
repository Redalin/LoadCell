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





String mainMessage = "Starting up...";
// Button state tracking for tare button
static int lastTareButtonState = HIGH; // set it high initially (not pressed)
static int batteryReadCounter = 0;  // Counter for battery reading
static int scaleReadCounter = 0;    // Counter for scale reading
static int tareButtonState = HIGH; // default state of the tare button

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
  tareButtonState = digitalRead(TARE_BUTTON_PIN);
  debug("Tare Button State: ");
  debugln(tareButtonState);
  debug(" Last State: ");
  debugln(lastTareButtonState);

  if (tareButtonState == LOW && lastTareButtonState == HIGH) {
    debugln("Tare button is PRESSED - sending tare command");
    if (!ESPNOW_IS_PARENT) {
      scaleTare(); // send tare command
      debugln("Tare performed locally on Child node");
    } else {
      // Parent node - broadcast tare command to all child nodes, max of 4
      String tareMessage = "Taring all nodes...";
      displayText(tareMessage, vbat);
      Serial.println(tareMessage);
           
      for (uint8_t nodeId = 1; nodeId <= 4; nodeId++) {
        debugln("Sending tare command to node " + String(nodeId));
        espnowSendTare(nodeId);
      }
    }
  } else if (tareButtonState == LOW && lastTareButtonState == LOW) {
    debugln("Tare button is STILL PRESSED");
  } else{
    // debugln("Tare button is NOT pressed");
  }
  lastTareButtonState = tareButtonState; // update button state

  // Read battery voltage every 100 loops (~10 seconds)
  batteryReadCounter++;
  debug("Battery Read Counter: ");
  debugln(batteryReadCounter);
  if (batteryReadCounter >= 100) {
    vbat = readVBAT();
    batteryReadCounter = 0;
    debugln("Battery Voltage: " + String(vbat, 2) + "v");
    mainMessage = "http:\\\\" + String(WiFi.getHostname()) + "\nIP: " + WiFi.localIP().toString();
    displayText(mainMessage, vbat); // update display with new voltage
  }

  delay(100);
}