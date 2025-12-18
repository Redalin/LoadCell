#ifndef ESPNOW_H
#define ESPNOW_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// Message types for ESP-NOW communication
enum ESPNowMsgType {
  MSG_TYPE_WEIGHT = 1,    // Weight data from child
  MSG_TYPE_TARE = 2,      // Tare command from parent
  MSG_TYPE_ACK = 3        // Acknowledgment
};

// Message structure for ESP-NOW communication
typedef struct {
  uint8_t type;           // Message type (MSG_TYPE_*)
  uint8_t id;             // Node ID (1-255)
  float value;            // Weight (for MSG_TYPE_WEIGHT) or scale index (for MSG_TYPE_TARE)
  uint32_t timestamp;     // Timestamp in ms
} ESPNowData;

// Initialize ESP-NOW (parent or child mode based on config)
void espnowInit();

// get the MAC address of the ESP32 board
void readMacAddress();

// Send weight data to parent (child nodes only)
void espnowSendWeight(float weight);

// Send tare command to child node (parent only)
// nodeId: target child node ID, whichScale: 1 or 2 (which scale to tare)
void espnowSendTare(uint8_t nodeId, uint8_t whichScale);

// Get weight data received from a specific child node
// Returns NaN if no data available or node not found
float espnowGetChildWeight(uint8_t childId);

// Check if there's a pending tare command for this node (child only)
// Returns: scale number (1 or 2) if tare needed, 0 if none
uint8_t espnowGetPendingTareCommand();

// Called regularly to handle any pending ESP-NOW tasks
void espnowLoop();

// Print connected peer information (debug)
void espnowPrintPeers();

#endif  // ESPNOW_H

