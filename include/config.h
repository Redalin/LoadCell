// config.h - global configuration and runtime variables
#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <Arduino.h>  // for Arduino types
#include "secrets.h"

// Firmware version reported by child nodes to parent (sent with weight payload)
#define FIRMWARE_VERSION "1.3.5"

// Global battery voltage variable
extern float vbat;

// Runtime configuration (can be overridden by Preferences)
extern uint8_t deviceId;           // 0 = parent, 1-4 = child
extern bool espnowIsParent;        // derived from deviceId
extern const char* hostName;       // hostname to use for WiFi / display
extern float calibrationFactor;    // scale calibration value
extern uint8_t tareButtonPin;      // pin number used for tare button

#define DEBUG 0

#if DEBUG
    #define debug(message) Serial.print(message)
    #define debugln(message) Serial.println(message)
#else
    #define debug(message) 
    #define debugln(message)
#endif

// Known WiFi networks and passwords are now in secrets.h
static constexpr int KNOWN_SSID_COUNT = sizeof(KNOWN_SSID) / sizeof(KNOWN_SSID[0]);

// Access Point credentials (if no known WiFi found)
// update these in secrets.h


// Load cell pins and calibration
#define LOADCELL_DOUT_PIN 16
#define LOADCELL_SCK_PIN 17

// VBAT measurement pin (ADC input). Pin36 Labelled as VP
#define VBAT_PIN 36
// Voltage divider ratio: actual_voltage = measured_voltage * VBAT_DIVIDER
// e.g. if using two equal resistors, VBAT_DIVIDER = 2.0
// input your actual resistor values here:
#define VBAT_DIVIDER_R1 20000 
#define VBAT_DIVIDER_R2 4700
#define VBAT_DIVIDER ((float)(VBAT_DIVIDER_R1 + VBAT_DIVIDER_R2) / (float)VBAT_DIVIDER_R2)

#define ADC_RESOLUTION 4095.0  // 12-bit ADC

// ESP-NOW Configuration
// A node ID of 0 = parent, 1-4 = children is used at runtime.  Defaults are
// provided by the initial values of the globals in nodeconfig.cpp; preferences
// may override the ID on first boot.  No compile-time macros are required.

// Data transmission interval
#define CHILD_NODE_INTERVAL 1000  // ms between scale readings on child
#define ESPNOW_CHANNEL 6 // WiFi channel for ESP-NOW communication  

#endif
