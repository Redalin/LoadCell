// add this line including the hash: 
// #include "config.h" 
// config.h
#ifndef CONFIG_H
#define CONFIG_H


#endif

#define DEBUG 1

#if DEBUG
    #define debug(message) Serial.print(message)
    #define debugln(message) Serial.println(message)
#else
    #define debug(message) 
    #define debugln(message)
#endif

// The device hostname
#define HOSTNAME "LaunchScale"
#define APNAME "scale.local"
#define APPASS "wellington"

// Known WiFi networks and passwords
constexpr const char* KNOWN_SSID[] = {"DRW", "ChrisnAimee.com"};
constexpr const char* KNOWN_PASSWORD[] = {"wellington", "carbondell"};
constexpr int KNOWN_SSID_COUNT = sizeof(KNOWN_SSID) / sizeof(KNOWN_SSID[0]);

// Load cell 1 pins and calibration
#define LOADCELL_DOUT_PIN 14
#define LOADCELL_SCK_PIN 13
#define CALIBRATION_FACTOR 2054.7

// Load cell 2 pins and calibration
#define LOADCELL2_DOUT_PIN 27
#define LOADCELL2_SCK_PIN 26
#define LOADCELL2_CALIBRATION_FACTOR 1979.4

// ESP-NOW Configuration
// Set to 1 for parent node (receives data), 0 for child node (sends data)
#define ESPNOW_IS_PARENT 1

// Parent node MAC address (set on parent device)
#define PARENT_MAC_ADDR {0x24, 0x0A, 0xC4, 0x11, 0x11, 0x11}

// Child nodes MAC addresses (for parent to add as peers)
// Update these with your child device MAC addresses
#define CHILD_NODE_INTERVAL 1000  // ms between scale readings on child
#define ESPNOW_CHANNEL 1


