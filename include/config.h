// add this line including the hash: 
// #include "config.h" 
// config.h
#ifndef CONFIG_H
#define CONFIG_H


#endif

#define DEBUG 0

#if DEBUG
    #define debug(message) Serial.print(message)
    #define debugln(message) Serial.println(message)
#else
    #define debug(message) 
    #define debugln(message)
#endif

// Known WiFi networks and passwords
constexpr const char* KNOWN_SSID[] = {"DRW", "DRW.yellow", "ChrisnAimee.com"};
constexpr const char* KNOWN_PASSWORD[] = {"wellington", "wellington", "carbondell"};
constexpr int KNOWN_SSID_COUNT = sizeof(KNOWN_SSID) / sizeof(KNOWN_SSID[0]);

// Access Point credentials (if no known WiFi found)
#define APNAME "DRW LaunchScale"
#define APPASS "wellington"


// Load cell pins and calibration
#define LOADCELL_DOUT_PIN 16
#define LOADCELL_SCK_PIN 17

// VBAT measurement pin (ADC input). Pin36 Labelled as VP
#define VBAT_PIN 36
// Voltage divider ratio: actual_voltage = measured_voltage * VBAT_DIVIDER
// e.g. if using two equal resistors, VBAT_DIVIDER = 2.0
#define VBAT_DIVIDER_R1 20000
#define VBAT_DIVIDER_R2 4700
#define VBAT_DIVIDER ((float)(VBAT_DIVIDER_R1 + VBAT_DIVIDER_R2) / (float)VBAT_DIVIDER_R2)

#define ADC_RESOLUTION 4095.0  // 12-bit ADC

// ESP-NOW Configuration
// Set to 0 for parent node (receives data), 1-4 for child nodes (sends data)
#define DEVICE_ID 0

#if DEVICE_ID == 0  // Parent node ID
  #define ESPNOW_IS_PARENT 1
  #define HOSTNAME "LaunchScale"
  #define CALIBRATION_FACTOR 0

#elif DEVICE_ID == 1 // First Child node ID
  #define ESPNOW_IS_PARENT 0
  #define HOSTNAME "Yellow"
  #define CALIBRATION_FACTOR 2128.66
#elif DEVICE_ID == 2 // Second Child node ID
  #define ESPNOW_IS_PARENT 0
  #define HOSTNAME "Grey"
  #define CALIBRATION_FACTOR 1979.4
#elif DEVICE_ID == 3 // Third Child node ID
  #define ESPNOW_IS_PARENT 0
  #define HOSTNAME "Purple"
  #define CALIBRATION_FACTOR 2000
#elif DEVICE_ID == 4 // Fourth Child node ID
  #define ESPNOW_IS_PARENT 0
  #define HOSTNAME "Black"
  #define CALIBRATION_FACTOR 1106.69
#else 
    #error "Invalid DEVICE_ID specified."
#endif

// Tare button pin
#ifdef ESPNOW_IS_PARENT
  #define TARE_BUTTON_PIN 14 // normal scale pin 15. Parent 14 because broken
#else
  #define TARE_BUTTON_PIN 15 // normal scale pin 15. Parent 17 because broken
#endif

// Parent node MAC address (set on parent device)
#define PARENT_MAC_ADDR  {0xa0, 0xdd, 0x6c, 0x02, 0xfb, 0xa4}

// Child nodes MAC addresses (for parent to add as peers)
// Update these with your child device MAC addresses
#define CHILD_NODE_INTERVAL 1000  // ms between scale readings on child
#define ESPNOW_CHANNEL 6

// Board MAC address
// Parent: a0:dd:6c:02:fb:a4
// Child Grey: a0:dd:6c:04:47:a8
// Child Yellow: a0:dd:6c:04:47:ac
// Child Purple: a0:dd:6c:04:47:ad
// Child Black: a0:dd:6c:04:47:ae