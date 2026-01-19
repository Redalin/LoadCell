#include "webpage.h"
#include "littlefs-conf.h"
#include "scale.h"
#include "settings.h"
#include "espnow.h"
#include "config.h"
#include "pitbuttons.h"
#include <map>
#include <ArduinoJson.h>


AsyncWebSocket ws("/ws");
AsyncWebServer server(80);
// broadcast interval (ms) - reduced load by increasing interval
static const unsigned long BROADCAST_INTERVAL = 2500;
static unsigned long lastBroadcast = 0;
// Track last sent child weights to only broadcast on data change (delta mode)
static std::map<uint8_t, float> lastSentWeights;


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

  // pitbuttons page
  server.on("/pitbuttons", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/pitbuttons/index.html", "text/html");
  });

  // Serve static files from pitbuttons directory
  server.serveStatic("/pitbuttons/", LittleFS, "/pitbuttons/");

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
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) { request->send(400, "application/json", "{\"error\":\"invalid json\"}"); return; }
    if (!doc["name1"].isNull()) settingsSetName(1, String((const char*)doc["name1"]));
    if (!doc["name2"].isNull()) settingsSetName(2, String((const char*)doc["name2"]));
    if (!doc["color1"].isNull()) settingsSetColor(1, String((const char*)doc["color1"]));
    if (!doc["color2"].isNull()) settingsSetColor(2, String((const char*)doc["color2"]));
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
      request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
  }

  server.serveStatic("/", LittleFS, "/");
  server.begin();

  Serial.println("Init Done. Ready");
}

// Callback helper for building the children JSON object
static JsonDocument *globalDoc = nullptr;
static bool dataChanged = false;

static void addChildToJson(uint8_t childId, float weight) {
  if (globalDoc) {
    JsonObject children = (*globalDoc)["children"];
    children[String(childId)] = weight;
    // Check if this child's weight changed
    auto it = lastSentWeights.find(childId);
    if (it == lastSentWeights.end() || it->second != weight) {
      dataChanged = true;
      lastSentWeights[childId] = weight;
    }
  }
}

// Send current weight to all connected websocket clients as JSON
static void notifyClients(){
  JsonDocument doc;
  
  // If parent node, only include child node data from ESP-NOW
  if (ESPNOW_IS_PARENT) {
    // Include child node data (add up to 5 child nodes). Each child is
    // sent as an object { "weight": <value>, "name": "hostname" }
    // Send children as an array of {id, weight, name} to avoid object key issues
    JsonArray children = doc["children"].to<JsonArray>();
    for (uint8_t i = 1; i <= 5; i++) {
      float childWeight = espnowGetChildWeight(i);
      if (!isnan(childWeight)) {
        JsonObject child = children.add<JsonObject>();
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
  // Serial.print("WS OUT: ");
  // Serial.println(buf);
  ws.textAll(buf, n);
}

void webBroadcastLoop(){
  unsigned long now = millis();
  if (now - lastBroadcast >= BROADCAST_INTERVAL) {
    lastBroadcast = now;
    notifyClients();
  }
}

// WebSocket event handler
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    // send immediate update when client connects
    notifyClients();
  } else if (type == WS_EVT_DATA) {
    // incoming message (assume simple text commands)
    debugln("OnEvent WS_EVT_Data received");
    debugln("Data is: " + String((char*)data));

    handleWebSocketMessage(arg, data, len);
    
    String msg = String((char*)data).substring(0, len);
    debugln("WS Received: " + msg);
    
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
        uint8_t nodeId = remainder.toInt(); // Get nodeId from final value after last colon
        
        Serial.print("Sending tare command to node ");
        Serial.println(nodeId);
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
  JsonDocument doc;
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