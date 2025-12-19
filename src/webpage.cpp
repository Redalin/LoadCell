#include "webpage.h"
#include "littlefs-conf.h"
#include "scale.h"
#include "settings.h"
#include "espnow.h"
#include "config.h"
#include <ArduinoJson.h>


AsyncWebSocket ws("/ws");
AsyncWebServer server(80);
// broadcast interval (ms)
static const unsigned long BROADCAST_INTERVAL = 1000;
static unsigned long lastBroadcast = 0;


void initwebservers(){ 
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  Serial.println("Starting Web Server");
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html");
  });
  server.on("/favicon.png", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/favicon.png", "image/png");
  });

  // settings endpoints
  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request){
    String s = settingsAsJson();
    request->send(200, "application/json", s);
  });
  server.on("/settings/reset", HTTP_POST, [](AsyncWebServerRequest *request){
    settingsResetDefaults();
    String s = settingsAsJson();
    request->send(200, "application/json", s);
  });
  // POST /settings with JSON body
  server.on("/settings", HTTP_POST, [](AsyncWebServerRequest *request){
    // ack will be sent from body handler
  }, nullptr, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    // collect body (may be called multiple times)
    String body;
    body.reserve(total);
    body = String((const char*)data, len);
    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) { request->send(400, "application/json", "{\"error\":\"invalid json\"}"); return; }
    if (doc.containsKey("name1")) settingsSetName(1, String((const char*)doc["name1"]));
    if (doc.containsKey("name2")) settingsSetName(2, String((const char*)doc["name2"]));
    if (doc.containsKey("color1")) settingsSetColor(1, String((const char*)doc["color1"]));
    if (doc.containsKey("color2")) settingsSetColor(2, String((const char*)doc["color2"]));
    bool ok = settingsSave();
    if (ok) request->send(200, "application/json", "{\"status\":\"ok\"}"); else request->send(500, "application/json", "{\"error\":\"save failed\"}");
  });

  // provide a simple HTTP endpoint to tare the scale (child nodes only)
  if (!ESPNOW_IS_PARENT) {
    server.on("/tare", HTTP_POST, [](AsyncWebServerRequest *request){
      int which = 0;
      if (request->hasArg("scale")) {
        which = request->arg("scale").toInt();
      }
      else scaleTareAll();
      request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
  }

  server.serveStatic("/", LittleFS, "/");
  server.begin();

  Serial.println("Init Done. Ready");
}

// Send current weight to all connected websocket clients as JSON
static void notifyClients(){
  StaticJsonDocument<1024> doc;
  
  // If parent node, only include child node data from ESP-NOW
  if (ESPNOW_IS_PARENT) {
    // Include child node data (add up to 10 child nodes). Each child is
    // sent as an object { "weight": <value>, "name": "hostname" }
    // Send children as an array of {id, weight, name} to avoid object key issues
    JsonArray children = doc.createNestedArray("children");
    for (uint8_t i = 1; i <= 10; i++) {
      float childWeight = espnowGetChildWeight(i);
      if (!isnan(childWeight)) {
        JsonObject child = children.createNestedObject();
        child["id"] = i;
        child["weight"] = childWeight;
        const char* hn = espnowGetChildName(i);
        if (hn && hn[0] != '\0') child["name"] = hn;
      }
    }
    doc["mode"] = "parent";
  } else {
    // If child node, just send its own scale data
    float weight = scaleRead();
    if (!isnan(weight)) doc["weight"] = weight; else doc["weight"] = nullptr;
    doc["mode"] = "child";
  }
  
  char buf[1024];
  size_t n = serializeJson(doc, buf, sizeof(buf));
  // log outgoing JSON for debugging (will appear on Serial)
  Serial.print("WS OUT: ");
  Serial.println(buf);
  ws.textAll(buf, n);
}

void webBroadcastLoop(){
  unsigned long now = millis();
  if (now - lastBroadcast >= BROADCAST_INTERVAL) {
    lastBroadcast = now;
    notifyClients();
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    // send immediate update when client connects
    notifyClients();
  } else if (type == WS_EVT_DATA) {
    // incoming message (assume simple text commands)
    String msg = String((char*)data).substring(0, len);
    Serial.println("WS Received: " + msg);
    
    if (ESPNOW_IS_PARENT) {
      // Parent node: convert simple tare commands to child node commands
      // tare -> tare all children (not implemented, ignore)
      // tare:X -> send to child node X, scale 1
      // tare:child:nodeId -> send to child node
      // tare:child:nodeId:scale -> send to child node with specific scale
      
      if (msg == "tare") {
        // Tare all - not implemented for parent mode (parent has no local scales)
        Serial.println("Tare all ignored (parent has no local scales)");
        notifyClients();
      } else if (msg.startsWith("tare:child:")) {
        // Parse: "tare:child:nodeId" or "tare:child:nodeId:scale"
        String remainder = msg.substring(11);  // Skip "tare:child:"
        int colonIdx = remainder.indexOf(':');
        uint8_t nodeId, scale = 1;
        
        if (colonIdx > 0) {
          nodeId = remainder.substring(0, colonIdx).toInt();
          scale = remainder.substring(colonIdx + 1).toInt();
        } else {
          nodeId = remainder.toInt();
        }
        
        Serial.print("Sending tare command to node ");
        Serial.print(nodeId);
        Serial.print(" scale ");
        Serial.println(scale);
        espnowSendTare(nodeId);
        notifyClients();
      } else if (msg.startsWith("tare:")) {
        // Simple format: tare:nodeId -> send to child node with scale 1
        uint8_t nodeId = msg.substring(5).toInt();
        Serial.print("Sending tare command to child node ");
        Serial.print(nodeId);
        espnowSendTare(nodeId);
        notifyClients();
      }
    } else {
      // Child node: only handle local tare commands
      if (msg == "tare") {
        scaleTare();  // Tare the scale
        notifyClients();
      } else if (msg.startsWith("tare:")) {
        int which = msg.substring(msg.indexOf(':') + 1).toInt();
        scaleTare();
        notifyClients();
      }
    }
  }
}

// send calibration result to clients (declared in header)
void sendCalibrationResult(int which, float value, uint32_t clientId) {
  StaticJsonDocument<128> doc;
  if (!isnan(value)) doc["calibrate"] = value; else doc["calibrate"] = nullptr;
  doc["scale"] = which;
  char buf[128]; size_t n = serializeJson(doc, buf);
  // if clientId provided, send to that client only when available
  if (clientId != 0) {
    // try to find client and send; fall back to broadcast
    AsyncWebSocketClient *c = ws.client(clientId);
    if (c) { c->text(buf, n); return; }
  }
  ws.textAll(buf, n);
}