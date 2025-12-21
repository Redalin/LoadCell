#include "espnow.h"
#include <esp_wifi.h>
#include <esp_err.h>
#include "config.h"
#include <map>

// Map to store child node weights: childId -> weight
static std::map<uint8_t, float> childWeights;
static std::map<uint8_t, String> childNames;
static uint8_t nodeId = 0;  // This device's ID (set on child nodes)
static uint8_t pendingTareCommand = 0;  // Pending tare command (scale number, 0 = none)

// Buffering for 500ms average
static const uint32_t ESPNOW_SEND_INTERVAL = 2000;  // 500ms between sends
static unsigned long lastSendTime = 0;
static float weightBuffer[100];  // Buffer for weight samples (enough for ~50 samples at 10ms reading interval)
static int bufferIndex = 0;

void espnowInit() {
  // Initialize WiFi in station mode (required for ESP-NOW)
  WiFi.mode(WIFI_STA);

  if (!ESPNOW_IS_PARENT) {
    // Child nodes can set a static channel if needed
    WiFi.channel(ESPNOW_CHANNEL);
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  }

  // stop wifi from sleeping
  esp_wifi_set_ps(WIFI_PS_NONE);
  
  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // show the MAC address of this device
  Serial.print("[DEFAULT] ESP32 Board MAC Address: ");
  readMacAddress();

  // Register send and receive callbacks
  esp_now_register_send_cb(espnowOnSend);
  esp_now_register_recv_cb(espnowOnRecv);
  
  Serial.print("ESP-NOW initialized. Mode: ");
  
  if (ESPNOW_IS_PARENT) {
    Serial.println("PARENT");
    // Parent node doesn't need to add itself as a peer
    // Child nodes will be added when they pair
    // Add a broadcast peer so parent can send commands to all children
    uint8_t broadcastMac[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastMac, 6);
    peerInfo.channel = ESPNOW_CHANNEL;
    peerInfo.encrypt = false;
    esp_err_t addres = esp_now_add_peer(&peerInfo);
    if (addres != ESP_OK) {
      Serial.print("Warning: failed to add broadcast peer: "); Serial.println(esp_err_to_name(addres));
    } else {
      Serial.println("Broadcast peer added");
    }
  } else {
    Serial.println("CHILD");
    Serial.print("Node ID: ");
    Serial.println(DEVICE_ID);
    Serial.print("Child Wifi Channel: ");
    Serial.println(ESPNOW_CHANNEL);
    
    // Child adds parent as a peer
    uint8_t parentMac[] = PARENT_MAC_ADDR;
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, parentMac, 6);
    peerInfo.channel = ESPNOW_CHANNEL;
    peerInfo.encrypt = false;
    
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("Failed to add parent as peer");
      return;
    } else
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
  // Serial.print("status message is: ");
  // Serial.println(status);

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
        Serial.print(" (");
        Serial.print(payload->name);
        Serial.print("): ");
        Serial.print(payload->value, 1);
        Serial.println(" g");
        
        // Store the weight data
        childWeights[payload->id] = payload->value;
        // store hostname if present
        if (payload->name[0] != '\0') {
          childNames[payload->id] = String(payload->name);
        }
      } else if (payload->type == MSG_TYPE_ACK) {
        Serial.print("Node ");
        Serial.print(payload->id);
        Serial.println(" acknowledged command");
      }
    } else {
      // Child receiving commands from parent
      if (payload->type == MSG_TYPE_TARE) {
        Serial.print("Received tare command for node id ");
        Serial.println((uint8_t)payload->id);
        // Only accept if addressed to this node (or id==0 for broadcast)
        if ((uint8_t)payload->id == DEVICE_ID || (uint8_t)payload->id == 0) {
          // mark pending tare (use 1 to indicate tare request)
          pendingTareCommand = 1;
          Serial.println("Tare queued");
        } else {
          Serial.println("Tare ignored (not for this node)");
        }
      }
    }
  }
}

void espnowSendTare(uint8_t nodeId) {
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
  data.value = 0; // default scale/index
  data.timestamp = millis();
  
  esp_err_t result = esp_now_send(broadcastMac, (uint8_t *)&data, sizeof(data));
  if (result != ESP_OK) {
    Serial.print("Error sending tare command: ");
    Serial.print(result);
    Serial.print(" ("); Serial.print(esp_err_to_name(result)); Serial.println(")");
  } else {
    Serial.print("Sent tare command to node ");
    Serial.print(nodeId);
  }
}

float espnowGetChildWeight(uint8_t childId) {
  auto it = childWeights.find(childId);
  if (it != childWeights.end()) {
    return it->second;
  }
  return NAN;
}

void espnowForEachChildWeight(void (*callback)(uint8_t childId, float weight)) {
  // Iterate through the map and call the callback for each child node
  for (auto& pair : childWeights) {
    if (callback) {
      callback(pair.first, pair.second);
    }
  }
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

void espnowBufferWeight(float weight) {
  if (ESPNOW_IS_PARENT || bufferIndex >= 100) {
    return;  // Parent doesn't buffer, or buffer is full
  }
  
  weightBuffer[bufferIndex++] = weight;
}

void espnowSendAveragedWeightIfReady() {
  if (ESPNOW_IS_PARENT) {
    return;  // Parent doesn't send weight data
  }
  
  unsigned long currentTime = millis();
  
  // Check if interval has elapsed since last send
  if (currentTime - lastSendTime >= ESPNOW_SEND_INTERVAL && bufferIndex > 0) {
    lastSendTime = currentTime;
    
    // Calculate average
    float sum = 0.0f;
    for (int i = 0; i < bufferIndex; i++) {
      sum += weightBuffer[i];
    }
    float average = sum / bufferIndex;
    
    // Reset buffer
    bufferIndex = 0;
    
    // Send the average
    uint8_t parentMac[] = PARENT_MAC_ADDR;
    ESPNowData data;
    data.type = MSG_TYPE_WEIGHT;
    data.id = DEVICE_ID;
    data.value = average;
    data.timestamp = currentTime;
    // include hostname
    memset(data.name, 0, sizeof(data.name));
    const char *hn = HOSTNAME;
    if (hn) strncpy(data.name, hn, sizeof(data.name) - 1);
    
    // Serial.println("Sending averaged weight: ");
    Serial.print("Sending: Node ID ");
    Serial.print(DEVICE_ID);
    Serial.print(" - ");
    Serial.print(data.name);
    Serial.print(": ");
    Serial.print(average, 1); 
    Serial.println(" g");

    esp_err_t result = esp_now_send(parentMac, (uint8_t *)&data, sizeof(data));
    if (result != ESP_OK) {
      Serial.print("Error sending weight data: ");
      Serial.println(result);
    }
  }
}

void espnowSendWeight(float weight) {
  if (ESPNOW_IS_PARENT) {
    return;  // Parent doesn't send weight data
  }
  
  uint8_t parentMac[] = PARENT_MAC_ADDR;
  ESPNowData data;
  data.type = MSG_TYPE_WEIGHT;
  data.id = DEVICE_ID;
  data.value = weight;
  data.timestamp = millis();
  // include hostname
  memset(data.name, 0, sizeof(data.name));
  const char *hn = HOSTNAME;
  if (hn) strncpy(data.name, hn, sizeof(data.name) - 1);
  
  // Serial.println("Sending averaged weight: ");
  Serial.print("Sending: Node ID ");
  Serial.print(DEVICE_ID);
  Serial.print(" - ");
  Serial.print(data.name);
  Serial.print(": ");
  Serial.print(weight, 1); 
  Serial.println(" g");

  esp_err_t result = esp_now_send(parentMac, (uint8_t *)&data, sizeof(data));
  if (result != ESP_OK) {
    Serial.print("Error sending weight data: ");
    Serial.println(result);
  }
}




const char* espnowGetChildName(uint8_t childId) {
  auto it = childNames.find(childId);
  if (it != childNames.end()) return it->second.c_str();
  return "";
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
