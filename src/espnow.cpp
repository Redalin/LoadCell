#include "espnow.h"
#include <esp_wifi.h>
#include "config.h"
#include <map>

// Map to store child node weights: childId -> weight
static std::map<uint8_t, float> childWeights;
static uint8_t nodeId = 0;  // This device's ID (set on child nodes)
static uint8_t pendingTareCommand = 0;  // Pending tare command (scale number, 0 = none)

// ESP-NOW callbacks
void espnowOnSend(const uint8_t *mac_addr, esp_now_send_status_t status);
void espnowOnRecv(const uint8_t *mac_addr, const uint8_t *data, int len);

void espnowInit() {
  // Initialize WiFi in station mode (required for ESP-NOW)
  WiFi.mode(WIFI_STA);
  
  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // Register send and receive callbacks
  esp_now_register_send_cb(espnowOnSend);
  esp_now_register_recv_cb(espnowOnRecv);
  
  Serial.print("ESP-NOW initialized. Mode: ");
  
  if (ESPNOW_IS_PARENT) {
    Serial.println("PARENT");
    Serial.print("[DEFAULT] ESP32 Board MAC Address: ");
    readMacAddress();
    // Parent node doesn't need to add itself as a peer
    // Child nodes will be added when they pair
  } else {
    Serial.println("CHILD");
    // Generate a simple node ID based on MAC address
    uint8_t mac[6];
    WiFi.macAddress(mac);
    nodeId = mac[5];  // Use last octet of MAC as node ID
    Serial.print("Node ID: ");
    Serial.println(nodeId);
    
    // Child adds parent as a peer
    uint8_t parentMac[] = PARENT_MAC_ADDR;
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, parentMac, 6);
    peerInfo.channel = ESPNOW_CHANNEL;
    peerInfo.encrypt = false;
    
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("Failed to add parent as peer");
      return;
    }
    Serial.println("Parent peer added");
  }
}

void readMacAddress(){
  uint8_t baseMac[6];
  esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
  if (ret == ESP_OK) {
    Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
                  baseMac[0], baseMac[1], baseMac[2],
                  baseMac[3], baseMac[4], baseMac[5]);
  } else {
    Serial.println("Failed to read MAC address");
  }
}

void espnowOnSend(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    // Successful send - nothing special needed
  } else {
    Serial.print("ESP-NOW send failed to: ");
    Serial.print(mac_addr[0], HEX);
    for (int i = 1; i < 6; i++) {
      Serial.print(":");
      Serial.print(mac_addr[i], HEX);
    }
    Serial.println();
  }
}

void espnowOnRecv(const uint8_t *mac_addr, const uint8_t *data, int len) {
  if (len == sizeof(ESPNowData)) {
    ESPNowData *payload = (ESPNowData *)data;
    
    if (ESPNOW_IS_PARENT) {
      // Parent receiving data from child
      if (payload->type == MSG_TYPE_WEIGHT) {
        Serial.print("Received from node ");
        Serial.print(payload->id);
        Serial.print(": ");
        Serial.print(payload->value);
        Serial.println(" g");
        
        // Store the weight data
        childWeights[payload->id] = payload->value;
      } else if (payload->type == MSG_TYPE_ACK) {
        Serial.print("Node ");
        Serial.print(payload->id);
        Serial.println(" acknowledged command");
      }
    } else {
      // Child receiving commands from parent
      if (payload->type == MSG_TYPE_TARE) {
        Serial.print("Received tare command for scale ");
        Serial.println((uint8_t)payload->value);
        
        // Store the pending tare command
        pendingTareCommand = (uint8_t)payload->value;
      }
    }
  }
}

void espnowSendWeight(float weight) {
  if (ESPNOW_IS_PARENT) {
    // Parent doesn't send weight data
    return;
  }
  
  // Child node sends weight to parent
  uint8_t parentMac[] = PARENT_MAC_ADDR;
  ESPNowData data;
  data.type = MSG_TYPE_WEIGHT;
  data.id = nodeId;
  data.value = weight;
  data.timestamp = millis();
  
  esp_err_t result = esp_now_send(parentMac, (uint8_t *)&data, sizeof(data));
  if (result != ESP_OK) {
    Serial.print("Error sending weight data: ");
    Serial.println(result);
  }
}

void espnowSendTare(uint8_t nodeId, uint8_t whichScale) {
  if (!ESPNOW_IS_PARENT) {
    // Only parent sends commands
    return;
  }
  
  // Parent node sends tare command to child
  // For simplicity, send broadcast; in production could look up child MAC
  uint8_t broadcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  ESPNowData data;
  data.type = MSG_TYPE_TARE;
  data.id = nodeId;
  data.value = whichScale;
  data.timestamp = millis();
  
  esp_err_t result = esp_now_send(broadcastMac, (uint8_t *)&data, sizeof(data));
  if (result != ESP_OK) {
    Serial.print("Error sending tare command: ");
    Serial.println(result);
  } else {
    Serial.print("Sent tare command to node ");
    Serial.print(nodeId);
    Serial.print(" for scale ");
    Serial.println(whichScale);
  }
}

float espnowGetChildWeight(uint8_t childId) {
  auto it = childWeights.find(childId);
  if (it != childWeights.end()) {
    return it->second;
  }
  return NAN;
}

uint8_t espnowGetPendingTareCommand() {
  uint8_t cmd = pendingTareCommand;
  pendingTareCommand = 0;  // Clear after reading
  return cmd;
}

void espnowLoop() {
  // ESP-NOW operates via callbacks, so nothing needed here currently
  // Could be used for periodic tasks in the future
}

void espnowPrintPeers() {
  Serial.println("=== ESP-NOW Peer Information ===");
  esp_now_peer_info_t peer;
  for (int i = 0; i < 20; i++) {  // Max 20 peers typically
    if (esp_now_fetch_peer(i, &peer) == ESP_OK) {
      Serial.print("Peer ");
      Serial.print(i);
      Serial.print(": ");
      Serial.print(peer.peer_addr[0], HEX);
      for (int j = 1; j < 6; j++) {
        Serial.print(":");
        Serial.print(peer.peer_addr[j], HEX);
      }
      Serial.print(" | Channel: ");
      Serial.println(peer.channel);
    }
  }
}
